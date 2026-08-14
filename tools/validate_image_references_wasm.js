#!/usr/bin/env node
"use strict";

const crypto = require("node:crypto");
const fs = require("node:fs");
const path = require("node:path");

const root = path.resolve(__dirname, "..");
const imagesRoot = path.join(root, "images");
const encodingsPath = path.join(imagesRoot, "recipes", "encodings.json");
const formatContract = {
    i400: { channelCount: 1, chromaFormatIdc: 1 },
    i420: { channelCount: 3, chromaFormatIdc: 0 },
    i444: { channelCount: 3, chromaFormatIdc: 2 },
};

function fail(message) {
    throw new Error(message);
}

function sha256(bytes) {
    return crypto.createHash("sha256").update(bytes).digest("hex");
}

function loadSuites(selectedSuite) {
    const encodings = JSON.parse(fs.readFileSync(encodingsPath, "utf8"));
    const suites = selectedSuite === undefined
        ? encodings.suites
        : encodings.suites.filter((suite) => suite.id === selectedSuite);
    if (suites.length === 0) fail(`unknown encoding suite: ${selectedSuite}`);
    return suites.map((suite) => {
        const lockPath = path.join(imagesRoot, "recipes", suite.lock_file);
        return {
            id: suite.id,
            records: JSON.parse(fs.readFileSync(lockPath, "utf8")).references,
        };
    });
}

function decodeRecord(exports, record) {
    const contract = formatContract[record.pixel_format];
    if (contract === undefined) {
        fail(`${record.case_id}: unsupported pixel format ${record.pixel_format}`);
    }
    const manifestPath = path.join(imagesRoot, record.manifest);
    const source = path.join(
        path.dirname(manifestPath),
        record.outputs.encoded_obu.filename
    );
    const encoded = fs.readFileSync(source);
    if (
        encoded.length !== record.outputs.encoded_obu.byte_size ||
        sha256(encoded) !== record.outputs.encoded_obu.sha256
    ) {
        fail(`${record.case_id}: retained OBU differs from reference lock`);
    }

    exports.av2h_reset();
    const inputPointer = exports.av2h_alloc(encoded.length);
    if (inputPointer === 0) fail(`${record.case_id}: input allocation failed`);
    new Uint8Array(exports.memory.buffer, inputPointer, encoded.length).set(encoded);
    const status = exports.av2h_decode(inputPointer, encoded.length);
    const result = new DataView(
        exports.memory.buffer,
        exports.av2h_result_pointer(),
        48
    );
    const values = Array.from(
        { length: 12 },
        (_, index) => result.getUint32(index * 4, true)
    );
    const [
        outputPointer,
        outputSize,
        width,
        height,
        resultStatus,
        cause,
        stage,
        channelCount,
        chromaFormatIdc,
        bitDepth,
        bytesPerSample,
        byteOrder,
    ] = values;
    if (status !== 0 || resultStatus !== 0) {
        fail(
            `${record.case_id}: status=${status}/${resultStatus} ` +
                `cause=${cause} stage=${stage}`
        );
    }
    const [expectedWidth, expectedHeight] = record.dimensions;
    const expected = record.outputs.decoded;
    if (
        width !== expectedWidth ||
        height !== expectedHeight ||
        channelCount !== contract.channelCount ||
        chromaFormatIdc !== contract.chromaFormatIdc ||
        bitDepth !== 8 ||
        bytesPerSample !== 1 ||
        byteOrder !== 0 ||
        outputSize !== expected.byte_size
    ) {
        fail(
            `${record.case_id}: result ${width}x${height}, ` +
                `channels=${channelCount}, chroma=${chromaFormatIdc}, ` +
                `depth=${bitDepth}, sample-bytes=${bytesPerSample}, ` +
                `byte-order=${byteOrder}, output-bytes=${outputSize}`
        );
    }
    const decoded = new Uint8Array(
        exports.memory.buffer,
        outputPointer,
        outputSize
    );
    const digest = sha256(decoded);
    if (digest !== expected.sha256) {
        fail(
            `${record.case_id}: decoded SHA-256 ${digest}, ` +
                `expected ${expected.sha256}`
        );
    }
}

async function main() {
    if (process.argv.length > 4) {
        fail("usage: validate_image_references_wasm.js [WASM] [SUITE]");
    }
    const wasmPath = path.resolve(process.argv[2] || path.join(root, "docs", "decoder.wasm"));
    const suites = loadSuites(process.argv[3]);
    const { instance } = await WebAssembly.instantiate(
        fs.readFileSync(wasmPath),
        {}
    );
    const exports = instance.exports;
    for (const name of [
        "memory",
        "av2h_reset",
        "av2h_alloc",
        "av2h_decode",
        "av2h_result_pointer",
    ]) {
        if (!(name in exports)) fail(`missing WebAssembly export: ${name}`);
    }
    let total = 0;
    for (const suite of suites) {
        for (const record of suite.records) decodeRecord(exports, record);
        total += suite.records.length;
        console.log(`${suite.id}: ${suite.records.length} WebAssembly records exact`);
    }
    if (suites.length > 1) {
        console.log(`all suites: ${total} WebAssembly records exact`);
    }
}

main().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
});
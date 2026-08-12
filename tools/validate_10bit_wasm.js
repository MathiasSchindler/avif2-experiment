#!/usr/bin/env node
"use strict";

const crypto = require("node:crypto");
const fs = require("node:fs");
const path = require("node:path");

const root = path.resolve(__dirname, "..");
const defaultWasm = path.join(root, "build/wasm-wp10/decoder.wasm");
const manifests = [
    path.join(root, "tests/fixtures/10bit/avm-reference/manifest.json"),
    path.join(root, "images/artifacts/10bit/avm-reference/manifest.json"),
];
const formats = {
    yuv420p10le: {
        chromaFormatIdc: 0,
        expectedCounts: [25, 18],
        label: "4:2:0",
    },
    yuv444p10le: {
        chromaFormatIdc: 2,
        expectedCounts: [21, 18],
        label: "4:4:4",
    },
};

function loadRecords(pixelFormat) {
    const format = formats[pixelFormat];
    return manifests.flatMap((manifestPath, index) => {
        const document = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
        const records = document.artifacts.filter(
            (record) => record.pixel_format === pixelFormat
        );
        if (records.length !== format.expectedCounts[index]) {
            fail(
                `${path.relative(root, manifestPath)}: expected ` +
                    `${format.expectedCounts[index]} records, ` +
                    `found ${records.length}`
            );
        }
        return records.map((record) => ({
            ...record,
            directory: path.dirname(manifestPath),
        }));
    });
}

function expectedSize(record, chromaFormatIdc) {
    if (chromaFormatIdc === 2) {
        return 6 * record.width * record.height;
    }
    const chromaWidth = Math.floor((record.width + 1) / 2);
    const chromaHeight = Math.floor((record.height + 1) / 2);
    return 2 * (
        record.width * record.height + 2 * chromaWidth * chromaHeight
    );
}

function sha256(bytes) {
    return crypto.createHash("sha256").update(bytes).digest("hex");
}

function decodeRecord(exports, record, format) {
    const inputPath = path.join(record.directory, record.obu.file);
    const encoded = fs.readFileSync(inputPath);
    if (
        encoded.length !== record.obu.size_bytes ||
        sha256(encoded) !== record.obu.sha256
    ) {
        fail(`${record.name}: retained OBU differs from manifest`);
    }

    exports.av2h_reset();
    const inputPointer = exports.av2h_alloc(encoded.length);
    if (inputPointer === 0) fail(`${record.name}: input allocation failed`);
    new Uint8Array(exports.memory.buffer, inputPointer, encoded.length).set(
        encoded
    );
    const status = exports.av2h_decode(inputPointer, encoded.length);
    const resultPointer = exports.av2h_result_pointer();
    const result = new DataView(exports.memory.buffer, resultPointer, 48);
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
            `${record.name}: status=${status}/${resultStatus} ` +
                `cause=${cause} stage=${stage}`
        );
    }
    const manifestSize = expectedSize(record, format.chromaFormatIdc);
    if (
        width !== record.width ||
        height !== record.height ||
        channelCount !== 3 ||
        chromaFormatIdc !== format.chromaFormatIdc ||
        bitDepth !== 10 ||
        bytesPerSample !== 2 ||
        byteOrder !== 1 ||
        outputSize !== manifestSize ||
        outputSize !== record.decoded.size_bytes
    ) {
        fail(
            `${record.name}: result ${width}x${height}, ` +
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
    if (digest !== record.decoded.sha256) {
        fail(
            `${record.name}: SHA-256 ${digest}, ` +
                `expected ${record.decoded.sha256}`
        );
    }
}

function fail(message) {
    throw new Error(message);
}

async function main() {
    if (process.argv.length > 4) {
        fail("usage: validate_10bit_wasm.js [WASM] [PIXEL_FORMAT]");
    }
    const wasmPath = path.resolve(process.argv[2] || defaultWasm);
    const pixelFormat = process.argv[3] || "yuv420p10le";
    const format = formats[pixelFormat];
    if (format === undefined) {
        fail(`unsupported pixel format: ${pixelFormat}`);
    }
    const records = loadRecords(pixelFormat);
    const { instance } = await WebAssembly.instantiate(
        fs.readFileSync(wasmPath), {}
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
    for (const record of records) decodeRecord(exports, record, format);
    console.log(
        `10-bit ${format.label} WASM corpus: ${records.length} records exact`
    );
}

main().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
});
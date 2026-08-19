#!/usr/bin/env node
"use strict";

const crypto = require("node:crypto");
const fs = require("node:fs");
const path = require("node:path");

const root = path.resolve(__dirname, "..");

function fail(message) {
    throw new Error(message);
}

function expectedGeometry(record) {
    const width = record.width;
    const height = record.height;
    if (record.pixel_format === "i400") {
        return [1, 1, width * height];
    }
    if (record.pixel_format === "i420") {
        const size = width * height +
            2 * Math.ceil(width / 2) * Math.ceil(height / 2);
        return [3, 0, size];
    }
    if (record.pixel_format === "i444") {
        return [3, 2, width * height * 3];
    }
    fail(`${record.id}: unsupported pixel format ${record.pixel_format}`);
}

async function main() {
    if (process.argv.length < 3 || process.argv.length > 4) {
        fail("usage: validate_spatial_still_wasm.js WASM [MANIFEST]");
    }
    const wasmPath = path.resolve(process.argv[2]);
    const manifestPath = process.argv[3]
        ? path.resolve(process.argv[3])
        : path.join(root, "tests", "spatial-still-profile.json");
    const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
    if (manifest.schema !== "av2h-spatial-still-profile-v1") {
        fail("unsupported spatial-still profile manifest schema");
    }
    const moduleBytes = fs.readFileSync(wasmPath);
    const { instance } = await WebAssembly.instantiate(moduleBytes, {});
    const exports = instance.exports;
    for (const name of [
        "memory", "av2h_reset", "av2h_alloc", "av2h_decode",
        "av2h_result_pointer",
    ]) {
        if (!(name in exports)) fail(`missing WebAssembly export: ${name}`);
    }

    for (const record of manifest.cases || []) {
        const source = path.join(root, record.path);
        const encoded = fs.readFileSync(source);
        const encodedDigest = crypto.createHash("sha256")
            .update(encoded).digest("hex");
        if (encodedDigest !== record.encoded_sha256) {
            fail(`${record.id}: encoded SHA-256 ${encodedDigest}`);
        }
        exports.av2h_reset();
        const inputPointer = exports.av2h_alloc(encoded.length);
        if (inputPointer === 0) {
            fail(`${record.id}: WebAssembly input allocation failed`);
        }
        new Uint8Array(exports.memory.buffer, inputPointer, encoded.length)
            .set(encoded);
        const status = exports.av2h_decode(inputPointer, encoded.length);
        const resultPointer = exports.av2h_result_pointer();
        const result = new DataView(exports.memory.buffer, resultPointer, 36);
        const values = Array.from(
            { length: 9 },
            (_, index) => result.getUint32(index * 4, true)
        );
        const [
            outputPointer, outputSize, width, height, resultStatus,
            cause, stage, channelCount, chromaFormatIdc,
        ] = values;
        if (status !== 0 || resultStatus !== 0) {
            fail(
                `${record.id}: status=${status}/${resultStatus} ` +
                `cause=${cause} stage=${stage}`
            );
        }
        const [expectedChannels, expectedChroma, expectedSize] =
            expectedGeometry(record);
        if (
            width !== record.width || height !== record.height ||
            channelCount !== expectedChannels ||
            chromaFormatIdc !== expectedChroma || outputSize !== expectedSize
        ) {
            fail(
                `${record.id}: result geometry ${width}x${height}, ` +
                `channels=${channelCount}, chroma=${chromaFormatIdc}, ` +
                `bytes=${outputSize}`
            );
        }
        const decoded = new Uint8Array(
            exports.memory.buffer, outputPointer, outputSize
        );
        const digest = crypto.createHash("sha256").update(decoded).digest("hex");
        if (digest !== record.decoded_sha256) {
            fail(`${record.id}: decoded SHA-256 ${digest}`);
        }
        console.log(`${record.id}: ${outputSize} bytes exact`);
    }
    console.log(`spatial-still WebAssembly: ${manifest.cases.length} cases exact`);
}

main().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
});

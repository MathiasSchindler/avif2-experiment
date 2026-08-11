#!/usr/bin/env node
"use strict";

const crypto = require("node:crypto");
const fs = require("node:fs");
const path = require("node:path");

function fail(message) {
    throw new Error(message);
}

async function main() {
    if (process.argv.length !== 4) {
        fail("usage: validate_8bit_444_wasm_node.js WASM MANIFEST");
    }
    const wasmPath = path.resolve(process.argv[2]);
    const manifestPath = path.resolve(process.argv[3]);
    const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
    const records = manifest.artifacts || [];
    if (records.length !== 12) {
        fail(`expected 12 artifacts, found ${records.length}`);
    }

    const moduleBytes = fs.readFileSync(wasmPath);
    const { instance } = await WebAssembly.instantiate(moduleBytes, {});
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

    for (const record of records) {
        const source = path.join(path.dirname(manifestPath), record.file);
        const encoded = fs.readFileSync(source);
        exports.av2h_reset();
        const inputPointer = exports.av2h_alloc(encoded.length);
        if (inputPointer === 0) {
            fail(`${record.name}: WebAssembly input allocation failed`);
        }
        new Uint8Array(
            exports.memory.buffer,
            inputPointer,
            encoded.length
        ).set(encoded);
        const status = exports.av2h_decode(inputPointer, encoded.length);
        const resultPointer = exports.av2h_result_pointer();
        const result = new DataView(exports.memory.buffer, resultPointer, 36);
        const values = Array.from(
            { length: 9 },
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
        ] = values;
        if (status !== 0 || resultStatus !== 0) {
            fail(
                `${record.name}: status=${status}/${resultStatus} ` +
                `cause=${cause} stage=${stage}`
            );
        }
        const expectedSize = record.width * record.height * 3;
        if (
            width !== record.width ||
            height !== record.height ||
            channelCount !== 3 ||
            chromaFormatIdc !== 2 ||
            outputSize !== expectedSize
        ) {
            fail(
                `${record.name}: result geometry ${width}x${height}, ` +
                `channels=${channelCount}, chroma=${chromaFormatIdc}, ` +
                `bytes=${outputSize}`
            );
        }
        const decoded = new Uint8Array(
            exports.memory.buffer,
            outputPointer,
            outputSize
        );
        const digest = crypto.createHash("sha256").update(decoded).digest("hex");
        if (digest !== record.decoded_i444_sha256) {
            fail(
                `${record.name}: SHA-256 ${digest}, expected ` +
                record.decoded_i444_sha256
            );
        }
        console.log(`${record.name}: ${outputSize} bytes exact`);
    }
}

main().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
});

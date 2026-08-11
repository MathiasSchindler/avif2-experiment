#!/usr/bin/env node
"use strict";

const crypto = require("node:crypto");
const fs = require("node:fs");
const path = require("node:path");

const root = path.resolve(__dirname, "..");
const defaultWasm = path.join(root, "build/wasm-wp10/decoder.wasm");
const inputPath = path.join(
    root,
    "tests/fixtures/10bit/avm-reference/" +
        "tiny-constrained-16x16-yuv420p10le-qp208.obu"
);
const expectedSha256 =
    "ca9ec6d9f6a901431de7364968b9b84ebf8ed34a832b167b66661add370f302b";

function fail(message) {
    throw new Error(message);
}

async function main() {
    if (process.argv.length > 3) {
        fail("usage: validate_10bit_wasm.js [WASM]");
    }
    const wasmPath = path.resolve(process.argv[2] || defaultWasm);
    const encoded = fs.readFileSync(inputPath);
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

    exports.av2h_reset();
    const inputPointer = exports.av2h_alloc(encoded.length);
    if (inputPointer === 0) fail("WebAssembly input allocation failed");
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
            `status=${status}/${resultStatus} cause=${cause} stage=${stage}`
        );
    }
    if (
        width !== 16 ||
        height !== 16 ||
        channelCount !== 3 ||
        chromaFormatIdc !== 0 ||
        bitDepth !== 10 ||
        bytesPerSample !== 2 ||
        byteOrder !== 1 ||
        outputSize !== 768
    ) {
        fail(
            `result ${width}x${height}, channels=${channelCount}, ` +
                `chroma=${chromaFormatIdc}, depth=${bitDepth}, ` +
                `sample-bytes=${bytesPerSample}, byte-order=${byteOrder}, ` +
                `output-bytes=${outputSize}`
        );
    }
    const decoded = new Uint8Array(
        exports.memory.buffer,
        outputPointer,
        outputSize
    );
    const digest = crypto.createHash("sha256").update(decoded).digest("hex");
    if (digest !== expectedSha256) {
        fail(`SHA-256 ${digest}, expected ${expectedSha256}`);
    }
    console.log("10-bit WASM: 768 bytes and metadata exact");
}

main().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
});
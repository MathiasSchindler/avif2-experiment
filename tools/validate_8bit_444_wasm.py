#!/usr/bin/env python3
"""Validate the production 8-bit I444 corpus through decoder.wasm."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

try:
    import wasmtime
except ImportError as error:
    raise SystemExit("install the development dependency with: python -m pip install wasmtime") from error


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "images" / "artifacts-8bit-444" / "manifest.json"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wasm", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    args = parser.parse_args()

    manifest_path = args.manifest.resolve()
    records = json.loads(manifest_path.read_text(encoding="utf-8")).get("artifacts", [])
    if len(records) != 12:
        raise RuntimeError(f"expected 12 artifacts, found {len(records)}")

    engine = wasmtime.Engine()
    module = wasmtime.Module.from_file(engine, str(args.wasm.resolve()))
    store = wasmtime.Store(engine)
    instance = wasmtime.Instance(store, module, [])
    exports = instance.exports(store)
    memory = exports["memory"]
    reset = exports["av2h_reset"]
    allocate = exports["av2h_alloc"]
    decode = exports["av2h_decode"]
    result_pointer = exports["av2h_result_pointer"]

    for record in records:
        source = manifest_path.parent / record["file"]
        encoded = source.read_bytes()
        reset(store)
        input_pointer = allocate(store, len(encoded))
        if input_pointer == 0:
            raise RuntimeError(f"{record['name']}: WebAssembly input allocation failed")
        memory.write(store, encoded, input_pointer)
        status = decode(store, input_pointer, len(encoded))
        result_offset = result_pointer(store)
        result_bytes = bytes(memory.read(store, result_offset, result_offset + 36))
        (
            output_pointer,
            output_size,
            width,
            height,
            result_status,
            cause,
            stage,
            channel_count,
            chroma_format_idc,
        ) = struct.unpack("<9I", result_bytes)
        if status != 0 or result_status != 0:
            raise RuntimeError(
                f"{record['name']}: status={status}/{result_status} "
                f"cause={cause} stage={stage}"
            )
        expected_size = record["width"] * record["height"] * 3
        if (width, height, channel_count, chroma_format_idc, output_size) != (
            record["width"],
            record["height"],
            3,
            2,
            expected_size,
        ):
            raise RuntimeError(
                f"{record['name']}: result geometry "
                f"{width}x{height}, channels={channel_count}, "
                f"chroma={chroma_format_idc}, bytes={output_size}"
            )
        decoded = bytes(memory.read(store, output_pointer, output_pointer + output_size))
        digest = hashlib.sha256(decoded).hexdigest()
        if digest != record["decoded_i444_sha256"]:
            raise RuntimeError(
                f"{record['name']}: SHA-256 {digest}, expected "
                f"{record['decoded_i444_sha256']}"
            )
        print(f"{record['name']}: {output_size} bytes exact")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

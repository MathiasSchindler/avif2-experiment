#!/usr/bin/env python3
"""Model and probe the M3 8x8 boundary partition context transcript."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
TOOLS = Path(__file__).resolve().parent
ENCODER = ROOT / "experimental/encoder"
NORMATIVE = ENCODER / "src/normative_cdfs.inc"
DRIVER = ENCODER / "build/m3-boundary-partition-driver"
AVMDEC = ROOT / "build/reference/avm/avmdec"
PROJECT_DECODER = ROOT / "build/host-cmake/av2heif-decoder"
CLASSES = (24, 32, 40, 48, 56, 64)
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))


def _load(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, TOOLS / filename)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


PARTITIONS = _load("m3_boundary_partitions", "m3_large_partition_model.py")
MULTILEAF = _load("m3_boundary_multileaf", "m3_multileaf_cdf_model.py")

SYMBOL_COUNTS = {
    "do_split": 2,
    "rect_type": 2,
    "do_ext_partition": 2,
    "y_mode_set": 4,
    "y_mode_index": 8,
    "fsc_mode": 2,
    **MULTILEAF.TABLE_SYMBOLS,
}


def parse_tables() -> dict[str, list[list[int]]]:
    text = NORMATIVE.read_text()
    result = {}
    for name in SYMBOL_COUNTS:
        match = re.search(
            rf"static const uint16_t enc_norm_{name}\[[^;]+?= \{{(.*?)\n\}};",
            text,
            re.DOTALL,
        )
        if match is None:
            raise ValueError(f"missing normative table {name}")
        result[name] = [
            [int(value) for value in re.findall(r"(\d+)U", row)]
            for row in re.findall(r"\{([^{}]+)\}", match.group(1))
        ]
    return result


class RangeWriter:
    def __init__(self) -> None:
        self.output = bytearray()
        self.symbols_out = 0
        self.low = 0
        self.range = 32768
        self.count = -9
        self.pending_byte = -1
        self.pending_ff = 0

    def tell(self) -> int:
        return self.symbols_out * 8 + self.count + 10

    def _carry(self, value: int) -> None:
        self.symbols_out += 1
        if value == 0xFF:
            self.pending_ff += 1
            return
        carry = value >> 8
        if self.pending_byte >= 0:
            self.output.append((self.pending_byte + carry) & 0xFF)
        self.output.extend(bytes([(0xFF + carry) & 0xFF]) * self.pending_ff)
        self.pending_ff = 0
        self.pending_byte = value & 0xFF

    def _normalize(self, low: int, width: int) -> None:
        shift = 15 - (width.bit_length() - 1)
        count = self.count
        available = count + shift
        if available >= 0:
            count += 16
            mask = (1 << count) - 1
            if available >= 8:
                self._carry(low >> count)
                low &= mask
                count -= 8
                mask >>= 8
            self._carry(low >> count)
            available = count + shift - 24
            low &= mask
        self.low = low << shift
        self.range = width << shift
        self.count = available

    @staticmethod
    def _scale(probability: int, width: int, symbol: int, count: int) -> int:
        reduced = ((probability >> 7) << 4)
        reduced += 16 * (count - symbol - 1) // count
        return (((width >> 8) * reduced) >> 7) << 3

    def symbol(self, symbol: int, cdf: list[int], count: int) -> None:
        upper = self.range if symbol == 0 else self._scale(
            cdf[symbol - 1], self.range, symbol - 1, count
        )
        lower = self._scale(cdf[symbol], self.range, symbol, count)
        self._normalize(self.low + self.range - upper, upper - lower)

    def finish(self) -> bytes:
        tell = self.tell()
        trailing_position = tell - 1
        trailing_byte = trailing_position >> 3
        trailing_offset = trailing_position & 7
        low = self.low
        width = self.range
        count = self.count
        bits = 9
        mask = 0x7FFF
        end = (low + mask) & ~mask
        while (end | mask) >= low + width:
            bits += 1
            mask >>= 1
            end = (low + mask) & ~mask
        bits += count
        if bits > 0:
            remainder_mask = (1 << (count + 16)) - 1
            while True:
                self._carry(end >> (count + 16))
                end &= remainder_mask
                bits -= 8
                count -= 8
                remainder_mask >>= 8
                if bits <= 0:
                    break
        if self.pending_byte >= 0 or self.pending_ff:
            self._carry(0)
        while len(self.output) <= trailing_byte:
            self.output.append(0)
        self.output[trailing_byte] &= (0xFF << (7 - trailing_offset)) & 0xFF
        self.output[trailing_byte] |= 1 << (7 - trailing_offset)
        return bytes(self.output)


def extension_allowed(node: str, axis: str) -> bool:
    width, height = (int(value) for value in node.split("x"))
    if width <= 8 and height <= 8:
        return False
    return not (
        (width, height, axis) in {
            (16, 8, "HORZ"), (8, 16, "VERT"),
            (32, 8, "HORZ"), (8, 32, "VERT"),
            (64, 16, "HORZ"), (16, 64, "VERT"),
        }
    )


def event_spec(width: int, height: int, buggy: bool) -> list[dict[str, object]]:
    partition = PARTITIONS.model(width, height)
    coefficients = MULTILEAF.model(width, height, flat=True)
    coefficient_events: dict[int, list[dict[str, object]]] = {}
    for event in coefficients["events"]:
        if "row_key" in event:
            coefficient_events.setdefault(int(event["leaf_index"]), []).append(
                event
            )
    events = []
    leaf_index = 0
    for decision in partition["decisions"]:
        contexts = decision.get("contexts", {})
        for name, symbol in decision["symbols"]:
            context_name = "rect_type" if name == "rect_type" else "split"
            row = int(contexts[context_name])
            if buggy and name == "do_split" and decision["node"] == "8x8":
                row = 4
            events.append({
                "name": name, "row": row, "symbol": symbol,
                "node": decision["node"], "x": decision["x"],
                "y": decision["y"],
            })
        if decision["partition"] != "NONE":
            axis = decision["partition"]
            if decision["symbols"] and extension_allowed(decision["node"], axis):
                events.append({
                    "name": "do_ext_partition",
                    "row": int(contexts["extension"]), "symbol": 0,
                    "node": decision["node"], "x": decision["x"],
                    "y": decision["y"],
                })
            continue
        leaf = partition["leaves"][leaf_index]
        block_group = 4 if leaf["width"] == leaf["height"] == 16 else (
            2 if leaf["width"] == leaf["height"] else 3
        )
        events.extend([
            {"name": "y_mode_set", "row": 0, "symbol": 0},
            {"name": "y_mode_index", "row": 0, "symbol": 0},
            {"name": "fsc_mode", "row": block_group, "symbol": 0},
        ])
        for event in coefficient_events[leaf_index]:
            events.append({
                "name": event["name"], "row": int(event["row"]),
                "symbol": int(event["symbol"]),
            })
        leaf_index += 1
    return events


def transcript(width: int, height: int, buggy: bool = False) -> dict[str, object]:
    tables = parse_tables()
    rows: dict[str, list[int]] = {}
    writer = RangeWriter()
    trace = []
    for index, event in enumerate(event_spec(width, height, buggy)):
        name = str(event["name"])
        row = int(event["row"])
        count = SYMBOL_COUNTS[name]
        key = f"{name}[{row}]"
        if key not in rows:
            rows[key] = MULTILEAF.materialize(tables[name][row], count)
        cdf = rows[key]
        before = list(cdf)
        state_before = {
            "low": writer.low, "range": writer.range,
            "count": writer.count, "tell": writer.tell(),
        }
        writer.symbol(int(event["symbol"]), cdf, count)
        MULTILEAF.update_cdf(cdf, count, int(event["symbol"]))
        trace.append({
            "index": index, **event, "row_key": key,
            "cdf_before": before, "state_before": state_before,
            "state_after": {
                "low": writer.low, "range": writer.range,
                "count": writer.count, "tell": writer.tell(),
            },
        })
    tile = writer.finish()
    return {
        "width": width, "height": height, "buggy": buggy,
        "events": trace, "tile_hex": tile.hex(),
        "tile_sha256": hashlib.sha256(tile).hexdigest(),
    }


def vector_case(size: int) -> dict[str, object]:
    correct = transcript(size, size)
    buggy = transcript(size, size, True)
    first = next(
        (
            index for index, pair in enumerate(
                zip(correct["events"], buggy["events"])
            ) if pair[0] != pair[1]
        ),
        None,
    )
    return {
        "size": size,
        "first_difference": first,
        "correct_event": None if first is None else correct["events"][first],
        "buggy_event": None if first is None else buggy["events"][first],
        "correct_tile_sha256": correct["tile_sha256"],
        "buggy_tile_sha256": buggy["tile_sha256"],
    }


def vectors() -> dict[str, object]:
    return {
        "schema": "av2-m3-boundary-partition-transcript-v1",
        "normative_cdf_sha256": hashlib.sha256(NORMATIVE.read_bytes()).hexdigest(),
        "cases": [vector_case(size) for size in CLASSES],
    }


def run(command: list[str]) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False
    )


def probe(output: Path) -> dict[str, object]:
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for size in CLASSES:
        modeled = MULTILEAF.model(size, size)
        for profile in ("flat", "nonflat"):
            name = f"{size}x{size}-{profile}"
            levels = output / f"{name}.levels"
            obu = output / f"{name}.obu"
            avm_raw = output / f"{name}.avm.i400"
            project_raw = output / f"{name}.project.i400"
            level_arg = "flat"
            if profile == "nonflat":
                levels.write_bytes(b"".join(
                    struct.pack("<h", int(tx["level"]))
                    for tx in modeled["transforms"]
                ))
                level_arg = str(levels)
            encoded = run([
                str(DRIVER), str(size), str(size), level_arg, "stream", str(obu)
            ])
            avm = run([
                str(AVMDEC), "--codec=av2", "--rawvideo", "--limit=1",
                "--output-bit-depth=8", "-o", str(avm_raw), str(obu),
            ]) if encoded.returncode == 0 else None
            project = run([
                str(PROJECT_DECODER), str(obu), "-o", str(project_raw)
            ]) if encoded.returncode == 0 else None
            avm_bytes = avm_raw.read_bytes() if avm_raw.is_file() else b""
            project_bytes = (
                project_raw.read_bytes() if project_raw.is_file() else b""
            )
            records.append({
                "name": name,
                "encoder_returncode": encoded.returncode,
                "avmdec_returncode": None if avm is None else avm.returncode,
                "project_decoder_returncode": (
                    None if project is None else project.returncode
                ),
                "obu_bytes": obu.stat().st_size if obu.is_file() else 0,
                "obu_sha256": (
                    hashlib.sha256(obu.read_bytes()).hexdigest()
                    if obu.is_file() else None
                ),
                "output_sha256": hashlib.sha256(avm_bytes).hexdigest(),
                "validators_exact_equal": avm_bytes == project_bytes,
            })
            for path in (levels, obu, avm_raw, project_raw):
                path.unlink(missing_ok=True)
    result = {
        "schema": "av2-m3-boundary-partition-probes-v1",
        "avm_revision": "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d",
        "avmdec_sha256": hashlib.sha256(AVMDEC.read_bytes()).hexdigest(),
        "project_decoder_sha256": hashlib.sha256(
            PROJECT_DECODER.read_bytes()
        ).hexdigest(),
        "cases": records,
        "all_pass": all(
            case["encoder_returncode"] == 0
            and case["avmdec_returncode"] == 0
            and case["project_decoder_returncode"] == 0
            and case["validators_exact_equal"]
            for case in records
        ),
    }
    (output / "report.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n"
    )
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vectors", action="store_true")
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--width", type=int, default=24)
    parser.add_argument("--height", type=int, default=24)
    parser.add_argument("--buggy", action="store_true")
    args = parser.parse_args()
    result = (
        probe(args.probe) if args.probe else
        vectors() if args.vectors else
        transcript(args.width, args.height, args.buggy)
    )
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if not args.probe or result["all_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

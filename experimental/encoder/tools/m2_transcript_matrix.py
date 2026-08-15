#!/usr/bin/env python3
"""Verify frozen C range-writer transcripts with an independent decoder."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from dataclasses import dataclass
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
DRIVER = ENCODER / "build/m2-transcript-driver"
FIXTURE = ENCODER / "fixtures/m2-transcript-matrix.json"


@dataclass
class RangeDecoder:
    data: bytes
    bit_position: int = 0

    def __post_init__(self) -> None:
        count = min(len(self.data) * 8, 15)
        initial = self._bits(count)
        self.value = 0x7FFF ^ (initial << (15 - count))
        self.range = 0x8000
        self.bits_left = len(self.data) * 8 - 15

    def _bits(self, count: int) -> int:
        result = 0
        for _ in range(count):
            result <<= 1
            if self.bit_position < len(self.data) * 8:
                byte = self.data[self.bit_position // 8]
                result |= (byte >> (7 - self.bit_position % 8)) & 1
            self.bit_position += 1
        return result

    def bypass(self) -> int:
        split = self.range >> 1
        result = int(self.value < split)
        if result == 0:
            self.value -= split
        available = max(0, min(1, self.bits_left))
        incoming = self._bits(available)
        self.value = (self.value << 1) | (incoming ^ 1)
        self.bits_left -= 1
        return result

    def literal(self, width: int) -> int:
        result = 0
        for _ in range(width):
            result = (result << 1) | self.bypass()
        return result

    def symbol(
        self, inverse_cdf: list[int], count: int, *, update: bool = False
    ) -> int:
        increments = [16 * (count - index - 1) // count
                      for index in range(count)]
        current = self.range
        symbol = -1
        while True:
            symbol += 1
            previous = current
            frequency = inverse_cdf[symbol]
            scaled = ((frequency >> 7) << 4) + increments[symbol]
            current = ((((self.range >> 8) * scaled) >> 7) << 3)
            if self.value >= current:
                break
        new_range = previous - current
        new_value = self.value - current
        shift = 15 - (new_range.bit_length() - 1)
        self.range = new_range << shift
        available = max(0, min(shift, self.bits_left))
        incoming = self._bits(available) << (shift - available)
        self.value = (new_value << shift) | (
            incoming ^ ((1 << shift) - 1))
        self.bits_left -= shift
        if update:
            interval = 2 if inverse_cdf[count] > 31 else (
                1 if inverse_cdf[count] > 15 else 0)
            rate = 2 + inverse_cdf[count + 1 + interval]
            target = 1 << 15
            for index in range(count - 1):
                if index == symbol:
                    target = 0
                value = inverse_cdf[index]
                if target < value:
                    inverse_cdf[index] = value - ((value - target) >> rate)
                else:
                    inverse_cdf[index] = value + ((target - value) >> rate)
            inverse_cdf[count] += int(inverse_cdf[count] < 32)
        return symbol

    def finish(self) -> None:
        if self.bits_left < -14:
            raise ValueError("decoder consumed beyond the permitted tail")
        trailing = self.bit_position - min(15, self.bits_left + 15)
        padding_end = self.bit_position + max(0, self.bits_left)
        bits = "".join(f"{byte:08b}" for byte in self.data)
        if trailing < 0 or padding_end > len(bits) or bits[trailing] != "1":
            raise ValueError("missing trailing one")
        if "1" in bits[trailing + 1:padding_end]:
            raise ValueError("nonzero trailing padding")


CASES = {
    "binary-sign": [
        ("symbol", 1, 2), ("sign", 1, 0), ("bypass", 0, 0),
    ],
    "adaptive-three": [
        ("symbol", value, 3) for value in (0, 2, 1, 1, 0, 2, 2, 1)
    ],
    "base-br-five": [
        ("base", 4, 5), ("br", 3, 5), ("sign", 1, 0),
    ],
    "remainder-eight": [
        ("symbol", 7, 8), ("literal", 0x13, 5),
        ("sign", 0, 0), ("remainder", 0x2D3, 10),
    ],
}


def compact_initial_cdf(alphabet: int) -> list[int]:
    return {
        2: [16384, 0],
        3: [24576, 8192, 0, 0, 0, 1, 2],
        5: [30000, 22000, 14000, 6000, 0],
        8: [31500, 28000, 23500, 18500, 13000, 8000, 3500, 0],
    }[alphabet].copy()


def matrix_initial_cdf(alphabet: int, count: int) -> list[int]:
    base = 2 if alphabet < 4 else 3
    return [
        (alphabet - index - 1) * 32768 // alphabet
        for index in range(alphabet)
    ] + [count, base, base + 1, base + 2]


def run_driver(*arguments: object) -> dict[str, object]:
    completed = subprocess.run(
        [str(DRIVER), *(str(argument) for argument in arguments)],
        check=True, text=True, capture_output=True)
    return json.loads(completed.stdout)


def decode_case(name: str, encoded: bytes) -> tuple[list[int], list[int]]:
    decoder = RangeDecoder(encoded)
    cdfs: dict[int, list[int]] = {}
    values = []
    for kind, expected, argument in CASES[name]:
        if kind in {"sign", "bypass"}:
            actual = decoder.bypass()
        elif kind in {"literal", "remainder"}:
            actual = decoder.literal(argument)
        else:
            cdf = cdfs.setdefault(argument, compact_initial_cdf(argument))
            actual = decoder.symbol(cdf, argument, update=argument == 3)
        if actual != expected:
            raise ValueError(f"{name}: decoded {actual}, expected {expected}")
        values.append(actual)
    decoder.finish()
    adaptive = cdfs.get(3, compact_initial_cdf(3))
    return values, adaptive[:4]


def matrix_symbols(alphabet: int) -> list[int]:
    return list(range(alphabet)) + list(reversed(range(alphabet)))


def decode_matrix_case(
    alphabet: int, count: int, encoded: bytes
) -> tuple[list[int], list[int]]:
    decoder = RangeDecoder(encoded)
    cdf = matrix_initial_cdf(alphabet, count)
    values = []
    for expected in matrix_symbols(alphabet):
        actual = decoder.symbol(cdf, alphabet, update=True)
        if actual != expected:
            raise ValueError(
                f"alphabet {alphabet}, count {count}: "
                f"decoded {actual}, expected {expected}"
            )
        values.append(actual)
    decoder.finish()
    return values, cdf


def build_matrix() -> dict[str, object]:
    compact_cases = []
    for name, operations in CASES.items():
        roomy = run_driver(name, 64)
        if roomy["status"] != 0:
            raise RuntimeError(f"{name}: roomy C encode failed: {roomy}")
        encoded = bytes.fromhex(str(roomy["hex"]))
        values, adaptive = decode_case(name, encoded)
        exact = run_driver(name, len(encoded))
        short = run_driver(name, len(encoded) - 1)
        if exact != roomy:
            raise RuntimeError(f"{name}: exact capacity changed transcript")
        if short["status"] != 3:
            raise RuntimeError(f"{name}: short capacity did not exhaust")
        compact_cases.append({
            "name": name,
            "coverage": [operation[0] for operation in operations],
            "decoded": values,
            "hex": encoded.hex(),
            "sha256": hashlib.sha256(encoded).hexdigest(),
            "bytes": len(encoded),
            "tells": roomy["tells"],
            "adaptive_cdf": adaptive,
            "capacity": {
                "exact_status": exact["status"],
                "one_byte_short_status": short["status"],
            },
        })
    exhaustive_cases = []
    for alphabet in range(2, 17):
        for count in (0, 15, 16, 31, 32):
            roomy = run_driver("matrix", alphabet, count, 64)
            if roomy["status"] != 0:
                raise RuntimeError(
                    f"alphabet {alphabet}, count {count}: C encode failed")
            encoded = bytes.fromhex(str(roomy["hex"]))
            values, final_cdf = decode_matrix_case(
                alphabet, count, encoded)
            exact = run_driver(
                "matrix", alphabet, count, len(encoded))
            short = run_driver(
                "matrix", alphabet, count, len(encoded) - 1)
            if exact != roomy:
                raise RuntimeError(
                    f"alphabet {alphabet}, count {count}: "
                    "exact capacity changed transcript")
            if short["status"] != 3:
                raise RuntimeError(
                    f"alphabet {alphabet}, count {count}: "
                    "short capacity did not exhaust")
            if roomy["cdf"] != final_cdf:
                raise RuntimeError(
                    f"alphabet {alphabet}, count {count}: "
                    "Python and C final CDFs differ")
            exhaustive_cases.append({
                "alphabet": alphabet,
                "initial_count": count,
                "decoded": values,
                "hex": encoded.hex(),
                "sha256": hashlib.sha256(encoded).hexdigest(),
                "bytes": len(encoded),
                "tells": roomy["tells"],
                "final_cdf": final_cdf,
                "capacity": {
                    "exact_status": exact["status"],
                    "one_byte_short_status": short["status"],
                },
            })
    return {
        "schema": "av2-encoder-m2-transcript-matrix-v2",
        "description": (
            "Frozen C writer transcripts decoded independently by Python; "
            "the exhaustive matrix complements the independent C model."
        ),
        "compact_cases": compact_cases,
        "exhaustive_cases": exhaustive_cases,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    actual = build_matrix()
    text = json.dumps(actual, indent=2, sort_keys=True) + "\n"
    if args.write:
        FIXTURE.write_text(text, encoding="utf-8")
        return 0
    expected = json.loads(FIXTURE.read_text(encoding="utf-8"))
    if actual != expected:
        raise SystemExit("M2 transcript fixture is stale; run with --write")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

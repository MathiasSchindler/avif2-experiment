#!/usr/bin/env python3
"""Independent AV2 v1.0.0 entropy model for the experimental M1 tile."""

from __future__ import annotations

import hashlib
import re
from dataclasses import dataclass
from pathlib import Path

from m1_syntax import SymbolDecoder


ROOT = Path(__file__).resolve().parents[3]
ATTACHMENTS = ROOT / "docs/spec/av2-v1.0.0/attachments"

ATTACHMENT_HASHES = {
    "default_do_split_cdf.h":
        "bb40c072c47561c4742cb96cd6e91cb1ea18ba153862e3e6bd1f88ef19c0b692",
    "default_delta_q_cdf.h":
        "cfdf6d6220bc37e8b29839948c4762502c408777fb73caaf1a1c85761bf74aa2",
    "default_y_mode_set_cdf.h":
        "90aef3ee9186b3244092611a287ffd2f690acf8449bb685ed8a816c79703fcd3",
    "default_y_mode_index_cdf.h":
        "4490c02782275216bda5d75367a13a2b8aa434e891d2558c593e091a5a50f294",
    "default_fsc_mode_cdf.h":
        "64e8bc4636afbcaf712c1ff220be0dff2ad0d67909669a6a8f4e396a63670fb0",
    "default_palette_y_mode_cdf.h":
        "a6bd3157e7092a7acb4219510360b809056cd9f4c7f7477601f17df0a12e0a40",
    "default_txb_skip_cdf.h":
        "0a2189ceaf2dc653d20a21bb6dad16b307c8137a1eb0fabfded3f0b36fa502c0",
    "para_adjustment_list.h":
        "0488e7e0a8d16a89f53ba8cf1ec8e449a9a3e74aa34ba2fa49e287159336ef80",
    "prob_inc.h":
        "d32d08e0e65c78968b87967d5b33e84493e8029dc1f4bc4d50e888e3f77ffeea",
}

RECONSTRUCTED_ENCODER_TRANSCRIPT = (
    ("do_split", 0),
    ("delta_q_abs", 1),
    ("delta_q_sign_bit", 1),
    ("y_mode_set", 0),
    ("y_mode_index", 0),
    ("fsc_mode", 0),
    ("all_zero", 1),
)


class NormativeMismatch(RuntimeError):
    pass


@dataclass(frozen=True)
class Decision:
    name: str
    context: str
    initial_cdf: tuple[int, ...] | None
    adaptation_enabled: bool
    value: int
    value_before: int
    range_before: int
    max_bits_after: int


def _integers(path: Path) -> list[int]:
    return [int(value) for value in re.findall(r"-?\d+", path.read_text())]


def verify_attachments() -> None:
    for name, expected in ATTACHMENT_HASHES.items():
        digest = hashlib.sha256((ATTACHMENTS / name).read_bytes()).hexdigest()
        if digest != expected:
            raise NormativeMismatch(
                f"{name}: sha256 {digest}, expected {expected}")


def _rows(name: str, row_size: int) -> list[tuple[int, ...]]:
    values = _integers(ATTACHMENTS / name)
    # Discard dimensions and declarations by taking only braced numeric rows.
    source = (ATTACHMENTS / name).read_text()
    pattern = r"\{\s*" + r"\s*,\s*".join(
        [r"(-?\d+)"] * row_size) + r"\s*\}"
    return [tuple(map(int, row)) for row in re.findall(pattern, source)]


def probability_increments() -> list[tuple[int, ...]]:
    rows = _rows("prob_inc.h", 8)
    if len(rows) != 7:
        raise NormativeMismatch(f"expected 7 Prob_Inc rows, got {len(rows)}")
    return rows


def parameter_adjustments() -> list[tuple[int, ...]]:
    rows = _rows("para_adjustment_list.h", 3)
    if len(rows) != 125:
        raise NormativeMismatch(
            f"expected 125 parameter-adjustment rows, got {len(rows)}")
    return rows


class M1EntropyModel:
    def __init__(self, tile: bytes, *, disable_cdf_update: bool = False) -> None:
        verify_attachments()
        self.decoder = SymbolDecoder(tile)
        self.disable_cdf_update = disable_cdf_update
        self.prob_inc = probability_increments()
        self.para_adjustment = parameter_adjustments()
        self.cdfs: dict[str, list[int]] = {}
        self.decisions: list[Decision] = []

    def symbol(self, name: str, context: str, row: tuple[int, ...]) -> int:
        cdf = self.cdfs.setdefault(context, list(row))
        before_value = self.decoder.value
        before_range = self.decoder.range
        symbols = len(cdf) - 1
        parameter = cdf[symbols - 1]
        base_rate = 3 + min(symbols.bit_length() - 1, 2)
        update_rates = tuple(
            base_rate + adjustment
            for adjustment in self.para_adjustment[parameter]
        )
        value = self.decoder.read_symbol(
            cdf,
            list(self.prob_inc[symbols - 2]),
            update_cdf=not self.disable_cdf_update,
            update_rates=update_rates,
        )
        self.decisions.append(Decision(
            name, context, row, not self.disable_cdf_update, value,
            before_value, before_range, self.decoder.max_bits))
        return value

    def literal_bit(self, name: str) -> int:
        before_value = self.decoder.value
        before_range = self.decoder.range
        value = self.decoder.read_bool()
        self.decisions.append(Decision(
            name, "equiprobable literal", None, False, value,
            before_value, before_range, self.decoder.max_bits))
        return value

    def decode_prefix(self) -> tuple[Decision, ...]:
        # BLOCK_8X8 is block-size index 3. Partition_Size_Adjust[3] is 0;
        # both edge contexts are false because contexts start at BLOCK_256X256.
        do_split = _rows("default_do_split_cdf.h", 3)[0]
        self.symbol("do_split", "TileDoSplitCdf[0][0]", do_split)

        delta_q = _rows("default_delta_q_cdf.h", 9)[0]
        delta = self.symbol("delta_q_abs", "TileDeltaQCdf", delta_q)
        if delta:
            self.literal_bit("delta_q_sign_bit")
        return tuple(self.decisions)

    def require_reconstructed_prefix(self) -> tuple[Decision, ...]:
        decisions = self.decode_prefix()
        expected = RECONSTRUCTED_ENCODER_TRANSCRIPT[:len(decisions)]
        actual = tuple((decision.name, decision.value) for decision in decisions)
        if actual != expected:
            raise NormativeMismatch(
                "hash-locked AV2 v1.0.0 tables do not decode the "
                f"reconstructed encoder prefix: {actual} != {expected}")
        return decisions


def main() -> int:
    model = M1EntropyModel(bytes.fromhex("8d80"))
    try:
        model.require_reconstructed_prefix()
    except NormativeMismatch as error:
        for decision in model.decisions:
            print(
                f"{decision.name}: value={decision.value} "
                f"context={decision.context} cdf={decision.initial_cdf} "
                f"adapt={int(decision.adaptation_enabled)}")
        print(f"BLOCKED: {error}")
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Independent AV2 v1.0.0 syntax-helper reference decoders."""

from __future__ import annotations


class ReferenceError(ValueError):
    pass


class RawReader:
    def __init__(self, data: bytes) -> None:
        self.bits = "".join(f"{byte:08b}" for byte in data)
        self.position = 0

    def literal(self, width: int) -> int:
        if width < 0 or self.position + width > len(self.bits):
            raise ReferenceError("truncated literal")
        value = int(self.bits[self.position:self.position + width] or "0", 2)
        self.position += width
        return value

    def uvlc(self) -> int:
        leading = 0
        while self.literal(1) == 0:
            leading += 1
            if leading >= 32:
                raise ReferenceError("invalid UVLC")
        return (1 << leading) - 1 + self.literal(leading)

    def svlc(self) -> int:
        value = self.uvlc()
        half = (value + 1) >> 1
        return half if value & 1 else -half

    def signed_literal(self, magnitude_bits: int) -> int:
        width = magnitude_bits + 1
        value = self.literal(width)
        return value - (1 << width) if value & (1 << magnitude_bits) else value

    def ns(self, symbol_count: int) -> int:
        if symbol_count <= 0:
            raise ReferenceError("invalid ns domain")
        if symbol_count == 1:
            return 0
        width = (symbol_count - 1).bit_length()
        cutoff = (1 << width) - symbol_count
        prefix = self.literal(width - 1)
        return prefix if prefix < cutoff else (
            (prefix << 1) - cutoff + self.literal(1)
        )

    def subexp(self, symbol_count: int, parameter: int) -> int:
        iteration = 0
        consumed = 0
        while True:
            width = parameter + iteration - 1 if iteration else parameter
            group = 1 << width
            if symbol_count <= consumed + 3 * group:
                return consumed + self.ns(symbol_count - consumed)
            if self.literal(1) == 0:
                return consumed + self.literal(width)
            iteration += 1
            consumed += group

    @staticmethod
    def inverse_recenter(reference: int, value: int) -> int:
        if value > 2 * reference:
            return value
        if value & 1:
            return reference - ((value + 1) >> 1)
        return reference + (value >> 1)

    def unsigned_subexp_with_ref(
        self, symbol_count: int, reference: int, parameter: int
    ) -> int:
        value = self.subexp(symbol_count, parameter)
        if 2 * reference <= symbol_count:
            return self.inverse_recenter(reference, value)
        return symbol_count - 1 - self.inverse_recenter(
            symbol_count - 1 - reference, value
        )

    def signed_subexp_with_ref(
        self, low: int, high: int, reference: int, parameter: int
    ) -> int:
        return low + self.unsigned_subexp_with_ref(
            high - low, reference - low, parameter
        )

    def trailing_bits(self) -> None:
        if self.literal(1) != 1:
            raise ReferenceError("missing trailing one bit")
        while self.position & 7:
            if self.literal(1) != 0:
                raise ReferenceError("nonzero trailing bit")


def read_range_ns(decoder, symbol_count: int) -> int:
    if symbol_count == 1:
        return 0
    width = (symbol_count - 1).bit_length()
    cutoff = (1 << width) - symbol_count
    prefix = decoder.read_literal(width - 1)
    return prefix if prefix < cutoff else (
        (prefix << 1) - cutoff + decoder.read_bool()
    )


def read_coeff_remainder(decoder, predicted_level: int) -> int:
    m = min(6, max(1, predicted_level.bit_length() - 1))
    k = m + 1
    cmax = min(m + 4, 6)
    q = 0
    while q < cmax and decoder.read_bool() == 0:
        q += 1
    if q < cmax:
        return (q << m) + decoder.read_literal(m)
    length = 0
    while decoder.read_bool() == 0:
        length += 1
    length += k
    base = (q << m) + (1 << length) - (1 << k)
    return base + decoder.read_literal(length)


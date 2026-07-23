# Milestone 6: frame-wide wiring and odd fixture

Milestone 6 is planned. It turns the private leaf-at-a-time machinery into a
complete coded-frame reconstruction path and proves that path on the 17x15 odd
fixture.

## Scope

- connect traversal commits to live Y, U, and V reconstruction planes;
- retain references and per-4x4 filter metadata for the complete coded extent;
- complete every prediction, transform, coefficient, and chroma path reached
  by the odd fixture;
- reconstruct padded coded planes and crop only after frame processing; and
- keep restoration and every unreached syntax path fail-closed.

The odd fixture is deliberately paired with frame-wide wiring: its coded-edge
extension, chroma rounding, and 17x15 crop make those ownership rules observable
without also carrying the ordinary fixture's larger syntax union.

## Exit gate

The complete odd fixture must match pinned AVM on unfiltered Y, U, and V
samples and exact entropy endpoints under ASan, UBSan, and the strict
freestanding dependency gate. No fixture-supplied contexts, references, or
leaf ordering may enter production decoding.

Milestone 6 does not expose a public decoder and does not claim filtered output
parity.
# Frame-wide wiring and odd fixture

Frame-wide wiring is implemented. It turns the private leaf-at-a-time machinery into
a complete coded-frame reconstruction path and proves that path on the 17x15
odd fixture.

## Scope

- connect traversal commits to live Y, U, and V reconstruction planes;
- retain live references and per-4x4 reconstruction-readiness metadata for the
  complete coded extent;
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

frame wiring does not expose a public decoder and does not claim filtered output
parity.

## Evidence

- all 12 luma leaves reconstruct into caller-owned 24x16 coded storage;
- all five chroma leaves reconstruct into caller-owned 12x8 U and V storage;
- crop-aware comparison matches the pinned AVM prefilter copy on all 255 Y,
  72 U, and 72 V visible samples;
- traversal terminates at the pinned entropy endpoint, tell 696;
- visible reference bounds, coded write bounds, and per-4x4 readiness are
  separate caller-owned state, so future and padded samples cannot become
  prediction references; and
- the hosted ASan/UBSan units and strict freestanding zero-undefined-symbol
  link pass. Complete deblock and CCSO map population remains filter parity work.
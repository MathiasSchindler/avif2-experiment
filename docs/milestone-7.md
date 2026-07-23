# Milestone 7: ordinary fixture coverage

Milestone 7 is in progress through an early checkpoint. Ordinary luma currently
commits six leaves. The decoder then stops fail-closed at `(4,14)`, block 4x8,
DC mode, TX_4X8, EOB 29, and primary `FLIPADST_ADST` before unsupported
coefficient scanning.

## Scope

- complete the remaining ordinary luma partition and leaf traversal;
- add only the normative CDF rows, scan rules, prediction modes, and inverse
  transforms reached by that traversal;
- complete ordinary chroma mode, coefficient, prediction, and reconstruction;
- preserve live-reference and commit-before-context-update invariants; and
- freeze each new frontier with exact syntax, tell, coefficient, and sample
  evidence.

## Exit gate

The complete ordinary fixture must match pinned AVM on unfiltered Y, U, and V
planes, terminate at the authoritative entropy endpoint, and pass sanitizers
and the strict freestanding gate. Sparse normative tables remain acceptable
only while every omitted row fails closed before reconstruction.

Milestone 7 does not integrate frame filters or expose a public decoder.
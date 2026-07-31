# Milestone 10: bounded public decode surface

Milestone 10 is planned. It converts the proven private pipeline into a stable
caller-owned API only after complete filtered sample parity.

## Scope

- compute exact workspace size and alignment for decoder state, coded planes,
  metadata maps, filter stages, and transform scratch;
- bind decode calls to immutable query results and reject stale or changed
  input;
- validate caller output layouts and preserve output on preflight failure;
- expose structured errors at the container, syntax, reconstruction, filter,
  workspace, and output layers;
- add truncation matrices, alignment coverage, low-limit fuzzing, and release
  archive/link-graph checks; and
- document the exact accepted profile and every fail-closed exclusion.

## Exit gate

The public decoder must reproduce pinned AVM's filtered planes for the four
currently byte-exact artifact fixtures across allowed workspace alignments.
Chimp Q64 remains outside this gate until its chroma reconstruction is exact.
Undersized workspace, stale
query state, malformed syntax, and every shorter input prefix must fail
deterministically without allocation or unintended output mutation.

Release artifacts must remain CC0, freestanding C11, allocator-free, nolibc,
and free of vendored headers, symbols, and runtime dependencies.
# Milestone 10: bounded public decode surface

Milestone 10 is complete. It exposes the proven private pipeline through a
bounded caller-owned API after complete filtered sample parity.

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

`av2h_query()` now validates the complete accepted decode profile and reports
the exact workspace size and power-of-two alignment, contiguous I420 plane
offsets, row bytes, and total output size. `av2h_decode()` reparses the immutable
input, verifies its size and fingerprint against the query result, validates
workspace and output bounds and disjointness, initializes only caller-owned
state, and decodes without allocation or I/O.

The output buffer is one contiguous I420 image. `Av2hInfo.planes[0..2]`
describe Y, U, and V respectively; each plane starts at `offset`, has
`row_bytes` bytes per row, and occupies `size` bytes. The workspace pointer
must satisfy `workspace_alignment` and provide at least `workspace_size` bytes.
Input, workspace, output, query-info, and error ranges must be mutually
disjoint where either range is writable.

The public error context distinguishes container and OBU parsing, frame syntax,
query binding, workspace, output, reconstruction, filtering, and remaining
decode failures. A changed input or altered `Av2hInfo` fails with
`AV2H_CONTEXT_BINDING`. Preflight failures leave the output untouched.

The hosted `public-decode-unit` normalizes and muxes the tiny, odd, ordinary,
and high-detail fixtures into canonical AV2-HEIF v0, then checks complete output
hashes through the public API. The ordinary case additionally covers
undersized and misaligned workspace, undersized output, stale query state,
overlap, and every shorter container prefix with output nonmutation.

The complete hosted suite passes 17 tests and the strict freestanding object
gate has no undefined symbols. The Windows x86-64 release suite passes all 68
tests, including the 31-artifact byte-parity gate and the four-fixture filtered
parity runner. All tracked artifacts are byte-identical to pinned AVM.

Release artifacts must remain CC0, freestanding C11, allocator-free, nolibc,
and free of vendored headers, symbols, and runtime dependencies.

The configured Windows sanitizer preset could not run on the validation host:
the installed AArch64 LLVM-MinGW package does not contain the requested ASan
runtime archive. The ordinary hosted, strict freestanding, and Windows release
gates are unaffected; sanitizer validation remains required on a host with the
runtime installed.
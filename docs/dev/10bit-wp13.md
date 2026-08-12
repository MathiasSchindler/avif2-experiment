# 10-bit implementation WP13 filters and public output

## Result

10B-WP13 is complete for profile-4 10-bit 4:4:4. Native OBU decode and the
canonical HEIF public API emit tightly packed little-endian `yuv444p10le` with
full-resolution Y, U, and V planes. All 21 retained synthetic and 18 retained
photographic records match pinned AVM revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d` byte-for-byte, including lossless,
odd, boundary, high-detail, filtered, and 1200 x 1596 inputs.

## Decoder integration

The canonical container admits only the normative profile-4/chroma-2 pair for
10-bit 4:4:4. The profile-0 10-bit 4:2:0 path remains separate. U16 chroma
reconstruction now covers full-resolution explicit and derived-alpha CfL,
MHCCP, directional prediction, coded-edge extension, and plane-major lossless
transform scheduling. Directional chroma uses AVM's two-tap interpolation;
luma retains four-tap IDIF. Large chroma references stop at the nominal edge,
and frame-edge filtering occurs before endpoint extension.

Lossless U/V entropy state is carried across the coding block, FSC inherits
IDTX, and deblock metadata is committed per reconstructed plane. The shared
inverse CCTX owner uses AVM's signed `8 + bit_depth` coefficient range. Final
corpus parity exercises deblock, CDEF, CCSO, and GDF at full chroma resolution.

## Public contract

For a 16 x 16 profile-4 image, `av2h_query` reports three 16 x 16 planes with
32-byte rows at offsets 0, 512, and 1024, an output size of 1,536 bytes, a
28,801-byte workspace, and four-byte workspace alignment. The public unit gate
also verifies U16 output alignment, undersized workspace/output rejection,
overlap rejection, stale bindings, pixel-metadata mismatch, every truncated
container prefix, and output nonmutation on failure.

`tools/validate_10bit_444_corpus.py` checks manifest sizes and SHA-256 hashes,
native decode, canonical HEIF packing, and public decode for all 39 records.
CMake registers hosted native/public and Windows native WP13 parity tests.
`make 10bit-wp13` covers hosted sanitizers and 8-bit regressions, Linux no-libc,
WebAssembly, the Windows cross-build/import contract, and the complete retained
profile-4 corpus.

## Validation

The completion run established:

- 39/39 retained profile-4 records exact through hosted native/public decode,
  Linux static no-libc, and WebAssembly;
- 24/24 hosted ASan/UBSan C tests and 36/36 Python tests passed;
- 31/31 retained I420 artifacts, both 12-image I400/I444 native corpora, and
  the 12-image I444 public corpus remained exact;
- strict freestanding and 2/2 Linux no-libc tests passed; and
- Windows x86-64 executables cross-built with exact import contracts.

Windows runtime parity remains environment-dependent because this Linux host
has no configured PE runtime.

A follow-up resolved the inherited profile-0 large-photo divergence. For large
SDP leaves, local CfL eligibility consulted `chroma_root_partition` before the
chroma traversal had populated it. The resulting `PARTITION_INVALID` sentinel
incorrectly disabled CfL/MHCCP, skipped `UV_CFL_PRED` and the CfL/MHCCP switch
at tell 273, and desynchronized the remaining entropy syntax. Large 128 x 128,
64 x 128, and 128 x 64 SDP leaves now follow AVM's `PARTITION_NONE` allowance.
All 43 retained 10-bit 4:2:0 records are byte-exact through hosted ASan/UBSan
native and public decode.

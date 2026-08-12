# 10-bit implementation WP11 corpus release gate

## Result

10B-WP11 is complete. Profile-0 10-bit 4:2:0 is an accepted public decoder
profile with tightly packed little-endian U16 `yuv420p10le` output. The complete
retained matrix matches pinned AVM revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d` sample-for-sample through native raw
OBU decode and canonical HEIF public decode.

The matrix contains 43 records: 25 deterministic synthetic records and 18
project-authored photographic records. It covers 16x16 through 1200x1596,
odd geometry, boundary extremes, high detail, retained Q-index sweeps, CfL and
MHCCP downsample filters, directional prediction, transforms, and in-loop
filters.

## Decoder completion

The final corpus expansion completed U16 intra-prediction reference extension,
wide-angle mapping, independent smooth contexts, CfL baselines and training,
MHCCP directions and downsample filters, inverse transforms, reconstruction,
and final frame output. The filter-aware CfL and MHCCP paths preserve Q3 luma
precision and clamp taps at transform and superblock boundaries as specified by
the pinned AVM behavior.

Existing 8-bit sample owners remain separate. The normal I400, I420, and I444
regression gates continue to require their original exact bytes.

## Retained evidence

The synthetic manifest is
`tests/fixtures/10bit/avm-reference/manifest.json`; the photographic manifest is
`images/artifacts/10bit/avm-reference/manifest.json`. Both pin encoded and
decoded sizes and SHA-256 values. `tools/validate_10bit_420_corpus.py` verifies
manifest integrity, exact native output, canonical HEIF packing, and exact
public output. Its native-only mode drives platform adapters that do not host
the public artifact tool.

`tools/validate_10bit_wasm.js` validates all 43 records in one WebAssembly
instance. For every record it checks dimensions, channel count, chroma format,
bit depth, two-byte samples, little-endian storage, output size, and decoded
SHA-256.

## Validation

Run the complete gate with:

```sh
make 10bit-wp11
```

The target runs the complete sanitized hosted C and Python regression suite,
all 43 native/public records, the strict freestanding undefined-symbol check,
the Linux static no-libc tests and native corpus, and the complete WebAssembly
corpus. CMake also registers `decoder-10bit-420-parity` when Python development
tests are enabled and `windows-decoder-10bit-420-parity` for Windows native
runtime environments.

The completion run established:

- 43/43 records exact through native and public decode;
- 43/43 records exact through the Linux static no-libc decoder;
- 43/43 records exact through WebAssembly;
- 24/24 hosted ASan/UBSan C tests and 36/36 Python tests passed;
- all retained 8-bit I400, I420, and I444 outputs remained exact;
- strict freestanding and 2/2 Linux no-libc tests passed;
- Windows x86-64 executables cross-built with their exact import contracts; and
- the 139-file retained 10-bit corpus reproduced byte-for-byte.

Windows runtime execution remains environment-dependent; the CMake gate is
registered for a configured emulator or Windows host. At WP11 completion the
repository had no standalone fuzz harness; WP14 added the bounded public
query/decode harness and deterministic sanitizer gate documented in
`docs/dev/10bit-wp14.md`. The WP11 profile did not admit 10-bit 4:4:4, 4:2:2,
12-bit, restoration, film grain, segmentation, multiframe, or multitile
streams.

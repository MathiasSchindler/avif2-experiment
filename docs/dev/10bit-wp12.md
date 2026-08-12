# 10-bit implementation WP12 4:4:4 integration gate

## Result

10B-WP12 is complete. The raw decoder accepts the normative profile-4,
10-bit 4:4:4 signaling pair and emits tightly packed little-endian U16
`yuv444p10le` with full-resolution Y, U, and V planes. The retained 16x16 tiny
and 17x15 odd fixtures match pinned AVM revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d` sample-for-sample on every
unfiltered plane.

The profile/chroma admission is deliberately paired: profile 0 remains the
10-bit 4:2:0 owner and profile 4 is admitted only with 4:4:4. Cross-paired
signals and other 10-bit chroma formats remain unsupported.

## Decoder integration

WP12 combines the existing U16 sample-domain owners with the mature 4:4:4
geometry. The pixel descriptor reports two-byte little-endian samples and
three full-resolution planes, including exact odd dimensions. U16 chroma
reconstruction uses the 4:4:4 leaf-coordinate scale while retaining the
existing 4:2:0 scale unchanged. POSIX, Linux no-libc, and Windows adapters
recognize the `.yuv444p10le` raw suffix and select the matching descriptor.

The two constrained records contain no in-loop filter accounting and therefore
provide the required unfiltered checkpoint. Full-resolution CfL/MHCCP and the
boundary, high-detail, lossless, photographic, filtered, and public canonical
HEIF surfaces remain fail-closed or deferred to WP13. The canonical container
admission boundary is unchanged.

## Retained evidence

`tools/validate_10bit_444_wp12.py` locks the two fixture names, dimensions,
format, bit depth, QP, filter-free census, encoded and decoded manifest hashes,
and exact output size. It compares the visible Y, U, and V planes independently
against the retained AVM output:

- tiny 16x16: 1,536 bytes exact;
- odd 17x15: 1,530 bytes exact.

CMake registers `decoder-10bit-444-wp12-parity` for hosted Python-enabled
builds and `windows-decoder-10bit-444-wp12-parity` where Windows runtime tests
are available. The Makefile gate runs the validator through hosted ASan/UBSan
and the static Linux no-libc decoder.

## Validation

Run the complete gate with:

```sh
make 10bit-wp12
```

The completion run established:

- 2/2 constrained 10-bit 4:4:4 records exact on unfiltered Y/U/V through the
  hosted sanitizer decoder and Linux static no-libc decoder;
- 43/43 retained 10-bit 4:2:0 records exact through native/public decode,
  Linux no-libc, and WebAssembly;
- 24/24 hosted ASan/UBSan C tests and 36/36 Python tests passed;
- all retained 8-bit I400, I420, and I444 native/public outputs remained exact;
- strict freestanding and 2/2 Linux no-libc tests passed;
- Windows x86-64 executables cross-built with exact import contracts; and
- the 139-file retained 10-bit corpus reproduced byte-for-byte.

Windows runtime parity remains environment-dependent because this Linux host
has no configured PE runtime. WP13 owns mandatory 10-bit 4:4:4 filters,
full-resolution CfL-family expansion, and canonical public HEIF output.
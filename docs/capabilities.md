# Decoder capabilities

**Status:** authoritative current decoder contract as of 19 August 2026

This document is the source of truth for decoder admission. A feature is
supported only for the stated tuple and only after exact decoded-output parity
with pinned AVM. Encoder presets may choose a narrower subset.

## Public outputs

| Format | Bit depth | Chroma | Storage | Status |
| --- | ---: | --- | --- | --- |
| I400 | 8 | Monochrome | One byte per sample | Supported |
| I420 | 8 | 4:2:0 | One byte per sample | Supported |
| I444 | 8 | 4:4:4 | One byte per sample | Supported |
| `yuv420p10le` | 10 | 4:2:0, profile 0 | Little-endian `uint16_t` | Supported |
| `yuv444p10le` | 10 | 4:4:4, profile 4 | Little-endian `uint16_t` | Supported |
| 10-bit monochrome | 10 | Monochrome | - | Unsupported |
| 4:2:2 | 8 or 10 | 4:2:2 | - | Unsupported |
| 12-bit | 12 | Any | - | Unsupported |

## Admitted 8-bit coding tools

| Area | Admitted subset | Important exclusions |
| --- | --- | --- |
| Picture structure | One closed-loop key picture; uniform/non-uniform spatial tiles, multiple tile groups, context resets, and both loop-filter boundary choices | Multi-frame, temporal/layered, non-initial, and additional-operating-point structures |
| Prediction | Still-applicable intra prediction, MRL, CfL/MHCCP, FSC/CCTX, Intra-DIP, IBP, palette, transform skip, and IntraBC | Inter prediction and temporal reference state |
| Transforms and coefficients | Legal admitted transform partitions, primary/secondary transforms, TCQ, and parity hiding | Frame-adaptive video TCQ and out-of-profile bit depths |
| Quantization | Lossy and lossless base Q, delta-Q, predefined/custom QMs, and segmented matrix selection | 10/12-bit combinations outside their separately documented profiles |
| Segmentation | Still-applicable segment IDs, ALT_Q routing, extended segmentation, and lossless interaction | Temporal segment-map prediction |
| Filters and restoration | Deblock, CDEF, CCSO, GDF, PC-Wiener, Wiener-nonsep, and switchable per-unit restoration in normative order and at tile boundaries | Temporal filtering and unadmitted 10-bit restoration |
| Signaling and geometry | I400/I420/I444, odd visible dimensions, legal 8-bit color/chroma-location metadata, and checked workspace endpoints | I422, 12-bit, film grain, and independent clean-aperture transforms |

Support for one row does not imply support for every cross-product with other
rows. Unknown or unsupported combinations must fail closed.

## Explicitly unsupported

- Film-grain synthesis.
- 10-bit restoration outside the separately admitted 10-bit decode profiles.
- Temporal segmentation, inter pictures, and multi-frame or layered streams.
- Container transforms, metadata expansion, auxiliary alpha, and image grids.

Malformed input, inconsistent HEIF metadata, unsupported profile/chroma pairs,
and insufficient or overlapping caller buffers are rejected.

## Evidence

The moving-oracle Baya Weaver I420 QP 152 regression additionally covers 8-bit
luma DC prediction at 4x64, with focused AVM-output checks for both 4x64 and
64x4, all edge-availability combinations, and IBP on/off. Its retained
[stream and provenance](../tests/fixtures/baya-weaver-current-i420-qp152.json)
bind the exact decoded output to AVM revision
`edaf3ebc7ce3666290003f70a66efe5a508982a1`; this is fixture provenance, not a
second pinned oracle. Frame-stage unsupported errors retain
`AV2H_UNSUPPORTED`, without conversion to overflow or other unrelated statuses.

The 8-bit photographic reference tree contains 88 legacy streams and 14
target-baseline streams. All 102 decode exactly with the hosted decoder and the
published `docs/decoder.wasm` against AVM revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.

The `spatial-still-8bit` admission manifest adds 16 retained exact streams and
eight source-hash-bound focused witnesses for syntax cross-products that pinned
AVM does not reliably emit as standalone vectors. The hosted and Linux no-libc
decoders and WebAssembly module produce the locked output for all 16 streams;
Windows x86-64 and ARM64 cross-build and import gates pass. Windows runtime
parity was not executed on the Linux admission host.

Run the complete profile and platform gate:

```sh
make spatial-still-platform-admission
```

Run lock validation without local media:

```sh
make test-reference-locks
```

When the ignored reference files are present, run exact native parity:

```sh
make test-reference-images
```

Run the same locks through the published WebAssembly decoder:

```sh
make test-reference-images-wasm
```

The 10-bit release evidence is recorded in [dev/10bit-wp14.md](dev/10bit-wp14.md).
Corpus provenance and recipe details are in [../images/README.md](../images/README.md).
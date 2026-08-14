# Decoder capabilities

**Status:** authoritative current decoder contract as of 14 August 2026

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
| Picture structure | One closed-loop key picture in the project-defined still-image carriage | Multi-frame, non-initial or unsupported tile-group forms |
| Prediction | Directional prediction, MRL, CfL/MHCCP, luma FSC, Intra-DIP, directional IBP, transform skip, and observed palette paths | Chroma FSC remains unaudited; feature combinations require reached evidence |
| IntraBC | Reached 8-bit still-picture IntraBC and extended search paths | Unreached geometry and profile combinations |
| Transforms | Non-DCT transforms, transform partitioning, and reached secondary transforms | Unreached transform, geometry, and coefficient tuples |
| TCQ | Pinned AVM always-on single-picture TCQ | Frame-adaptive video TCQ and parity hiding |
| Quantization matrices | Predefined matrices in non-segmented pictures | Custom matrix OBUs and segmented multi-matrix selection |
| AQ, delta-Q, segmentation | Pinned AVM `aq-mode=1` ALT_Q segmentation and `deltaq-mode=1`/`2` for closed-loop key pictures | Extended segmentation features, non-ALT_Q features, and untested bit depths |
| Filters | Deblock, CDEF, CCSO, restoration, and GDF in normative pipeline order | Unsupported restoration combinations below |
| Restoration | 8-bit luma PC-Wiener; 8-bit `RESTORE_WIENER_NONSEP` on any I420 plane with frame-level or per-unit filters and one or more luma classes | Switchable restoration, other chroma formats, and all 10-bit restoration |

Support for one row does not imply support for every cross-product with other
rows. Unknown or unsupported combinations must fail closed.

## Explicitly unsupported

- Film-grain synthesis.
- Parity-hiding coefficient syntax.
- Custom quantization-matrix OBUs.
- Segmentation features outside the admitted ALT_Q subset.
- Switchable and 10-bit restoration.
- Unconstrained base-Q-zero streams outside existing lossless evidence.
- Multi-frame streams and unsupported tile signaling.
- Container transforms, metadata expansion, auxiliary alpha, and image grids.

Malformed input, inconsistent HEIF metadata, unsupported profile/chroma pairs,
and insufficient or overlapping caller buffers are rejected.

## Evidence

The 8-bit photographic reference tree contains 88 legacy streams and 14
target-baseline streams. All 102 decode exactly with the hosted decoder and the
published `docs/decoder.wasm` against AVM revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.

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
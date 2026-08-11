# 10-bit preparation WP6 sample-domain semantics

WP6 was implemented and validated on 2026-08-11. It centralizes semantic
8-bit sample constants and clipping bounds without enabling 10-bit decode,
changing sample storage, or changing existing 8-bit output bytes.

## Sample-domain contract

`sample_math.h` now owns the private `Av2hSampleDomain` description:

| Field | 8-bit production value | Meaning |
| --- | ---: | --- |
| `bit_depth` | 8 | Bits in one decoded sample |
| `bytes_per_sample` | 1 | Current typed-plane storage width |
| `midpoint` | 128 | Neutral and missing-reference sample |
| `maximum` | 255 | Inclusive sample maximum |

The 8-bit domain is the only production domain instantiated. Named helpers
provide its midpoint, midpoint-adjacent defaults, and maximum. The existing
`av2h_sample_clip` interface remains an 8-bit `uint8_t` wrapper so current
callers retain their storage contract.

`av2h_sample_clip_to_domain` is the width-neutral primitive. It accepts a
signed 32-bit intermediate and returns `uint16_t`, allowing a future 10-bit
caller to clip to `[0, 1023]` without narrowing through `uint8_t`. Unit tests
exercise the complete 8-bit boundary, a synthetic `{ 10, 2, 512, 1023 }`
domain, and `INT32_MIN`/`INT32_MAX` inputs. The synthetic domain is test-only;
it does not enable a 10-bit decoder path.

## Semantic inventory

The following existing 8-bit dependencies now use the domain or its wrappers:

| Owner | Migrated semantics |
| --- | --- |
| Luma intra prediction | Missing references, neutral fills, midpoint-adjacent smooth defaults, and final predictor saturation |
| Chroma prediction | DC averages, neutral fills, missing references, fixed-point neutral neighbor defaults, bit depth, and final predictor saturation |
| Intra block-copy prediction | Morph-predictor sample saturation |
| Reconstruction and inverse transform | Missing-predictor midpoint and final reconstructed-sample saturation |
| Palette | Sample maximum and palette range |
| CDEF | Sample recentering around the midpoint |
| CCSO, deblock, CDEF, and GDF | Existing sample stores retain `av2h_sample_clip` and inherit its domain-backed implementation |

The fixed-point chroma luma fallback remains midpoint shifted by its explicit
three fractional bits. This records both the semantic sample value and the
algorithm's fixed-point representation.

The reviewed literal census deliberately preserves these unrelated constants:

- directional interpolation and IBP weights whose normalization is 128;
- transform coefficients and identity scales, including 128 and 255;
- CDF, probability, feature-vector, rounding, and normalization constants;
- syntax mode values and partition enum sentinels;
- Q-index limits in leaf, coefficient, and deblock code; and
- signed coefficient and transform intermediate bounds.

Those values are algorithm, syntax, or coefficient-domain properties rather
than decoded-sample midpoint or maximum values.

## Compatibility

This package changes no public declaration, public aggregate, sample storage
type, plane layout, workspace layout, syntax parsing, transform arithmetic, or
filter algorithm. Sample-producing call sites still store `uint8_t`, and the
8-bit clip wrapper still saturates to `[0, 255]`. The tracked viewer WebAssembly
module was regenerated from the same source changes.

## Validation gates

The completion run produced these results:

- focused sample-math, prediction, reconstruction, tile, leaf, and filter
  vectors passed under ASan/UBSan after each owner migration;
- hosted ASan/UBSan C suite: 20/20 passed;
- Python development suite: 21/21 passed;
- independent semantic-literal review found no remaining sample-domain use;
- clean native decoder and strict freestanding core: built with no undefined
  core symbols;
- production I420 parity: 31/31 exact;
- production I400 parity: 12/12 exact;
- production I444 native parity: 12/12 exact;
- canonical HEIF public I444 parity: 12/12 exact;
- Linux static no-libc suite: 2/2 passed;
- WebAssembly module: built and produced 12/12 exact I444 outputs through
  Node's WebAssembly runtime; and
- Windows x86-64: decoder and smoke PE32+ binaries built with no diagnostics;
  the decoder retains its established ten `KERNEL32.dll` imports and the smoke
  binary imports only `ExitProcess`.

Windows runtime parity could not execute on this Linux validation host because
no Wine, Wibo, or PE binfmt handler is installed. The native Windows CTest gate
remains required on Windows or a host with a configured PE emulator.

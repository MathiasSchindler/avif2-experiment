# M2 validator matrix

This matrix freezes the entropy-writer boundary without changing production
range or syntax code.

## Range transcripts

`tests/m2_transcript_driver.c` links the production `src/range.c` and emits
four compact deterministic transcripts plus 75 exhaustive transcripts:
alphabets 2 through 16 at initial update counts 0, 15, 16, 31, and 32. Each
exhaustive transcript writes every symbol ascending and descending.
`tools/m2_transcript_matrix.py` contains a separate Python range decoder; it
does not call a C decoder or import the existing syntax decoder.

| Case | Coverage |
|---|---|
| `binary-sign` | 2-symbol alphabet, sign and bypass bits |
| `adaptive-three` | 3-symbol alphabet, eight updates and final CDF state |
| `base-br-five` | 5-symbol base/BR-style symbols and sign |
| `remainder-eight` | 8-symbol alphabet, literal and remainder bit strings |

The fixture records encoded bytes and SHA-256, every post-operation `tell`,
decoded values, and final adaptive CDFs. Each case is rerun with a roomy
buffer, the exact encoded size, and one byte less. Exact capacity must
reproduce the transcript; one byte less must return `ENC_OUT_OF_SPACE`.
`tests/range_writer_test.c` has a second, independent C decoder model and
checks the same 75 alphabet/boundary transcripts, including final CDF states,
against the production writer.

These arbitrary range transcripts are not complete AV2 image streams and
cannot be passed to image decoders. Their gate is agreement of both
independent transcript models with the C writer and the frozen fixture.

## Full streams

`fixtures/m2-full-streams/manifest.json` records source, OBU, and decoded
hashes for:

- M1 coefficient-free constant 8x8;
- M3 8x8 DC magnitudes 1, 5, and 8, reaching base, BR, and literal remainder;
- M3 16x16 four-block coding, exercising carried reconstruction and repeated
  adaptive CDF use;
- M4 8x8 DC plus vertical `(1,0)` AC with sign syntax.

The pinned AVM decoder is v1.0.0 revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`, executable SHA-256
`c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`.
Every committed OBU must regenerate byte-for-byte with the current encoder.
These complete streams, unlike the raw transcripts, are decoded by AVM and
the project decoder; outputs must match byte-for-byte and match the recorded
decoded hash. The multi-block case is intentionally lossy relative to its
source but has exact cross-decoder parity.

`make test` fails before tests if either decoder is missing or non-executable.
`make test-unit` is the only explicit validator-free path.

## Exhaustive native/model gate

The transcript matrix is supplemented by broader
`tests/range_writer_test.c` checks covering all 125 CDF parameter
combinations at counts 0/15/16/31/32, adaptation through count saturation,
0-through-32-bit literals, exact tell costs, every split point for compact
streams, five split positions for a 6,000-operation stress stream, long
`0xff` pending chains, carry propagation, deterministic finish, and
exact/one-byte-short/one-byte/zero capacities.

`tests/syntax_helpers_vectors.c` and the independent
`tools/syntax_reference.py` additionally cover raw UVLC/SVLC, two's-complement
signed literals, `ns`, subexponential coding and signed/unsigned reference
recentering, zero alignment and trailing bits, arithmetic signed literals and
`ns`, and general coefficient base/BR/remainder paths. Compact failure vectors
run every insufficient capacity while preserving bytes beyond the declared
span.

## Post-M2 limitations

M2 supplies entropy-writing primitives, not later codec breadth. Chroma,
general AC scans, additional transforms, broader frame/partition syntax,
search, filters, restoration, and rate control remain M3-and-later work.

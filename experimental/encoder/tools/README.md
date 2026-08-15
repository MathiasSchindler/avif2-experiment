# Encoder oracle harness

`encoder_oracle.py` decodes one experimental raw OBU with the pinned AVM
reference decoder and, by default, this project's decoder. It validates and
normalizes one packed planar frame, hashes both normalized outputs, compares
them byte-for-byte, and appends JSON Lines command logs and metrics.

```sh
python3 experimental/encoder/tools/encoder_oracle.py input.obu \
  --width 16 --height 16 --format yuv420p10le
```

Supported canonical formats are `gray8`, `yuv420p8`, `yuv444p8`,
`yuv420p10le`, and `yuv444p10le`. Decoder, work, log, and metrics paths are
configurable; use `--help` for all options. Missing tools, decoder failures,
unexpected output sizes or sample depths, and mismatches return nonzero.
`--reference-only` intentionally omits the project comparison.

Run the unit and existing-fixture smoke test with:

```sh
python3 -m unittest experimental/encoder/tests/test_encoder_oracle.py
```

`m1_syntax.py` is the executable AV2 v1.0.0 raw-bit model for the 8×8
constant-I400 M1 stream. It covers UVLC, ns, LEB128/OBU framing, exact
Sequence Header and CLK frame-header fields, forced partition traversal, and
symbol-decoder initialization/termination. Its focused tests are:

```sh
python3 -m unittest experimental/encoder/tests/test_m1_syntax.py
```

`m1_entropy.py` independently hash-checks the normative v1.0.0 entropy
attachments and decodes the `8d80` prefix. Its probability reduction uses the
normative `EC_PROB_SHIFT=7`, validates `Prob_Inc`, and models both normative
CDF adaptation and the C writer's explicitly inverse-CDF API. Tests round-trip
the native writer's fixed and adaptive golden vectors. The separate AVM
fixture uses a lossless configuration with no delta-Q symbol.

`m2_transcript_matrix.py` runs the native C range writer through four compact
syntax transcripts plus an exhaustive 2-through-16 alphabet matrix at initial
update counts 0/15/16/31/32. It independently decodes them in Python, checks
final adaptive CDF states and per-operation bit positions, and verifies
exact-capacity success plus one-byte-short exhaustion:

```sh
python3 experimental/encoder/tools/m2_transcript_matrix.py
```

The independent decoder in `range_writer_test.c` checks the same exhaustive
alphabet/update-boundary transcript family directly against the writer and
also covers every symbol, all 125 CDF parameter combinations, literals of 0
through 32 bits, split-buffer decoding, carry/`0xff` stress, deterministic
finish, and exact/short/zero capacities. Raw transcripts are intentionally
validated only by these range-decoder models; they are not image bitstreams.

`m2_full_stream_matrix.py` regenerates the tracked M1/M3/M4 streams and checks
them against their manifest. `--write` is the explicit fixture-maintenance
mode and additionally requires the hash-pinned AVM v1.0.0 decoder and the
project decoder:

```sh
python3 experimental/encoder/tools/m2_full_stream_matrix.py
python3 experimental/encoder/tools/m2_full_stream_matrix.py --write
```

`syntax_reference.py` independently decodes the freestanding raw and
range-coded helper vectors: UVLC/SVLC, signed literals, ns, subexp with
reference recentering, trailing bits, arithmetic ns, signed literals, and
general coefficient BR/remainder paths. The test exhausts small domains and
all output capacities of compact boundary transcripts:

```sh
python3 -m unittest experimental/encoder/tests/test_syntax_helpers.py
```

`generate_cdf_subset.py` verifies and extracts the exact AV2 v1.0.0 normative
CDF subset used by M1 and fixed-DC 8x8 grayscale M3 syntax. It emits the
tracked `src/normative_cdfs.inc`; production builds never read the
attachments. Reproduce or verify it with:

```sh
python3 experimental/encoder/tools/generate_cdf_subset.py
python3 experimental/encoder/tools/generate_cdf_subset.py --check
```

`m3_dc_model.py` is the executable 4x4/8x8 grayscale DC forward model. It
implements qindex lookup/dequantization, the normative local inverse,
prediction/clipping, canonical coefficient search, DC-only symbol transcripts,
selected CDF rows, lossless WHT/IDTX checks, and constant/ramp vectors:

```sh
python3 experimental/encoder/tools/m3_dc_model.py --vectors
python3 experimental/encoder/tools/m3_dc_model.py --transcript -21 8 255
```

`m3_q_sweep.py` runs the locked generated/prepared-crop qindex ladder, requires
exact local/AVM/project reconstruction parity, and writes aggregate JSON whose
SSE cannot increase as qindex decreases:

```sh
python3 experimental/encoder/tools/m3_q_sweep.py
```

`m4_ac_model.py` extends the executable model to an 8x8 DCT_DCT block with
DC plus scan-position-1 AC, including the normative scan, inverse transform,
canonical forward search, and EOB/base/BR/sign transcript:

```sh
python3 experimental/encoder/tools/m4_ac_model.py --transcript
```

`obu_inspect.py` independently parses canonical ULEB128 length prefixes and
AV2 OBU headers. Add `--bits` to include each payload as MSB-first bit groups:

```sh
python3 experimental/encoder/tools/obu_inspect.py --bits input.obu
```
## M3 benchmark

`m3_benchmark.py` orchestrates the locked gray-image M3 suite, both decoders,
integer distortion metrics, optional reconstruction parity, timing, and JSONL
reporting. See `../docs/m3-benchmark.md`.

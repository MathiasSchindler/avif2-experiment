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

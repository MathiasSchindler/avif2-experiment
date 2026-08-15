# M3 encoder benchmark

`tools/m3_benchmark.py` is the reproducible M3 gate runner. Its locked suite in
`metrics/fixtures.json` contains generated flat, horizontal-ramp, impulse, and
odd-edge I400 inputs. It also locks offset crops, without copying licensed
bytes, from the prepared square Pitti ceiling, landscape Baya weaver, and
portrait Elbrus luma planes. The source dimensions, source SHA-256, preparation
manifest, crop `(x,y)`, crop dimensions, and crop SHA-256 are all checked
before a temporary packed I400 crop is written below the ignored work tree.
The set covers 1×1, mixed odd/even rectangles, 15×15, even rectangles,
the legacy 16×16 path, and representative larger cases.

From the repository root:

```sh
make -C experimental/encoder
python3 experimental/encoder/tools/m3_benchmark.py
```

Each generated fixture writes one sorted JSON object. Each prepared crop runs
the locked qindex sweep `50,100,150,200` and writes one object per Q to
`experimental/encoder/results/m3-benchmark.jsonl`. The record includes tool
hashes, exact commands, exit codes, nanosecond timings, OBU bytes and exact
bits-per-pixel numerator/denominator. Successful streams are decoded by pinned
`build/reference/avm/avmdec` and the project decoder; their I400 bytes must be
identical. Encoder-reported local bytes and SSE must agree with the encoded
file and decoded reconstruction, and a nonzero workspace byte count is
mandatory. Source distortion is
represented without third-party packages as
integer SSE, a reduced integer MSE fraction, and rounded integer
`psnr_millidb` (`null` plus `psnr_infinite: true` for exact output).

To check an encoder-produced reconstruction, place `FIXTURE-ID.i400` in a
directory and pass `--reconstruction-dir DIR`. Missing optional
reconstructions are recorded rather than treated as failures.

Exit 3 (invalid dimension/profile metadata) and exit 5 (accepted input whose
encoding is not implemented) are recorded as `unsupported`; decoder rejection
is likewise recorded per fixture. Missing or hash-mismatched prepared inputs
are `blocked`, so a partial local corpus does not abort the rest of the suite.
The generated 17×15 case is mixed-axis production coverage.
Use repeated `--fixture ID` options for a subset, repeated `--qindex Q` options
to filter prepared sweeps, and `--append` to retain an existing JSONL file.

`tools/m3_q_sweep.py` is the stricter aggregate quality gate. Its locked
`metrics/m3-q-sweep.json` order is qindex `124,100,76,52,28,4`, from lower to
higher quality; these values share one lookup-table mantissa across power-of-two
scales. It checks exact local-model/pinned-AVM/project reconstruction equality
for generated fixtures and three hash-locked prepared 16×16 crops, then writes
`metrics/m3-q-sweep-results.json`. Aggregate SSE must be non-increasing as
qindex decreases:

```sh
python3 experimental/encoder/tools/m3_q_sweep.py
```

The bounded implementation also passed a separate 257-case deterministic
decoder-parity sweep: every constant 8×8 value from 0 through 255, plus one
four-quadrant 16×16 grid. The constant streams ranged from 19 to 23 bytes;
flat-128 was 19 bytes with infinite PSNR, while the exact four-quadrant grid
was 30 bytes with infinite PSNR. This exhaustive small-value check does not
replace the full M3 requirements for odd/larger dimensions and a multi-Q
quality sweep.

## Full prepared-image corpus gate

`tools/m3_prepared_corpus.py` covers complete prepared planes rather than
crops. Its generated `metrics/m3-prepared-corpus.json` inventories all 28
locked preparations, including every available format, source and preparation
manifest hash, dimensions, packed I400 stride/byte count, and output hashes.
The benchmark policy selects the minimum pixel-area preparation for each
source (currently all 14 `le896` planes), with source ID then preparation ID as
the deterministic tie/order rule.

```sh
python3 experimental/encoder/tools/m3_prepared_corpus.py --inventory-only
python3 experimental/encoder/tools/m3_prepared_corpus.py
```

Each selected full image is encoded twice at qindex 100. The selected complete
planes are 896x424, 896x448, 896x504, five 896x598 planes, two 598x896
planes, 896x672, and three 896x896 planes. Any inconsistent rerun is an error.
All 14 are required to pass; `expected_blocked`, `blocked`, and `error` are
gate failures. The current production boundary is 896 pixels per axis for
8-bit I400, not future arbitrary dimensions or chroma.
After encoder success, the OBU byte count/hash must repeat exactly and pinned
AVM plus the project validator must both accept it. Validator rejection is
always an error. Successful records additionally require byte-identical local,
AVM, and project reconstruction hashes and include integer SSE, reduced MSE,
PSNR, commands, exit codes, and nanosecond timings.

Prepared bytes remain ignored and are never copied into the manifest. A
missing or hash-mismatched selected plane is `blocked` and makes the gate fail.
Refresh the generated manifest only after intentional lock changes:

```sh
python3 experimental/encoder/tools/m3_prepared_corpus.py --write-manifest \
  --inventory-only
```

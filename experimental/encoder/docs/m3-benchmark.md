# M3 encoder benchmark

`tools/m3_benchmark.py` is the reproducible M3 gate runner. Its locked suite in
`metrics/fixtures.json` contains generated flat, horizontal-ramp, impulse, and
odd-edge I400 inputs. It also locks offset crops, without copying licensed
bytes, from the prepared square Pitti ceiling, landscape Baya weaver, and
portrait Elbrus luma planes. The source dimensions, source SHA-256, preparation
manifest, crop `(x,y)`, crop dimensions, and crop SHA-256 are all checked
before a temporary packed I400 crop is written below the ignored work tree.
The bounded set covers 1×1, mixed odd/even rectangles, 15×15, even rectangles,
and the full supported 16×16 limit.

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
identical. Encoder-reported local bytes, SSE, and workspace must also agree
with the encoded file and decoded reconstruction. Source distortion is
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
The generated 17×15 case remains explicit out-of-bound unsupported coverage.
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

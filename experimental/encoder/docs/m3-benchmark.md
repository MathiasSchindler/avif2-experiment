# M3 encoder benchmark

`tools/m3_benchmark.py` is the reproducible M3 gate runner. Its locked suite in
`metrics/fixtures.json` contains generated flat, horizontal-ramp, impulse, and
odd-edge I400 inputs. It also points (without copying licensed bytes) to the
prepared 896×896 Pitti ceiling, 896×598 landscape Baya weaver, and 598×896
portrait Elbrus luma planes and their preparation manifests.

From the repository root:

```sh
make -C experimental/encoder
python3 experimental/encoder/tools/m3_benchmark.py
```

Each fixture writes one sorted JSON object to
`experimental/encoder/results/m3-benchmark.jsonl`. The record includes tool
hashes, exact commands, exit codes, nanosecond timings, OBU bytes and exact
bits-per-pixel numerator/denominator. Successful streams are decoded by pinned
`build/reference/avm/avmdec` and the project decoder; their I400 bytes must be
identical. Source distortion is represented without third-party packages as
integer SSE, a reduced integer MSE fraction, and rounded integer
`psnr_millidb` (`null` plus `psnr_infinite: true` for exact output).

To check an encoder-produced reconstruction, place `FIXTURE-ID.i400` in a
directory and pass `--reconstruction-dir DIR`. Missing optional
reconstructions are recorded rather than treated as failures.

Exit 3 (invalid dimension/profile metadata) and exit 5 (accepted input whose
encoding is not implemented) are recorded as `unsupported`; decoder rejection
is likewise recorded per fixture. Missing or hash-mismatched prepared inputs
are `blocked`, so a partial local corpus does not abort the rest of the suite.
The generated 16×8 ramp is a supported rectangular parity gate; 17×15 and the
large prepared corpus remain explicit unsupported/blocked coverage.
Use repeated `--fixture ID` options for a subset and `--append` to retain an
existing JSONL file.

The bounded implementation also passed a separate 257-case deterministic
decoder-parity sweep: every constant 8×8 value from 0 through 255, plus one
four-quadrant 16×16 grid. The constant streams ranged from 19 to 23 bytes;
flat-128 was 19 bytes with infinite PSNR, while the exact four-quadrant grid
was 30 bytes with infinite PSNR. This exhaustive small-value check does not
replace the full M3 requirements for odd/larger dimensions and a multi-Q
quality sweep.

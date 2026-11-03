# Native decoder performance baseline

Measured on 2026-11-03 at `b7cd879aab6291efa9d194e330e8c44ca72c49d2`.
This is a local performance investigation, not a new capability or release gate.
Production decoder code and default build flags were not changed.

## Conclusions

1. **Start with GDF for ordinary lossy photographs.** It accounts for roughly
   44-67% of sampled execution in the representative 8-bit photographs.
   A portable C locality/reuse prototype improved whole-decode speed by
   1.34-1.62x on the three matched Baya formats.
2. **There is not one bottleneck for every stream.** Lossless photographs and
   coefficient-heavy 10-bit fixtures move the cost toward range decoding,
   coefficient scanning, and inverse transforms.
3. **Test `-O3` as a separate build decision.** With no source changes or
   architecture-specific flags, its median per-case speedup was 1.15x across
   the 88 photographic cases. It is not equally beneficial everywhere.
4. **Do not globally enable the scan-table prototype.** It helped the
   coefficient-heavy 10-bit I444 case by 1.16x, but slowed several photographs
   by 2-4%. Its applicability needs refinement.
5. **Do not start by rewriting allocation or parsing for large photographs.**
   Their profiles are dominated by pixel kernels. Setup does matter for tiny
   images, and process startup overwhelms their actual decode time.

## Method and coverage

- Apple M4 Max, ARM64, 16 logical CPUs, 64 GiB RAM; macOS 27.2 (26B123).
- Homebrew Clang 22.1.8. The baseline uses the normal freestanding core flags:
  `-O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables
  -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -nostdinc`,
  strict C11 warnings, and the repository include paths. Debug information was
  added with `-g`; diagnostics remained disabled.
- Single-threaded, hosted native execution. No sanitizer instrumentation,
  explicit SIMD, `-march=native`, LTO, or AVM code was added.
- The harness includes the existing POSIX frontend with its `main` renamed and
  calls its actual [raw OBU decode pipeline](../../src/platform/posix/decoder.c#L106).
  Input is read once before timing. Parsing, workspace/output allocation,
  workspace initialization, reconstruction, filtering, final planar copying,
  and workspace/output release are included in every iteration.
- One untimed warm decode writes verification output. Every output is checked
  against its recorded SHA-256; locked encoded inputs and manifests are checked
  too. Output pixels are also consumed during timed iterations.
- Adaptive power-of-two batches run for at least 30 ms, followed by five
  measured batches per case. Reported times are medians, in milliseconds per
  image. MPix/s counts **visible luma pixels**, not total YUV samples.
  Each sample is a batch total divided by its iteration count; reported
  min/max values are batch-average ranges, not individual-image tail latency.
- Cases run serially in a deterministic shuffled order. Median within-case
  `max/min - 1` was 1.43%; the largest was 14.89%. Median wall/process-CPU time
  ratio was 1.00008. This is a normally scheduled machine, not an isolated
  laboratory or a cold-cache/disk benchmark.

All **179 cases** decoded with the expected output:

| Corpus | Cases |
|---|---:|
| Pinned photographic 8-bit references | 88 |
| Pinned AVM-default 8-bit references | 28 |
| Synthetic 10-bit references | 46 |
| Spatial/profile feature fixtures | 16 |
| Retained current-oracle Baya regression | 1 |

Pinned photographic/default references use AVM
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.
The separate retained current-oracle fixture is measured as its own case,
not treated as the same encoded stream.

The [complete baseline CSV](performance-baseline.csv) records every case,
format, dimensions, encoded size, median/range, throughput, and leaf counts.
QP denotes an encoder setting/recipe label where available, **not necessarily
the bitstream's base qindex**.

### Coverage limitations

The 8-bit photographs cover 896- and 1440-long-edge preparations, up to
1440x1440. Photographic I400/I444 cases use the 896 preparation.
The 10-bit corpus is synthetic: 16x16, 17x15, 65x63, and 96x80.
There are no large photographic 10-bit measurements here; do not infer their
throughput by comparing these tiny fixtures with 8-bit photographs.
No 12-bit, video/inter-frame, cross-platform, peak-RSS, or energy claims are made.
The public container/query/binding API and preview generation are not part of
the core OBU timing.

## Baseline timings

These are ranges of **case medians**, not measurement error bars.

| Format | Corpus used in this row | Cases | Decode range, ms | Median MPix/s |
|---|---|---:|---:|---:|
| 8-bit I400 | Photographic | 14 | 36.729-140.873 | 6.75 |
| 8-bit I420 | Photographic | 60 | 44.755-419.593 | 6.14 |
| 8-bit I444 | Photographic | 14 | 66.452-229.509 | 4.08 |
| 10-bit I420 | Synthetic | 25 | 0.026-1.263 | 8.33 |
| 10-bit I444 | Synthetic | 21 | 0.049-2.840 | 3.70 |

The other 45 cases are in the complete CSV. Selected examples:

| Case | Visible size | Decode, ms |
|---|---:|---:|
| Baya, I400, QP152 | 896x598 | 74.966 |
| Baya, I420, QP152 | 896x598 | 87.785 |
| Baya, I444, QP152 | 896x598 | 114.169 |
| Baya, I420, QP96 | 1440x960 | 207.045 |
| Pitti ceiling, I420, QP152 | 1440x1440 | 419.593 |
| PC-Wiener restoration fixture | 896x598 | 113.636 |
| Nonuniform multi-tile-group fixture | 320x320 | 20.863 |
| High-detail 10-bit I420, QP128 | 96x80 | 1.263 |
| High-detail 10-bit I444, QP128 | 96x80 | 2.840 |
| Filtered boundary 10-bit I420, QP128 | 65x63 | 0.587 |

### What the comparisons establish

- Across 14 matched source/size/QP triplets, median I400/I420 time ratio is
  **0.837**, and I444/I420 is **1.370**. Monochrome does not remove most of the
  work because the dominant GDF processing is on luma. I444 increases chroma
  filtering/reconstruction cost; it does not double every stage.
- Baya 896 I420 QP96 is 27,884 encoded bytes and takes 87.693 ms; QP152 is
  5,454 bytes and takes 87.785 ms. Fewer compressed bytes alone do not predict
  faster decoding when pixel filtering dominates.
- Pastel-chalks 896 I420 QP0 takes 76.276 ms versus 86.909 ms at QP255.
  The QP0 profile is entropy-heavy and has no sampled GDF/CDEF work. This is
  why a higher-quality/larger stream is not automatically slower.
- Across 28 matched default-versus-photographic recipe pairs, the median time
  ratio is 0.997, with individual ratios 0.863-1.096. There is no systematic
  large default-recipe penalty in this retained corpus. This does not isolate
  the cost of any one encoder tool.

## Where execution time goes

Twelve sustained baseline runs were sampled for five seconds each with
`/usr/bin/sample PID 5 1 -file OUTPUT`, totaling **51,018 main-thread samples**.
The following are approximate **self/top-of-stack** shares, not overlapping
inclusive call-tree percentages. Small functions below the profiler's
five-sample listing threshold are omitted. Optimized inlining and incomplete
unwind information limit attribution; use these as priorities, not exact
instruction-level accounting.

| Stream | GDF | CDEF block | 1D inverse transform | Coeff. scan | Range functions |
|---|---:|---:|---:|---:|---:|
| Baya 1440 I420 QP96 | 54.5% | 15.1% | 12.3% | 0.9% | 3.9% |
| Baya 896 I400 QP152 | 67.3% | 14.2% | 9.3% | 0.2% | 0.7% |
| Baya 896 I444 QP152 | 43.8% | 29.4% | 13.3% | 0.2% | 0.7% |
| Pitti 1440 I420 QP152 | 47.6% | 14.6% | 13.6% | 2.1% | 4.8% |
| Pastel 896 I420 QP0 | not listed | not listed | 0.3% | 8.2% | 41.7% |
| Pastel 896 I420 QP255 | 58.7% | 20.9% | 13.3% | not listed | not listed |
| High-detail 10-bit I420 | not listed | not listed | 15.4% | 11.2% | 34.9% |
| High-detail 10-bit I444 | not listed | not listed | 29.4% | 18.7% | 31.2% |
| Boundary 10-bit I420 | 74.4% | not listed | 12.4% | 0.2% | 1.3% |
| PC-Wiener fixture | 41.0% | 14.6% | 14.7% | 0.1% | 0.8% |
| Nonuniform tile groups | 49.2% | 15.4% | 15.7% | 0.7% | 3.4% |
| Tiny 10-bit I420 QP148 | not listed | not listed | 23.7% | 4.0% | 12.2% |

Additional profile-specific observations:

- PC-Wiener filtering and classification consume another 10.3% and 5.5% of
  their fixture's samples. It matters when enabled, but is not the first
  optimization for the ordinary photographic corpus.
- Tiny 10-bit I420 spends 8.5% in CDF initialization and 6.5% in memory fill;
  setup is meaningful there.
- Lossless Pastel spends 9.3% in memory fill/copy combined. Corresponding
  shares in the sampled lossy photographs are approximately 0.5-1.5%.
- Neither compressed-file I/O nor PNG encoding is present in these profiles.
  They cannot explain the measured core-kernel hotspots.

## Isolated experiments

Each variant decoded all 179 cases with the same expected output hashes:
**716 successful image/hash checks including the baseline**.
The GDF variant also passed the existing
[filter unit suite](../../tests/filter_unit.c), including U16, stripe sources,
control maps, and alias validation. The scan prototype passed 73,548 direct
old/new comparisons, including rectangular, non-table, and out-of-range scans.
These are experimental checks, not a substitute for the full release gates.

Ten selected cases were then repeated in three deterministic shuffled,
interleaved passes across all four binaries, with three measured batches per
pass: **nine measurements per case/variant**. Each batch targets at least
15 ms, or one complete decode for slower images.
The [experiment CSV](performance-experiments.csv) records medians, ranges,
and speedups. These repeated timings deliberately replace the earlier
unpaired matrix numbers when discussing individual improvements.

| Case | Baseline, ms | GDF locality/reuse, ms | GDF speedup | Scan-table speedup | O3-core speedup |
|---|---:|---:|---:|---:|---:|
| Baya I400 | 75.891 | 46.992 | 1.62x | 0.96x | 1.16x |
| Baya I420 | 88.635 | 59.419 | 1.49x | 0.97x | 1.15x |
| Baya I444 | 116.065 | 86.425 | 1.34x | 0.98x | 1.14x |
| Pitti I420 | 422.596 | 311.888 | 1.35x | 0.98x | 1.15x |
| Lossless Pastel I420 | 77.241 | 77.171 | 1.00x | 1.06x | 1.02x |
| High-detail 10-bit I420 | 1.291 | 1.339 | 0.96x | 1.07x | 1.04x |
| High-detail 10-bit I444 | 2.805 | 2.902 | 0.97x | 1.16x | 1.08x |
| Filtered boundary 10-bit I420 | 0.582 | 0.309 | 1.89x | 1.02x | 1.23x |
| Tiny 10-bit I420 | 0.031 | 0.030 | 1.00x | 1.04x | 1.12x |
| PC-Wiener fixture | 113.529 | 88.211 | 1.29x | 0.96x | 1.13x |

Speedup is baseline time divided by variant time; **below 1.0 is a regression**.
The variants were tested independently; their speedups must not be multiplied
to predict a combined result.

### GDF: the strongest measured portable opportunity

In [the GDF block kernel](../../src/decoder/gdf.c#L193), one 4x4 output block
repeatedly resolves the same nearby samples, tile bounds, and stripe-specific
choice between CDEF and deblocked input.

The prototype:

1. Materializes the exact 16x16 source neighborhood once, using the existing
   boundary/stripe resolver.
2. Uses that local array for gradients and symmetric kernel taps.
3. Retains the 16 class-region directional gradient sums rather than
   recomputing them for each output pixel.

At source level this reduces boundary-resolved sample queries from 1,024 to
256 per block and directional 4x4 sum evaluations from 80 to 16. There are no
intrinsics, new global mutable state, allocations, or changed rounding rules.
The separate U8/U16 scaling remains intact.

Tradeoffs and unresolved work:

- Clang's static stack-usage report grows from 528 to 800 bytes for the U8
  block kernel and from 544 to 1,040 bytes for U16. This is per-function stack,
  not a measurement of total thread peak stack.
- The high-detail 10-bit cases, which had no listed GDF samples, nevertheless
  regress by about 3.5-3.7% in interleaved runs. The cause is **not established**;
  do not call the prototype universally beneficial or blame code layout
  without further evidence.
- Before integration, resolve those regressions and run the complete
  diagnostics-on/off, malformed-input, stack/resource, and filter parity gates.
  Tile boundaries, odd visible crops, stripe source selection, and aliases
  must retain their current behavior.

### Coefficient scanning: selective, not universal

[The diagonal scan helper](../../src/decoder/coeff_context.c#L14) walks from
diagonal zero for each requested scan index. Coefficient parsing invokes it
repeatedly, which becomes expensive for dense blocks.

A prototype substitutes immutable tables for the 16 combinations of
4/8/16/32 coefficient widths and heights, retaining the original fallback.
The position values occupy 7,200 bytes, plus a small offset table and dispatch.
The 10-bit high-detail I444 improvement is real in the interleaved run, but
the generic dispatch/table tradeoff is unfavorable for several photographic
and restoration cases.

Next investigate a scan iterator or per-transform selected scan pointer,
retaining cheap paths for early indices, column scans, and linear scans.
Do not repeatedly rediscover geometry in the innermost coefficient loop.
Preserve unsupported/out-of-range helper behavior rather than removing
validation to manufacture a speedup.

### Compiler settings

The O3 experiment changes only the core optimization level; its frontend
stays at O2. In the full matrix, median per-case speedups are 1.15x for the
photographic suite and 1.11x for the synthetic 10-bit suite.
The Mach-O `__TEXT` segment grows from 770,048 to 835,584 bytes
(65,536 bytes, approximately 8.5%; segment sizes include alignment).
This is a speed/size decision to validate separately, not a reason to enable
unsafe arithmetic flags or replace the size-oriented freestanding targets.

## Further opportunities, in priority order

The following are **hotspot-backed proposals, not measured speedup claims**:

| Priority | Work | Evidence and approach | Portability / risk |
|---|---|---|---|
| 1 | Finish GDF locality/reuse | Measured large wins above; resolve unaffected-path regressions and stack tradeoff first | Portable C; medium integration risk |
| 2 | Evaluate O3 for the hosted speed-oriented build | Measured benefit with no decoder source changes; retain explicit size-oriented configurations | Compiler optimization, not ISA-specific source; low-medium risk |
| 3 | Specialize CDEF's interior kernel | 14-29% of photographic samples; hoist direction offsets and damping/strength adjustment, skip zero-strength tap families, separate interior from boundary handling | Portable C; medium risk around tiles, clipping, and in-place semantics |
| 4 | Improve dense/sparse inverse transforms | `recon_transform_1d` is 9-16% on sampled photographs and up to 29% on high-detail 10-bit; select fixed kernel/size once, improve matrix access, then consider exact sparse/DC kernels | Portable C first; high arithmetic/rounding risk |
| 5 | Optimize range normalization/refill for entropy-heavy streams | Range functions are 31-42% on the dense/lossless cases; replace repeated log2 shifting and per-bit span rescans with bounded log2 and a byte/word refill path | Portable fallback plus optional compiler builtins; high truncation/state risk |
| 6 | Refine scan reuse | 8-19% scan shares in dense/lossless cases; measured selective wins, but generic lookup regresses photographs | Portable; medium risk and modest read-only data cost |
| 7 | Reduce setup/reset work for tiny images or dense lossless blocks | CDF initialization and memory fill are visible there, not a dominant large-photo cost | Portable; preserve all initialized-state and ownership guarantees |
| Later | SIMD and parallel filtering | GDF/CDEF/transform kernels remain natural candidates after eliminating redundant work; no SIMD or multithread speedup measured here | ISA-specific implementation or portable scheduling with halo/dependency care |

Supporting code facts:

- [CDEF's constraint helper](../../src/decoder/cdef.c#L252) derives the
  damping adjustment from strength; its tap path also checks boundaries for
  every neighbor. Hoisting must preserve zero-strength and min/max rules.
- [The inverse transform](../../src/decoder/inverse_transform.c#L688) uses
  dense size-by-size integer matrix products. Whole-block all-zero bypasses
  already exist: simply proposing another zero-block shortcut would duplicate
  existing work. Sparse/DC improvements must account for secondary transforms
  and exact intermediate rounding/clamping.
- [Range decoding](../../src/decoder/range.c#L20) obtains input one bit at a
  time, locating the source span afresh. Baseline ARM64 disassembly confirms
  that normalization also retains a repeated shift/branch log2 loop; it was
  not automatically converted to a count-leading-zeros instruction.
  Refilling must preserve tell/available-bit accounting, span boundaries,
  truncation, and fail-closed error behavior. Do not remove CDF checks.

For scale, if GDF is 55% of a decode and its kernel becomes twice as fast,
Amdahl's law predicts only about **1.38x overall**, not 2x.
After a GDF improvement, reprofile: CDEF and inverse transforms become larger
fractions of the remaining time.

## CLI startup and output are separate costs

Seven fresh subprocesses per case/output mode, warm filesystem cache, normal
buffered writes, no `fsync`. Times include parent-side launch/wait overhead,
input reading, decoding, and output writing. Raw outputs were hash-checked;
PNG outputs were checked for dimensions and repeatability.

| Case | In-process decode, ms | Raw CLI median, ms | PNG CLI median, ms |
|---|---:|---:|---:|
| Odd I400, 17x15 | 0.073 | 5.786 | 5.606 |
| Baya I420, 896x598 | 87.785 | 91.878 | 99.733 |
| Baya I444, 896x598 | 114.169 | 123.914 | not measured |
| Pitti I420, 1440x1440 | 419.593 | 417.633 | not measured |
| High-detail 10-bit I420, 96x80 | 1.263 | 9.768 | unsupported |
| High-detail 10-bit I444, 96x80 | 2.840 | 9.503 | unsupported |

Tiny images are overwhelmingly process-bound when run one file per CLI
invocation. A batch or long-lived host can avoid that startup cost, without
claiming a faster entropy decoder. PNG adds conversion/encoding work that is
not part of OBU decoding. Do not subtract timings from separate runs to infer
an exact fixed overhead: the small negative Pitti difference demonstrates
normal between-run variability.

## Reproduction

The accompanying local evidence bundle contains the harnesses, experimental
sources/generator, complete JSON samples, profiles, and this run's environment
metadata. Extract it to a scratch directory and run commands from the
repository root at the recorded revision. It is research tooling, not a new
production frontend or a supported benchmark API.

For the baseline, with `EVIDENCE` set to the extracted directory:

```sh
flags='-std=c11 -Wall -Wextra -Wpedantic -Werror -pipe -O2 -g -ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -nostdinc -Iinclude -Isrc -Isrc/shared'
make -j4 NATIVE_BUILD_DIR=build/performance-o2 FREESTANDING_CFLAGS="$flags" decoder
clang -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g \
  -Iinclude -Isrc "$EVIDENCE/perf_decode.c" \
  build/performance-o2/libav2heif-core.a -o build/performance-o2/perf-decode
python3 "$EVIDENCE/perf_matrix.py"
python3 "$EVIDENCE/perf_profiles.py"
python3 "$EVIDENCE/perf_cli.py"
```

For the isolated variants, the bundle also provides
`sh "$EVIDENCE/perf_build_variants.sh"` (forces recompilation), followed by:

```sh
for variant in gdf scan o3; do
  python3 "$EVIDENCE/perf_matrix.py" \
    "build/performance-$variant/perf-decode" "performance-$variant"
done
python3 "$EVIDENCE/perf_paired.py"
```

Use fresh build directories when changing flags; the object rules do not
track every flag change. Do not run `make clean` between building and measuring:
it removes these performance build directories while preserving AVM oracles.
All experiments and timings should remain serial to avoid CPU contention.

## A3 entropy-loop implementation

A3 implements the normalization/refill priority above, compared against A2
(`369db20`). The historical baseline and prototype measurements above are
unchanged; these are new matched measurements, not comparisons between runs
made at different revisions or with different optimization flags.

The implementation adds bounded byte-chunk input extraction, contiguous range
refills, batched bypass input with exact scalar exhaustion behavior, fused CDF
validation/search, and a checked binary-CDF path. CDF checks remain enabled in
production. Range normalization uses the shared floor-log2 helper's guarded
compiler builtin, with defined zero handling and the existing portable C
fallback. ARM64 disassembly confirms a leading-zero instruction replaces the
original repeated shift loop. The first byte-chunk/CDF-fusion version alone
showed no measurable whole-image gain; the results below include the subsequent
normalization and common-case refinements.

### Matched method

- Same Apple M4 Max and Clang 22.1.8, normal freestanding `-O2` core flags,
  diagnostics disabled. No LTO, explicit SIMD, architecture-specific flags,
  threading, or changes to the default optimization level.
- The harness calls the actual shared raw-OBU CLI decode service. Input loading
  is outside timing; parsing, allocation, initialization, entropy decoding,
  reconstruction, filters, planar copying, and release are inside it. No CLI
  subprocess startup or output-file writing is timed.
- One warm decode, calibration for at least 30 ms, and seven measured batches
  per process. Nine paired rounds alternate before/after and after/before,
  using the same deterministically shuffled case order within each pair.
  Measurements run serially without concurrent builds or tests.
- Times below are medians of the nine per-process batch medians. Speedup is
  the median of the nine paired before/after ratios, so it need not equal the
  ratio of the two displayed medians. Intervals are approximate 95% percentile
  bootstrap intervals from 5,000 resamples of the paired ratios, not latency
  bounds or cross-machine guarantees.
- Both binaries produce the recorded pixel SHA-256 for each measured case
  before timing. Complete subsequent regression checks preserve all 179 raw
  hashes, 176 admitted canonical public-API hashes, and 14 CLI contracts in
  both diagnostic configurations, alongside sanitizer suites and fuzzing.

### Whole-decode results

| Case | Before, ms | After, ms | Paired speedup | Approx. 95% interval |
|---|---:|---:|---:|---:|
| Pastel, 896 I420 QP0 (lossless) | 75.643 | 65.267 | 1.156x | 1.146-1.164x |
| Pastel, 896 I420 QP48 | 76.234 | 66.193 | 1.149x | 1.140-1.151x |
| High-detail 10-bit I420, 96x80 QP128 | 1.293 | 1.139 | 1.136x | 1.092-1.171x |
| High-detail 10-bit I444, 96x80 QP128 | 2.808 | 2.553 | 1.100x | 1.074-1.108x |
| Baya, 896 I420 QP152 | 86.816 | 83.883 | 1.035x | 1.031-1.055x |
| Baya, 896 I444 QP152 | 114.323 | 108.665 | 1.051x | 1.050-1.059x |
| Pastel, 896 I420 QP255 | 86.217 | 82.823 | 1.043x | 1.038-1.044x |
| Baya, 1440 I420 QP96 | 207.399 | 196.799 | 1.055x | 1.048-1.059x |

These establish a measurable whole-image benefit on the lossless/high-bitrate
and coefficient-heavy cases, without a measured regression in the selected
filter-heavy controls. They do not measure the individual contribution of each
source change, establish the speed of the portable fallback on other compilers,
or imply the same gain across the whole corpus. GDF/CDEF/transform work remains
unchanged and is still important for ordinary lossy photographs.

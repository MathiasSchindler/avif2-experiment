# AV2 8-bit 4:2:0 coding opportunities

> [!NOTE]
> **Status: active encoder research record.** Decoder admission is authoritative
> in the [capability matrix](../capabilities.md); this file records experiments,
> measured opportunities, and proposed evidence expansion.

## Purpose

This document records AV2 and pinned-AVM options that may improve compression
or reconstructed quality for one-frame 8-bit 4:2:0 images. It separates
encoder-only improvements from bitstream features that require decoder work.

The effects below are estimates, not measured results for this repository.
They should be treated as hypotheses until a rate-distortion experiment records
encoded size, encode time, and decoded quality across the photographic and
screen-content corpora. A feature can improve one quality metric while reducing
another, so PSNR, SSIM, and at least one perceptual metric should be reported.

## Current baseline

The photographic reference generator uses pinned AVM v1.0.0 with the following
common arguments, in addition to the input and output paths:

```text
--debug
--codec=av2
--obu
--passes=1
--limit=1
--good
--cpu-used=9
--threads=<detected-logical-cores>
--row-mt=1
--profile=<0-for-I400/I420-or-1-for-I444>
--bit-depth=8
--input-bit-depth=8
--force-video-mode=0
--film-grain-test=0
--enable-lcr=0
--enable-operating-point-sets=0
--enable-atlas=0
--enable-intrabc=0
--enable-restoration=0
--end-usage=q
--qp=<case-QP>
--test-decode=fatal
--quiet
```

I400 adds `--monochrome`. I420 adds:

```text
--width=<width>
--height=<height>
--i420
--input-chroma-subsampling-x=1
--input-chroma-subsampling-y=1
--chroma-sample-position=unspecified
```

I444 adds:

```text
--width=<width>
--height=<height>
--i444
--input-chroma-subsampling-x=0
--input-chroma-subsampling-y=0
```

After encoding, the generator removes any content-interpretation OBU and
requires the normalized ledger to contain exactly a temporal delimiter,
Sequence Header, and closed-loop key-frame OBU.

The generator explicitly disables IntraBC and restoration, and prevents the
film-grain test-vector mode. It does not currently pass explicit TCQ,
parity-hiding, quantization-matrix, AQ, or delta-Q switches. The current
`cpu-used=9` output stays inside the production encoding profile, but future
experiments must pin those syntax choices instead of relying on AVM defaults or
speed policy. That production profile still excludes TCQ, parity hiding,
quantization matrices, segmentation, film grain, and restoration. This is an
encoder-admission policy, not a complete statement of decoder capability: the
decoder now supports admitted subsets of TCQ, predefined quantization matrices,
ALT_Q segmentation with delta-Q, and restoration in addition to IntraBC.

For the currently validated AVM-generated corpus tuples, the decoder supports
non-DCT transforms, directional prediction, MRL, CfL/MHCCP, luma FSC, IntraBC,
transform skip, palette prediction, always-on single-picture TCQ, deblock,
CDEF, CCSO, predefined non-segmented quantization matrices, 8-bit luma
PC-Wiener restoration, 8-bit `RESTORE_WIENER_NONSEP` on any I420 plane with
either frame-level or per-unit filters and one or more luma filter classes,
and GDF. The target baseline also has exact
pinned-AVM parity through transform partitioning, Intra-DIP, directional IBP,
MRL, secondary transforms, and the complete filter pipeline. This is not a
claim of exhaustive feature-family support: chroma FSC remains unaudited,
palette evidence covers the two observed luma cases, frame-adaptive video TCQ
remains deferred, custom matrix OBUs and segmented multi-matrix selection remain
unsupported, and switchable restoration, other chroma formats, and all 10-bit
restoration remain fail-closed. Support for a feature path does not imply support for every
combination of geometry, coefficient profile, transform, and surrounding
syntax.

## Effect scale

| Estimate | Meaning |
| --- | --- |
| Small | Likely measurable but not a primary reason to change the profile. |
| Medium | Plausibly material for a representative subset of images. |
| Large | Potentially dominant for the named content class, but not general. |
| Content-specific | Expected benefit depends strongly on image structure. |

No percentage or BD-rate estimate is assigned before a controlled experiment
exists. AVM exposing a switch proves availability, not a gain for this corpus.

## Summary

| Opportunity | Estimated effect | Current compatibility | Decoder work |
| --- | --- | --- | --- |
| Slower AVM search | Small to medium compression improvement at equal quality; much slower encoding | Expected to remain compatible when excluded syntax is explicitly disabled | None; validate emitted streams |
| Stronger ordinary trellis optimization | Small compression or quality improvement | Expected to use existing coefficient syntax | None; validate emitted streams |
| IntraBC and screen-content tuning | Medium to large for text, diagrams, and repeated screen regions; usually small for photographs | IntraBC, transform skip, and observed palette paths are supported | No new core feature; broaden end-to-end evidence |
| Trellis-coded quantization (TCQ) | Small to medium compression improvement | Supported for pinned AVM's always-on single-picture subset | Broaden Q-context, transform, bit-depth, malformed-input, and durable platform evidence; defer frame-adaptive video mode |
| Loop restoration | Small to medium quality or compression improvement, especially near visible artifacts | Supported for 8-bit luma PC-Wiener and 8-bit Wiener-nonsep on any I420 plane, with frame-level or per-unit filters and multi-class luma; switchable restoration and 10-bit remain fail-closed | Add durable Wiener-nonsep fixtures, measure corpus impact, and broaden modes or formats only from observed demand |
| Quantization matrices | Small to medium perceptual-quality improvement; metric-dependent | Predefined matrices are supported for non-segmented pictures; custom QM OBUs remain fail-closed | Broaden levels, formats, transforms, malformed-input, and platform evidence; add custom OBU state only from observed demand |
| Adaptive quantization, delta-Q, and segmentation | Medium perceptual-quality improvement on spatially varied images | Pinned AVM `aq-mode=1` ALT_Q segmentation and `deltaq-mode=1`/`2` are supported for closed-loop key pictures | Broaden bit depths, QPs, malformed-input vectors, and platform evidence; retain fail-closed extended and non-ALT_Q features |
| Film-grain synthesis | Large compression opportunity for genuinely noisy or grainy images; neutral or harmful for clean images | Unsupported and not sample-preserving | Grain metadata parsing, deterministic synthesis, output semantics, workspace, and conformance vectors |

## Encoder-only opportunities

### Slower AVM search

The existing `--cpu-used=9` setting is AVM's fastest good-quality mode. Lower
settings search more partition, prediction, transform, and coefficient choices.
This can produce a better rate-distortion result without intentionally adding
a new bitstream tool, at the cost of substantially longer encoding. TCQ
requires `--cpu-used=1` or lower in pinned AVM, so `cpu-used=1` is the target
for a future high-effort still-image profile; it is not yet the production
setting.

A local two-run wall-clock probe used the 896 x 598 Pastel I420 source at QP
152, 16 threads, and the otherwise pinned TCQ-off recipe. Median encode times
were 19.86 seconds at `cpu-used=9`, 20.44 seconds at 5, 90.99 seconds at 2,
and 158.08 seconds at 1. Thus the TCQ-capable setting was approximately 7.96
times slower than the current setting for this image. Encoding is normally an
offline, one-time cost, so this does not rule out the profile, but it makes
artifact reuse and resumability requirements rather than conveniences.

Keep non-baseline syntax explicit in the production profile and in every slower
search experiment:

```text
--enable-tcq=0
--enable-parity-hiding=0
--enable-restoration=0
--enable-qm=0
--aq-mode=0
--deltaq-mode=0
--enable-intrabc=0
```

AVM disables TCQ automatically at `cpu-used >= 2`, but the explicit option
protects the experiment against changed defaults. TCQ and restoration remain
disabled in this experiment to isolate ordinary search and keep its output in
the production encoding profile, not because their admitted decoder subsets are
absent. Disabling TCQ exposes the separate parity-hiding Sequence Header flag,
which must also be disabled because the decoder does not support its coefficient
syntax. Other unsupported features should likewise be pinned rather than
inferred from current defaults.

Decoder implementation steps:

1. No decoder algorithm should be required.
2. Encode representative photographic and synthetic cases at fixed QP for
   `cpu-used` 9, 5, and 2.
3. Inspect each OBU with the AVM census and reject any newly emitted unsupported
   syntax or unaudited feature tuple.
4. Require exact output parity between pinned `avmdec` and the project decoder.
5. Record encoded bytes, encode time, PSNR, SSIM, and a perceptual metric before
   changing the reference recipe.

### Stronger ordinary trellis optimization

AVM's `--enable-trellis-quant` controls encoder-side optimization of ordinary
quantized coefficients. This is distinct from the AV2 TCQ bitstream feature.
Mode 1 performs trellis optimization in all encoding stages; the current
default mode 3 limits where it runs.

Estimated effect: small, with potentially high encoding cost. It should be
tested after lowering `cpu-used`, because their searches interact.

Decoder implementation steps:

1. No new syntax or reconstruction algorithm is expected.
2. Compare modes 1, 2, and 3 with `--enable-tcq=0` held constant.
3. Census the results and run exact AVM/project-decoder parity before treating
   the option as compatible.

### IntraBC and screen-content tuning

AVM exposes `--enable-intrabc`, `--enable-intrabc-ext`, and
`--tune-content=screen`. IntraBC reuses reconstructed regions from the same
image and is most useful for repeated text, icons, line art, and interface
elements. Palette prediction and transform skip are related screen-content
paths already observed in the artifact corpus.

Estimated effect: medium to large on screen content, but small or negative on
most natural photographs after signaling and search costs. It should use a
separate screen-content preset rather than become an unconditional photographic
default.

IntraBC was outside the accepted profile when this opportunity was first
recorded. The decoder already contained the core prediction path, and the work
below established the missing transform mapping and end-to-end evidence needed
to admit the observed 8-bit I420 subset. The production photographic generator
still disables IntraBC by policy; that does not make the decoder path
unsupported.

An initial controlled photographic probe encoded all 14 `le896` I420 inputs at
QP 96 and 152 with the current pinned AVM binary and identical commands except
for `--enable-intrabc=0` versus
`--enable-intrabc=1 --enable-intrabc-ext=1`. TCQ, parity hiding, restoration,
quantization matrices, AQ, and delta-Q were explicitly disabled in both arms.
IntraBC reduced aggregate OBU bytes by 0.722% at QP 96 and 1.241% at QP 152.
Mean PSNR changed by +0.000944 dB and -0.005529 dB respectively; mean SSIM
changed by -0.00004014 and +0.00019393. The census observed 6,931 IntraBC blocks
at QP 96 and 2,398 at QP 152, including both IntraBC modes, skipped and residual
blocks, and broad transform/geometry coverage. These fixed-QP results are
promising but are not a BD-rate result and do not include a perceptual metric.

The project decoder initially matched pinned `avmdec` on 27 of the 28 streams.
The remaining stream exposed an ordinary long-transform mapping reached because
IntraBC changed surrounding encoder decisions: `(long_side=0,
primary_symbol=2)` for 4x32/32x4 was rejected instead of mapping to
H_FLIPADST/V_FLIPADST. After adding the normative symmetric mappings, all 28
streams decode byte-identically. No IntraBC prediction or reconstruction
algorithm change was required.

The completed admission gate extends that probe to 32 locked source/QP cases
and 64 IntraBC-off/on arms. All arms encode deterministically and match pinned
AVM byte-for-byte through native raw OBU decode and the public canonical-HEIF
path. The census records 19,567 IntraBC blocks: all 32 disabled arms contain
zero, 31 enabled arms contain positive evidence, and the Pastel QP 255 enabled
arm is an explicit zero-occurrence boundary control because pinned AVM does not
select IntraBC there. Aggregate normalized OBU size falls from 3,402,416 bytes
with IntraBC disabled to 3,374,124 bytes with it enabled, a reduction of 28,292
bytes (approximately 0.8315%). This is fixed-QP admission evidence, not a
BD-rate result.

Current capability and remaining work:

1. No new IntraBC core algorithm is currently required; spatial/reference-bank
   prediction, DRL selection, both observed modes, transform skip, and observed
   palette paths already have focused evidence. Keep the new long-transform
   mappings covered by focused tests and whole-image parity.
2. Keep the deterministic 32-case admission matrix as the minimum I420 gate.
   Require zero occurrence in every disabled arm, positive occurrence in the
   31 designated enabled arms, and retain Pastel QP 255 as the explicit enabled
   zero-occurrence boundary control. I400 and I444 still require separate
   evidence before enabling IntraBC in those generators.
3. Add a representative screen-content corpus with repeated regions, text,
   sharp edges, and palettes larger than the two currently observed examples.
4. Exercise local and global IntraBC search ranges, bank rollover, block edges,
   chroma interaction, residual and skipped blocks, and all legal geometries.
5. Require exact AVM parity through raw OBU, public HEIF, no-libc, Windows, and
   WebAssembly gates before enabling it in a production recipe.

## Decoder features

### Trellis-coded quantization

AVM exposes `--enable-tcq=0|1|2`. TCQ changes coefficient-level entropy and
state transitions; it is not merely a slower encoder search. Mode 2 permits
frame-adaptive use in video, but AVM coerces either nonzero mode to always-on
mode 1 for the single-picture header used by this project. AVM also disables
TCQ at `cpu-used >= 2`, which is why the `cpu-used=9` corpus does not exercise
it.

Estimated effect: small to medium compression improvement at equal objective
quality. The result should be verified at several QPs and transform sizes.

TCQ was unsupported when this opportunity was first recorded. The decoder now
implements pinned AVM's always-on TCQ mode for single-picture files, including
state-dependent luma CDFs, high-range levels, dequantization, and the ordinary
FSC/chroma exclusions. The locked I420 development pilot reached exact native
and WebAssembly parity in both TCQ-off and TCQ-on arms. Production-profile
admission still requires broader Q-context, transform, bit-depth,
malformed-input, and durable platform evidence; frame-adaptive video mode
remains deferred.

Adoption should start with one locked I420 source. First encode it at
`cpu-used=1` with TCQ explicitly disabled and require census and decoder parity;
this separates syntax selected by the deeper ordinary search from TCQ itself.
Pin Intra-DIP and transform partitioning off in this initial profile because
both are separate decoder features that pinned AVM may select at this speed.
Then encode the same source and QPs with TCQ enabled for AVM-only measurement
and fixture extraction. The locked pilot has passed the initial exactness gate,
but TCQ-on output must not enter the production profile until the broader
matrix passes. Promotion must also measure native and WebAssembly decode
throughput for TCQ-on files and verify that TCQ-off files do not regress when
the new state-machine path is dormant.

Current implementation and remaining work:

1. The single-picture Sequence Header TCQ flag and its interaction with parity
   hiding are retained; frame-adaptive video-only modes remain outside scope.
2. The normative state machine, state-dependent coefficient contexts, low- and
   high-range levels, scan interaction, dequantization, clipping, and inverse
   transform handoff are implemented for the admitted subset.
3. TCQ state and CDF transitions have focused unit coverage, and the locked
   I420 pilot supplied whole-image development parity.
4. Keep TCQ dormant without changing non-TCQ behavior, ordinary FSC, or chroma
   coefficient paths.
5. Add durable positive OBU fixtures across Q contexts, transform sizes,
   planes, EOB classes, bit depths, and truncation points.
6. Require native, public HEIF, WebAssembly, no-libc, and Windows parity plus
   output nonmutation on malformed inputs before production-profile admission.

### Loop restoration

AVM enables restoration by default and exposes `--enable-restoration`. The
current generators still disable it. Restoration was unsupported when this
opportunity was first recorded; the decoder now supports the exact subset
observed in the locked fixture: 8-bit luma-only `RESTORE_PC_WIENER`. It also
supports scalar 8-bit `RESTORE_WIENER_NONSEP` on any subset of the I420 planes,
including cross-component chroma filtering, with either per-unit filters carried
in the tile data or a frame-level filter carried in the frame header, and with
one or more luma filter classes selected by the PC-Wiener classifier. Switchable
restoration, other chroma formats, and all 10-bit restoration remain
fail-closed.

Estimated effect: small to medium. Restoration can reduce ringing, blur, and
other reconstruction artifacts, allowing a better quality/size tradeoff. Its
benefit and cost depend on image texture and QP.

Current implementation and remaining work:

1. Sequence capabilities, per-frame mode and unit geometry, per-unit entropy
   flags, and the exact luma transform-skip map are parsed and retained with
   bounded validation.
2. The scalar 8-bit PC-Wiener classifier and filter implement pinned AVM's
   rounding, clipping, stripe boundaries, unit geometry, and edge extension.
3. Scalar Wiener-nonsep implements luma and cross-component chroma taps,
   pinned-AVM rounding, processing stripes, and downsampled-luma boundary
   extension for the target I420 profile. Per-unit filters are decoded from the
   filter bank with the normative subset, symmetry, and four-part tap coding;
   frame-level filters are decoded from the frame header with match-index group
   coding against previously decoded classes and the translated pre-computed
   Wiener bank. Multi-class luma filtering derives a 4x4 class from the
   PC-Wiener classifier and folds it onto the signalled class count, which is
   why the transform-skip map is retained whenever it is needed.
   `av2h_frame_restoration_needs_tx_skip()` is the single predicate for that.
4. Checked caller-owned workspace now includes the restoration destination,
   per-plane unit flags and taps, and transform-skip map while preserving
   allocation-free decode.
5. The filter order is `deblock -> CDEF -> CCSO -> restoration -> GDF`; GDF
   adds its correction to the restored baseline while sampling the earlier
   required filter signals.
6. `tests/fixtures/pc-wiener-restoration.obu` and the
   `decoder-pc-wiener-restoration-parity` CTest provide exact pinned-AVM
   whole-image evidence for the admitted subset.
7. The 896x598 target baseline probe is byte-exact against pinned AVM through
   reconstruction, every filter checkpoint, and final planar output.
8. `tests/filter_unit.c` covers the Wiener-nonsep coefficient tables and
   signalled subsets, the frame-filter match-index predictor including its
   fail-closed paths, the multi-class classifier folding for every supported
   class count and set index, and the transform-skip requirement predicate.
9. Add a retained Wiener-nonsep fixture plus focused malformed-control,
   truncation, edge, and odd-dimension cases to make the whole-image evidence
   durable.
10. Measure rate-distortion and decode cost across the photographic corpus before
   admitting restoration to the production generator. Add chroma, other modes,
   or 10-bit support only with separate observed demand and exact parity gates.

### Quantization matrices

AVM exposes `--enable-qm`, `--qm-min`, and `--qm-max`. Quantization matrices
weight transform frequencies differently and may preserve perceptually
important structure while spending fewer bits on less visible coefficients.

Estimated effect: small to medium perceptual improvement. PSNR may stay flat or
decline while subjective quality improves, so metric choice is important.

The decoder now implements pinned AVM's predefined matrix path for the existing
non-segmented still-picture profile. Frame parsing retains the selected Y, U,
and V IDs; IDs 0 through 14 use the complete predefined inverse-weight tables,
and ID 15 is the normative flat matrix. Matrix lookup follows AVM's adjusted
transform geometry, uses raster coefficient positions, and remains flat for
lossless, identity, and one-dimensional transforms. Dequantization uses checked
wide products and AVM's exact five-bit rounding.

`tests/fixtures/predefined-qmatrix.obu` is a pinned 8-bit I420 level-7 stream
generated with `--enable-qm=1`. The `decoder-predefined-qmatrix-parity` CTest
locks its complete decoded output to pinned AVM, while coefficient units cover
matrix weighting, flat-transform and lossless bypasses, and invalid geometry.
Pinned AVM emits no Quantization Matrix OBU for this predefined path.

Current capability and remaining work:

1. Keep segmentation disabled, which limits the admitted profile to one
   picture-level Y/U/V matrix selection and avoids per-block matrix indices.
2. Keep type-22 Quantization Matrix OBUs fail-closed. User-defined fundamental
   matrices, their scan-coded deltas, expansion, lifetime, and container
   placement are not implemented.
3. Add AVM parity fixtures across predefined levels, monochrome and I444,
   10-bit, transform geometries and classes, per-plane IDs, and Q contexts.
4. Add malformed and truncation tests that preserve caller output, then require
   public HEIF, WebAssembly, no-libc, and Windows parity before production
   profile admission.
5. Measure rate-distortion, encode cost, and native/WebAssembly decode cost
   before enabling matrices in the production generator.

### Adaptive quantization, delta-Q, and segmentation

AVM exposes variance, complexity, and cyclic-refresh adaptive quantization plus
objective and perceptual delta-Q modes. For a still image, variance or
perceptual spatial allocation is more relevant than cyclic refresh. These tools
can spend more bits on salient or complex areas and fewer on flat regions.

Estimated effect: medium perceptual improvement on spatially varied images.
File size at a fixed nominal QP is not guaranteed to decrease because the tool
changes spatial allocation rather than simply reducing rate.

The decoder admits the non-extended, eight-segment ALT_Q subset used by pinned
AVM `--aq-mode=1` on a closed-loop key picture. Segment feature data may be
signaled in the sequence header and reused by the frame, or signaled directly
in the frame header. Spatial segment IDs use the normative top, left, and
upper-left predictor, three adaptive CDF contexts, and negative deinterleave.
The full caller-owned luma mode map retains the ID at 4x4 resolution.

Segment ALT_Q is applied after the current superblock delta-Q base and clamped
to the bit-depth Q domain. The resulting per-block Q index drives luma and
chroma coefficient dequantization and the existing deblock cell map without
feeding the segment offset back into later delta-Q accumulation. Pinned AVM
whole-image parity is exact for `aq-mode=1`, `deltaq-mode=1`, and
`deltaq-mode=2` probes.

Extended 16-segment syntax, SKIP and GLOBALMV segment features, lossless
segments, temporal map reuse, cyclic refresh, and arbitrary multi-frame update
behavior remain fail-closed. The next useful work is broader bit-depth/QP
coverage, malformed segment-map vectors, and additional freestanding platform
evidence.

### Film-grain synthesis

AVM accepts a film-grain table and emits the model in a Film Grain Model OBU,
with Sequence Header enablement and frame-level model selection. The useful
compression workflow is to encode a denoised base image and synthesize a model
of the removed grain at decode time. Merely adding grain metadata to an
unchanged source does not guarantee a gain.

Estimated effect: large for genuinely noisy or intentionally grainy material,
but neutral or harmful for clean images. Synthesized output is perceptually
targeted and is not sample-identical to the pre-encode source or the decoded
base image.

Decoder implementation steps:

1. Define whether the public API returns the normative grain-applied image, the
   clean base image, or offers an explicit choice; include that choice in query
   binding and pixel-output semantics.
2. Parse and validate Sequence Header enablement, Film Grain Model OBU IDs,
   updates and availability, and frame-level model selection before admitting
   the model's points, scaling data, AR coefficients, chroma coupling, seed,
   overlap, and clipping parameters.
3. Implement the normative deterministic pseudo-random generator, grain-block
   construction, autoregressive filtering, scaling lookup, overlap, chroma
   handling, restricted-range clipping, and odd-edge behavior.
4. Add bounded grain buffers and scratch to workspace accounting with no global
   mutable state or allocation.
5. Apply synthesis after reconstruction filters and before output commit while
   preserving output nonmutation on all preflight and metadata failures.
6. Add bit-exact AVM vectors for random sequences and kernels, then whole-image
   fixtures covering monochrome/4:2:0 policy, overlap, chroma-from-luma,
   restricted range, edges, seeds, and malformed tables.
7. Define container carriage and normalization policy for Film Grain Model OBUs,
   including model lifetime and availability within the one-image profile,
   before admitting film grain to canonical HEIF.

## Measurement plan

Use a fixed source corpus split into natural photographs, noisy/grainy images,
and screen content. For each feature, encode enough QPs to compare curves rather
than comparing a single nominal QP. Record:

- encoded OBU and canonical HEIF byte sizes;
- encode wall time and peak memory;
- native and WebAssembly decode wall time, including a TCQ-off regression
   baseline;
- AVM and project-decoder output hashes;
- PSNR and SSIM per plane;
- at least one declared perceptual metric;
- census-observed feature values and unsupported/unknown tuples; and
- decoder status, diagnostic context, and output nonmutation for negative cases.

Compare rate-distortion curves with BD-rate or an equivalent curve integral.
Do not infer a compression gain from a smaller file at the same QP unless the
decoded-quality difference is also accounted for.

Slow reference encodes should be persistent, content-addressed work products.
Each arm should record the source hash, pinned AVM revision and encoder hash,
complete normalized argument vector, output hash, byte size, elapsed time, and
census/output hashes in an atomically written manifest. A completed arm may be
reused only when all inputs and recorded artifacts still match. Begin with one
source and a small QP ladder; after it passes, run additional cases overnight
or as independently shardable jobs. Future distributed workers should write
only per-arm artifacts and manifests, allowing a coordinator to validate and
merge results without rerunning successful encodes or permitting concurrent
writes to one aggregate report.

## Recommended order

1. Keep the current production recipe at `cpu-used=9` with TCQ and restoration
   explicitly disabled until replacement profiles pass their broader gates.
2. Preserve the locked TCQ pilot, then expand TCQ off/on coverage across Q
   contexts, transforms, bit depths, malformed inputs, and platform targets.
3. Measure TCQ rate-distortion and native/WebAssembly decode cost over a small
   QP ladder before expanding as overnight or independently shardable jobs.
4. Promote a separate high-effort encoder profile only after the complete
   corpus and platform gates pass.
5. Measure ordinary trellis modes with TCQ disabled.
6. Keep the separate IntraBC/screen-content profile and broaden its I400, I444,
   palette, search-range, rollover, and geometry evidence.
7. Measure PC-Wiener restoration off/on across representative photographic
   QPs, including encode cost, decode cost, and perceptual metrics.
8. Broaden restoration modes or formats only after the existing luma PC-Wiener
   subset has stronger focused evidence and observed corpus demand.
9. Evaluate quantization matrices and spatial AQ using perceptual metrics.
10. Treat film grain as a separate noisy-image profile with explicit output
   semantics, not as a transparent extension of sample-exact decode.

## Source pointers

- `tools/generate_image_references.py`: current photographic AVM recipe.
- `vendor/avm/av2/arg_defs.c`: pinned encoder switches and defaults.
- `vendor/avm/av2/encoder/speed_features.c`: speed-dependent TCQ policy.
- `tools/census/support.json`: reviewed feature-support classifications.
- `tools/census/status.md`: generated feature demand and support report.
- `docs/dev/10bit-wp14.md`: current accepted formats and unsupported boundary.
- `src/decoder/frame.c`: segmentation, matrix, restoration, and tile gates.
- `src/decoder/coeff_level.c`: TCQ, parity-hiding, and matrix gates.
- `src/decoder/restoration.c`: 8-bit luma PC-Wiener classifier and kernel.
- `src/decoder/filter.c`: restoration validation, orchestration, and current
   filter order.
# AV2 8-bit 4:2:0 coding opportunities

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

The photographic reference generator currently uses pinned AVM v1.0.0 with
`--good --cpu-used=9 --passes=1 --limit=1`. It explicitly disables IntraBC and
restoration. The accepted decoder profile also excludes TCQ, quantization
matrices, segmentation, film grain, and restoration.

The decoder already supports the AVM-generated corpus paths for non-DCT
transforms, directional prediction, MRL, CfL/MHCCP, luma FSC, IntraBC,
transform skip, palette prediction, deblock, CDEF, CCSO, and GDF. Support for a
feature path does not imply support for every combination of geometry,
coefficient profile, transform, and surrounding syntax.

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
| Trellis-coded quantization (TCQ) | Small to medium compression improvement | Unsupported | Sequence/frame signaling, coefficient entropy, dequantization, and reconstruction work |
| Loop restoration | Small to medium quality or compression improvement, especially near visible artifacts | Unsupported | Header/control parsing, restoration kernels, workspace, and filter-pipeline integration |
| Quantization matrices | Small to medium perceptual-quality improvement; metric-dependent | Unsupported | Matrix OBU/state, frame selection, matrix-aware quantization steps, and validation |
| Adaptive quantization, delta-Q, and segmentation | Medium perceptual-quality improvement on spatially varied images | Delta-Q is partial; segmentation is unsupported | Complete frame/leaf syntax, segment state, per-block Q propagation, and filter interaction |
| Film-grain synthesis | Large compression opportunity for genuinely noisy or grainy images; neutral or harmful for clean images | Unsupported and not sample-preserving | Grain metadata parsing, deterministic synthesis, output semantics, workspace, and conformance vectors |

## Encoder-only opportunities

### Slower AVM search

The existing `--cpu-used=9` setting is AVM's fastest good-quality mode. Lower
settings search more partition, prediction, transform, and coefficient choices.
This can produce a better rate-distortion result without intentionally adding
a new bitstream tool, at the cost of substantially longer encoding.

Start with `--cpu-used=5` and `--cpu-used=2`. Keep the accepted profile explicit:

```text
--enable-tcq=0
--enable-restoration=0
--enable-qm=0
--aq-mode=0
--deltaq-mode=0
```

AVM disables TCQ automatically at `cpu-used >= 2`, but the explicit option
protects the experiment against changed defaults. Other unsupported features
should likewise be pinned rather than inferred from current defaults.

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

Decoder implementation steps:

1. No new IntraBC core algorithm is currently required; spatial/reference-bank
   prediction, DRL selection, both observed modes, transform skip, and observed
   palette paths already have focused evidence.
2. Add a representative screen-content corpus with repeated regions, text,
   sharp edges, and palettes larger than the two currently observed examples.
3. Exercise local and global IntraBC search ranges, bank rollover, block edges,
   chroma interaction, residual and skipped blocks, and all legal geometries.
4. Require exact AVM parity through raw OBU, public HEIF, no-libc, Windows, and
   WebAssembly gates before enabling it in a production recipe.

## Decoder features

### Trellis-coded quantization

AVM exposes `--enable-tcq=0|1|2`. TCQ changes coefficient-level entropy and
state transitions; it is not merely a slower encoder search. AVM currently
disables it at `cpu-used >= 2`, which is why the `cpu-used=9` corpus does not
exercise it.

Estimated effect: small to medium compression improvement at equal objective
quality. The result should be verified at several QPs and transform sizes.

Decoder implementation steps:

1. Extend Sequence Header parsing to preserve every TCQ mode and its interaction
   with parity hiding instead of treating TCQ as a boolean exclusion.
2. Extend frame state for frame-adaptive TCQ selection where the sequence mode
   permits it.
3. Implement the normative TCQ state machine, coefficient contexts, low- and
   high-range level decoding, parity/state transitions, and scan interaction.
4. Integrate TCQ levels with dequantization, clipping, inverse transforms, FSC,
   chroma, and all supported bit depths without weakening non-TCQ behavior.
5. Account for any additional CDF or scratch state in caller-owned workspace.
6. Add frozen AVM vectors for individual state transitions and coefficient
   blocks, then positive OBU fixtures across Q contexts, transform sizes, planes,
   EOB classes, and truncation points.
7. Remove the TCQ rejection gates in `coeff_level.c` only after focused vectors
   pass, then require full output parity and output nonmutation on malformed
   inputs.

### Loop restoration

AVM enables restoration by default and exposes `--enable-restoration`. The
current generators disable it, and the decoder rejects the Sequence Header flag
before frame decode and again at the filter boundary.

Estimated effect: small to medium. Restoration can reduce ringing, blur, and
other reconstruction artifacts, allowing a better quality/size tradeoff. Its
benefit and cost depend on image texture and QP.

Decoder implementation steps:

1. Parse and retain restoration capabilities and per-frame/per-unit controls,
   rejecting reserved combinations and malformed unit layouts.
2. Determine the exact AV2 v1.0.0 restoration modes emitted by pinned AVM and
   stage implementation one mode at a time.
3. Implement sample-exact 8-bit kernels with normative rounding, clipping,
   boundary extension, unit geometry, and chroma subsampling behavior.
4. Add restoration source/destination planes and line or unit scratch to the
   checked workspace layout; preserve the allocation-free API contract.
5. Derive restoration's exact normative position and source planes from the
   pinned specification and AVM, then integrate it with the existing
   deblock/CDEF/CCSO/GDF pipeline while preserving required references.
6. Add kernel vectors, edge/odd-dimension matrices, malformed controls,
   truncation tests, and AVM-generated whole-image parity fixtures.
7. Lift the fail-closed checks in `frame.c`, `filter.c`, and `frame_output.c`
   only after both 8-bit and retained 10-bit filter paths have explicit policy.

### Quantization matrices

AVM exposes `--enable-qm`, `--qm-min`, and `--qm-max`. Quantization matrices
weight transform frequencies differently and may preserve perceptually
important structure while spending fewer bits on less visible coefficients.

Estimated effect: small to medium perceptual improvement. PSNR may stay flat or
decline while subjective quality improves, so metric choice is important.

Decoder implementation steps:

1. Implement bounded parsing and validation of the AV2 quantization-matrix OBU
   and its matrix-set lifetime within the one-image profile.
2. Extend frame quantization parsing to retain matrix enablement and selected
   levels for Y, U, and V instead of rejecting `using_qmatrix`.
3. Add normative default/custom matrix lookup by plane, transform size, and
   coefficient position with strict index and dimension validation.
4. Apply inverse-matrix scaling in dequantization with checked intermediate
   widths, exact rounding, clipping, and 8/10-bit coverage.
5. Define container placement and normalization rules for matrix OBUs before
   admitting them to canonical HEIF.
6. Add unit vectors for matrix expansion and dequantization, negative OBU cases,
   and AVM parity fixtures across matrix levels, planes, and transform sizes.
7. Remove the frame and coefficient rejection gates only after malformed matrix
   data cannot mutate caller output.

### Adaptive quantization, delta-Q, and segmentation

AVM exposes variance, complexity, and cyclic-refresh adaptive quantization plus
objective and perceptual delta-Q modes. For a still image, variance or
perceptual spatial allocation is more relevant than cyclic refresh. These tools
can spend more bits on salient or complex areas and fewer on flat regions.

Estimated effect: medium perceptual improvement on spatially varied images.
File size at a fixed nominal QP is not guaranteed to decrease because the tool
changes spatial allocation rather than simply reducing rate.

The decoder already has partial delta-Q infrastructure: frame flags and
resolution, tile CDFs, current-Q state, coefficient dequantization, and deblock
Q propagation. Frame-level segmentation remains an explicit rejection.

Decoder implementation steps:

1. Define the accepted still-image subset of sequence and frame segmentation;
   begin with the exact modes emitted by AVM `--aq-mode=1` and `2`.
2. Parse segment maps, feature enable/data arrays, inheritance/update flags, and
   every segment feature that can affect a key still frame.
3. Store segment IDs in bounded caller-owned tile maps and propagate the active
   segment through partition, prediction, coefficient, and filter stages.
4. Combine segment Q offsets, frame delta-Q, plane deltas, and sequence deltas
   in the normative order with the existing wide Q-index domain and clamps.
5. Complete delta-Q reset/update behavior for every superblock and CDF-update
   mode, including skipped, palette, FSC, and IntraBC blocks.
6. Apply segment effects to loop filters and reject segment features outside
   the first accepted subset before output mutation.
7. Add focused syntax/state vectors, segment-boundary images, negative feature
   combinations, and AVM whole-image parity at several AQ modes and QPs.

### Film-grain synthesis

AVM accepts a film-grain table and emits film-grain parameters. The useful
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
2. Parse and validate film-grain sequence/frame metadata, points, scaling data,
   AR coefficients, chroma coupling, seed, overlap, clipping, and update rules.
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
7. Extend container metadata and normalization policy if film-grain information
   is carried outside the current canonical Sequence Header/item payload shape.

## Measurement plan

Use a fixed source corpus split into natural photographs, noisy/grainy images,
and screen content. For each feature, encode enough QPs to compare curves rather
than comparing a single nominal QP. Record:

- encoded OBU and canonical HEIF byte sizes;
- encode wall time and peak memory;
- AVM and project-decoder output hashes;
- PSNR and SSIM per plane;
- at least one declared perceptual metric;
- census-observed feature values and unsupported/unknown tuples; and
- decoder status, diagnostic context, and output nonmutation for negative cases.

Compare rate-distortion curves with BD-rate or an equivalent curve integral.
Do not infer a compression gain from a smaller file at the same QP unless the
decoded-quality difference is also accounted for.

## Recommended order

1. Measure `cpu-used` 9, 5, and 2 with the current accepted syntax pinned.
2. Measure ordinary trellis modes with TCQ disabled.
3. Add a separate IntraBC/screen-content encoder preset and broader evidence.
4. Prototype TCQ because it directly targets coefficient coding efficiency.
5. Prototype restoration because it can improve the filtered reconstruction
   across ordinary photographic content.
6. Evaluate quantization matrices and spatial AQ using perceptual metrics.
7. Treat film grain as a separate noisy-image profile with explicit output
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
- `src/decoder/filter.c`: restoration filter gate and current filter order.
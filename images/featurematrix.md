# AV2 still-image feature matrix

This matrix records which AV2 coding tools the two generated photographic
reference profiles permit and whether each tool is useful for a single-frame
still image. The third decoder profile, `spatial-still-8bit`, is admitted; its
88-case photographic encoder suite remains defined but not generated.

The inventory is based on the feature controls exposed by the pinned AVM
revision `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`, principally
[`apps/avmenc.c`](../vendor/avm/apps/avmenc.c) and
[`av2/arg_defs.c`](../vendor/avm/av2/arg_defs.c). It is not a row-by-row
transcription of every syntax element in the AV2 specification. Syntax that has
no encoder control, basic entropy-coding machinery, and purely diagnostic or
speed-control options are summarized instead of being expanded into hundreds
of rows.

The exact profile recipes are in
[`recipes/encodings.json`](recipes/encodings.json). Compression opportunities,
decoder admission, and existing measurements are discussed in
[`../docs/research/feature.md`](../docs/research/feature.md) and
[`../docs/capabilities.md`](../docs/capabilities.md).

## How to read the profile columns

| Mark | Meaning |
| --- | --- |
| `[x]` | Enabled or permitted by this profile. This does not prove that every encoded image selects the tool. |
| `[ ]` | Disabled by an explicit argument, an effective speed-policy decision documented for the profile, or a prerequisite that is off. |
| `cond.` | Enabled only for the named format/mode, or subordinate to another checked feature. |
| `n/a` | Cannot affect the current one-frame closed-loop key-picture corpus, even if AVM's generic video default is on. |

`D` means the profile inherits the pinned encoder default instead of spelling
the choice out in its recipe. The current comparison rows describe encoder
admission, not observed bitstream syntax; proving actual use requires parsing
the generated streams. `spatial-still-8bit` pins its material choices explicitly
in the recipe and is described separately because it has no generated streams
to observe yet.

Both profiles encode one frame with `--force-video-mode=0`, one pass,
constant-quality rate control, and AVM's `good` mode. `photographic-8bit` uses
`--cpu-used=9`; `target-baseline-8bit` uses `--cpu-used=1`.

## Stream, sample, and picture structure

| AV2 feature / AVM option | Relevant or useful for still images? | `photographic-8bit` | `target-baseline-8bit` | Notes |
| --- | --- | --- | --- | --- |
| AV2 codec, OBU elementary stream (`--codec=av2 --obu`) | Required | `[x]` | `[x]` | Both produce low-overhead OBU streams rather than IVF/WebM. |
| One closed-loop key picture (`--limit=1`, video mode off) | Required | `[x]` | `[x]` | Removes temporal dependencies and is the defining corpus model. |
| Reduced still-picture header | Yes | `[x]` | `[x]` | `--full-still-picture-hdr` is absent; AVM uses its ordinary single-picture header. |
| Full still-picture header (`--full-still-picture-hdr`) | Occasionally | `[ ]` | `[ ]` | Useful only if full-header syntax or metadata flexibility is specifically required. |
| 8-bit coding (`--bit-depth=8 --input-bit-depth=8`) | Required for these suites | `[x]` | `[x]` | `spatial-still-8bit` also remains 8-bit. |
| Monochrome / I400 (`--monochrome`, profile 0) | Content-specific | `cond. (14 cases)` | `[ ]` | Useful for true grayscale assets; not part of the target suite. |
| 4:2:0 / I420 (profile 0) | Yes, primary photographic format | `[x]` | `[x]` | 60 legacy cases and all 14 target cases. |
| 4:4:4 / I444 (profile 1) | Content-specific | `cond. (14 cases)` | `[ ]` | Useful for sharp chroma and graphics; costs more than 4:2:0. |
| 4:2:2 input/profile path | Potentially | `[ ]` | `[ ]` | Exposed by AVM but absent from prepared inputs and both suites. |
| 10/12-bit coding | Potentially, not this goal | `[ ]` | `[ ]` | Valuable for HDR and gradients, but outside the proposed 8-bit profile. |
| Full-range BT.709 samples | Yes | `[x]` | `[x]` | Recorded in prepared/reference manifests; CICP signaling should be audited separately. |
| Chroma sample position signaling | Yes for subsampled chroma | `cond. (I420: unspecified)` | `cond. (I420: unspecified)` | Explicitly set to `unspecified` for I420. |
| Error-resilient mode | Usually no | `[ ]` | `[ ]` | Transmission resilience normally adds overhead without value for an independently stored still. |
| Tiles (`--tile-columns`, `--tile-rows`) | Sometimes | `[ ] D` | `[ ] D` | Default single tile favors compression; tiles may help parallel decode or bounded memory at some overhead. |
| Loop filtering across tile boundaries | Conditional on tiling | `[x] D` | `[x] D` | `--disable-loopfilters-across-tiles=0`; immaterial while a single tile is used. |
| Cropping window (`--enable-cropping-window`) | Sometimes | `[ ] D` | `[ ] D` | Could preserve coded alignment while signaling display dimensions; current inputs use exact dimensions. |
| Superblock/partition size controls | Yes | `[x] D` | `[x] D` | AVM defaults to 256 maximum SB/partition and 4 minimum partition (subject to speed/size policy). |
| Lossless mode (`--lossless`) | Yes for archival/synthetic cases | `cond. (QP 0 case)` | `[ ]` | The legacy Pastel spectrum includes QP 0; the target suite is QP 152. |
| Layer configuration record OBU (`--enable-lcr`) | No for current stills | `[ ]` | `[ ]` | Explicitly disabled. |
| Operating-point-set OBU (`--enable-operating-point-sets`) | No for current stills | `[ ]` | `[ ]` | Explicitly disabled; one picture has no scalability structure to describe. |
| Atlas segment OBU (`--enable-atlas`) | No for current stills | `[ ]` | `[ ]` | Explicitly disabled. |
| Multiple operating points / layers | No for current stills | `[ ]` | `[ ]` | Single-layer, single-picture corpus. |
| Timing info, frame rate, display timing | No for current stills | `[ ]` | `[ ]` | Container/application metadata should carry still-image timing if needed. |
| Frame hash / short content-interpretation metadata (`--use-per-plane-frame-hash`, `--use-short-metadata`) | Validation-specific | `[ ]` | `[ ]` | The generator normalizes content-interpretation OBUs out of reference streams. |
| MFH OBU signaling (`--enable-mfh-obu-signaling`) | No for one picture | `[ ] D` | `[ ] D` | Multi-frame-header signaling has no purpose in the current corpus. |
| ICC OBU/file (`--icc-file`) | Potentially | `[ ]` | `[ ]` | Color-management value is high, but it is orthogonal to coding efficiency and current raw OBU tests. |

## Intra prediction, partitioning, and transforms

These are the principal coding families for a closed-loop key picture. `D`
marks AVM defaults; `cpu-used` may prune encoder searches without disabling the
corresponding decoder syntax.

| AV2 feature / AVM option | Relevant or useful for still images? | `photographic-8bit` | `target-baseline-8bit` | Notes |
| --- | --- | --- | --- | --- |
| Extended partitions (`--enable-ext-partitions`) | Yes | `[x] D` | `[x] D` | More shapes can improve spatial adaptation. |
| Rectangular partitions (`--enable-rect-partitions`) | Yes | `[x] D` | `[x] D` | Useful around directional edges and textures. |
| Uneven 4-way partitions (`--enable-uneven-4way-partitions`) | Yes | `[x] D` | `[x] D` | Adds asymmetric partition choices. |
| Transform partitioning (`--enable-tx-partition`) | Yes | `[x] D` | `[x] D` | Lets transform boundaries differ from prediction partitions. |
| Semi-decoupled partitioning (`--enable-sdp`) | Yes | `[x] D` | `[x] D` | AVM describes this control specifically for key frames. |
| Extended SDP (`--enable-extended-sdp`) | No | `n/a` | `n/a` | Inter-frame extension; ordinary SDP is the still-image row above. |
| Multiple reference lines (`--enable-mrls`) | Yes | `[x] D` | `[x] D` | Additional reconstructed reference lines for intra prediction. |
| Intra edge filtering (`--enable-intra-edge-filter`) | Yes | `[x] D` | `[x] D` | Improves directional intra references. |
| Directional angle delta (`--enable-angle-delta`) | Yes | `[x] D` | `[x] D` | Finer directional prediction. |
| Smooth intra modes (`--enable-smooth-intra`) | Yes | `[x] D` | `[x] D` | Useful for gradients and low-detail areas. |
| Paeth intra (`--enable-paeth-intra`) | Yes | `[x] D` | `[x] D` | Spatial predictor useful around edges. |
| Intra data-driven prediction (`--enable-intra-dip`) | Yes; promising | `[x] D` | `[x]` | Explicitly pinned on in the target recipe. |
| Intra bi-prediction (`--enable-ibp`) | Yes; promising | `[x] D` | `[x] D` | Combines directional intra predictors. |
| Chroma from luma (`--enable-cfl-intra`) | Yes for color formats | `cond. D` | `[x] D` | Inapplicable to I400. |
| Multi-hypothesis cross-component prediction (`--enable-mhccp`) | Yes for color formats | `cond. D` | `[x] D` | Exploits cross-plane correlation; inapplicable to I400. |
| Adaptive CfL downsampling (`--select-adaptive-ds=3`) | Yes for subsampled chroma | `cond. D` | `[x] D` | Default adaptive choice; inapplicable to I400/I444 downsampling. |
| Palette prediction (`--enable-palette`) | Content-specific | `[x] D` | `[x] D` | Large potential for graphics; occasional photographic use. |
| Intra block copy (`--enable-intrabc`) | Content-specific | `[ ]` | `[x]` | Explicit profile difference; usually modest for photographs, large for repeated screen content. |
| Extended IntraBC search (`--enable-intrabc-ext`) | Conditional on IntraBC | `[ ]` | `[x]` | Explicitly on in target; ineffective when IntraBC is off. |
| Forward skip coding (`--enable-fsc`) | Yes | `[x] D` | `[x] D` | Particularly relevant to transform-skip-like residuals. |
| Intra identity transform (`--enable-idtx-intra`) | Yes | `[x] D` | `[x] D` | Used when FSC is not selected. |
| Intra secondary transform (`--enable-ist`) | Yes | `[x] D` | `[x] D` | Improves residual energy compaction. |
| Extended/flip transform types (`--enable-flip-idtx`) | Yes | `[x] D` | `[x] D` | Includes FLIPADST and directional transform variants. |
| Cross-chroma component transform (`--enable-cctx`) | Yes for color formats | `cond. D` | `[x] D` | Inapplicable to monochrome. |
| Chroma DCT-only restriction (`--enable-chroma-dctonly`) | Usually harmful | `[ ] D` | `[ ] D` | Off means the broader chroma transform set remains available. |
| Reduced transform partition/type sets (`--reduced-tx-part-set`, `--reduced-tx-type-set`) | Usually no | `[ ] D` | `[ ] D` | Full sets favor compression; reduced sets trade efficiency for complexity. |
| Intra DCT-only/default-transform-only restrictions (`--use-intra-dct-only`, `--use-intra-default-tx-only`) | Usually no | `[ ] D` | `[ ] D` | Restrictions are off, leaving all admitted intra transforms available. |
| ML partition/transform pruning (`--use-ml-erp-pruning`, `--disable-ml-partition-speed-features`, `--disable-ml-transform-speed-features`) | Encoder-only tradeoff | `[x] D` | `[x] D` | Does not create new syntax; lower `cpu-used` is the more important target-profile search difference. |

## Quantization and coefficient coding

| AV2 feature / AVM option | Relevant or useful for still images? | `photographic-8bit` | `target-baseline-8bit` | Notes |
| --- | --- | --- | --- | --- |
| Fixed-QP constant-quality mode (`--end-usage=q --qp=N`) | Yes | `[x]` | `[x]` | Legacy covers several QPs; target currently uses only QP 152. |
| Ordinary trellis coefficient optimization (`--enable-trellis-quant`) | Yes | `[x] D (mode 3)` | `[x] (mode 1)` | Encoder-side RDO; target explicitly requests all encoding stages. |
| Trellis-coded quantization / TCQ (`--enable-tcq`) | Yes; promising | `[ ]` | `[x]` | Legacy `cpu-used=9` effectively excludes TCQ; target explicitly enables it and uses a compatible speed. |
| Parity hiding (`--enable-parity-hiding`) | Potentially | `[ ]` | `[ ]` | Target explicitly disables it; current production policy also excludes it pending focused evidence. |
| Quantization matrices (`--enable-qm`) | Potentially; metric-dependent | `[ ]` | `[ ]` | Target explicitly disables them. Predefined non-segmented matrices are decoder-admitted, but not yet a profile choice. |
| QM level range (`--qm-min`, `--qm-max`) | Conditional on QM | `n/a` | `n/a` | Defaults 8..15 are ineffective while QM is off. |
| Adaptive quantization (`--aq-mode`) | Yes; promising | `[ ]` | `[ ]` | Target explicitly uses mode 0. Variance AQ should be tested as an alternative to perceptual delta-Q, not stacked blindly. |
| Delta-Q (`--deltaq-mode`) | Yes; promising | `[ ]` | `[x] (mode 2)` | Target explicitly enables perceptual delta-Q. |
| Temporal dependency model (`--enable-tpl-model`) | Conditional | `[ ] D` | `[x] D` | Delta-Q prerequisite; meaningful in target because mode 2 is enabled. |
| Chroma delta-Q (`--enable-chroma-deltaq`) | Potentially | `[ ] D` | `[ ] D` | Candidate for chroma-sensitive tests; not currently enabled. |
| Extended segmentation (`--enable-ext-seg`) | Usually no | `[ ] D` | `[ ]` | Target explicitly disables the expanded segment count. |
| ALT_Q segmentation used by AQ | Conditional on AQ | `[ ]` | `[ ]` | Supported subset, but AQ is off in both current profiles. |
| Adaptive `quant_b` (`--quant-b-adapt`) | Encoder experiment | `[ ] D` | `[ ] D` | Not currently part of either pinned profile. |
| CDF adaptation (`--cdf-update-mode`) | Yes | `[x] D` | `[x] D` | Normal adaptive entropy coding remains enabled. |
| Average CDF initialization (`--enable-avg-cdf`) | Small/conditional | `[x] D` | `[x] D` | More relevant with multiple tiles/frames, but permitted. |
| Scan-type signaling (`--scan-type-info-present-flag`) | Potentially | `[ ] D` | `[ ] D` | Not currently enabled. |

## In-loop filtering and reconstruction

| AV2 feature / AVM option | Relevant or useful for still images? | `photographic-8bit` | `target-baseline-8bit` | Notes |
| --- | --- | --- | --- | --- |
| Deblocking (`--enable-deblocking`) | Yes | `[x] D` | `[x] D` | Core reconstruction filter. |
| CDEF (`--enable-cdef`) | Yes | `[x] D` | `[x] D` | Constrained directional enhancement filter. |
| CDEF on transform-skipped blocks (`--enable-cdef-on-skip-txfm`) | Yes | `[x] D` | `[x] D` | Default mode 1. |
| Guided detail filter / GDF (`--enable-gdf`) | Yes | `[x] D` | `[x] D` | Core target filter pipeline. |
| GDF unit matches superblock (`--gdf-unit-matches-sb`) | Conditional | `[ ] D` | `[ ] D` | Unit-size policy, not an independent filter. |
| Cross-component sample offset / CCSO (`--enable-ccso`) | Yes | `[x] D` | `[x] D` | Useful for cross-plane reconstruction correction. |
| CCSO unit matches superblock (`--ccso-unit-matches-sb`) | Conditional | `[ ] D` | `[ ] D` | Unit-size policy. |
| Loop restoration (`--enable-restoration`) | Yes; promising | `[ ]` | `[x]` | Major explicit profile difference. |
| PC-Wiener restoration (`--enable-pc-wiener`) | Conditional on restoration | `[ ]` | `[x] D` | Current decoder admission is limited; see capability matrix. |
| Nonseparable Wiener restoration (`--enable-wiener-nonsep`) | Conditional on restoration | `[ ]` | `[x] D` | Current decoder supports an admitted 8-bit I420 subset. |
| Switchable restoration and other restoration modes | Potentially | `[ ]` | `cond.` | Encoder search may choose admitted modes; decoder support is not exhaustive. |
| Deblocking on sub-prediction blocks (`--enable-lf-sub-pu`) | No for key-only stills | `n/a` | `n/a` | Inter-prediction-specific. |
| Banding metadata (`--enable-banding-metadata`) | Potentially | `[ ] D` | `[ ] D` | Metadata rather than a compression tool; currently off. |

## Film grain, denoising, and content tuning

| AV2 feature / AVM option | Relevant or useful for still images? | `photographic-8bit` | `target-baseline-8bit` | Notes |
| --- | --- | --- | --- | --- |
| Film-grain synthesis (`--film-grain-table`) | Content-specific; potentially large | `[ ]` | `[ ]` | Could greatly help genuinely noisy images, but changes output semantics and is not decoder-supported. |
| Film-grain test vectors (`--film-grain-test`) | No, test-only | `[ ]` | `[ ]` | Explicitly disabled. |
| Encoder denoising/noise sensitivity | Content-specific | `[ ]` | `[ ]` | Lossy preprocessing; should not be conflated with sample-preserving codec evaluation. |
| Content tuning (`--tune-content`) | Potentially | `[ ] D` | `[ ] D` | A third profile should test photographic/default tuning explicitly. |
| Metric tuning (`--tune`) | Yes, encoder-only choice | `[ ] D` | `[ ] D` | Affects RDO decisions rather than adding syntax; metric choice must match evaluation goals. |
| Sharpness control (`--sharpness`) | Potentially | `[ ] D` | `[ ] D` | Filter behavior should be measured, not assumed. |

## Video-only prediction and reference tools

The following controls are exposed by `avmenc`, and many are enabled by its
generic defaults, but they cannot be selected in the current one-frame
closed-loop key picture. They are therefore `n/a` for both profiles rather than
misleadingly checked.

| AV2 feature / AVM option | Still-image relevance | `photographic-8bit` | `target-baseline-8bit` |
| --- | --- | --- | --- |
| Temporal interpolated prediction (`--enable-tip`, `--enable-tip-refinemv`) | None for one frame | `n/a` | `n/a` |
| MV trajectory/high-motion search (`--enable-mv-traj`, `--enable-high-motion`) | None for one frame | `n/a` | `n/a` |
| BAWP, CWP, implicit masked blending (`--enable-bawp`, `--enable-cwp`, `--enable-imp-msk-bld`) | None for key-only stills | `n/a` | `n/a` |
| Adaptive/flexible/joint MVD and sign derivation (`--enable-adaptive-mvd`, `--enable-flex-mvres`, `--enable-joint-mvd`, `--enable-mvd-sign-derive`) | None for key-only stills | `n/a` | `n/a` |
| RefineMV/optical-flow refinement (`--enable-refinemv`, `--enable-opfl-refine`) | None for key-only stills | `n/a` | `n/a` |
| Inter transforms (`--enable-inter-ist`, `--enable-inter-ddt`, `--use-inter-dct-only`) | None for key-only stills | `n/a` | `n/a` |
| Masked/one-sided/inter-intra compound (`--enable-masked-comp`, `--enable-onesided-comp`, `--enable-interintra-comp`, `--enable-smooth-interintra`) | None for key-only stills | `n/a` | `n/a` |
| Difference-weighted/wedge compound (`--enable-diff-wtd-comp`, `--enable-interinter-wedge`, `--enable-interintra-wedge`) | None for key-only stills | `n/a` | `n/a` |
| Global/local warp (`--enable-global-motion`, `--enable-warped-motion`, `--enable-warp-causal`, `--enable-warp-delta`, `--enable-six-param-warp-delta`, `--enable-warp-extend`) | None for key-only stills | `n/a` | `n/a` |
| Skip mode (`--enable-skip-mode`) | None for key-only stills | `n/a` | `n/a` |
| Temporal reference MVs (`--enable-ref-frame-mvs`, `--enable-refmvbank`, `--reduced-ref-frame-mvs-mode`) | None for one frame | `n/a` | `n/a` |
| DRL reorder/reference-MV limits (`--enable-drl-reorder`) | None for one frame | `n/a` | `n/a` |
| Reference maps/sets and BRU (`--reduced-reference-set`, `--enable-bru`) | None for one frame | `n/a` | `n/a` |
| Alt-ref, overlay, key-frame filtering (`--enable-overlay`, `--enable-keyframe-filtering`) | None for one frame | `n/a` | `n/a` |
| Frame-parallel decoding and monotonic output order | None for one frame | `n/a` | `n/a` |
| S-frames/forward key frames/refresh flags (`--enable-sframe`, `--enable-fwd-kf`, `--enable-short-refresh-frame-flags`, `--disable-kf`) | None for one frame | `n/a` | `n/a` |
| Frame QP offsets and inter-frame rate controls (`--use-fixed-qp-offsets`) | None for one frame | `n/a` | `n/a` |

## Encoder-only controls that still matter

These controls do not identify an AV2 bitstream feature, so a checkbox means
the profile uses the setting rather than enabling decoder syntax.

| Encoder control | Relevant or useful for still images? | `photographic-8bit` | `target-baseline-8bit` | Notes |
| --- | --- | --- | --- | --- |
| Slow search (`--cpu-used`) | Yes; small-to-medium expected gain | `[ ] (9)` | `[x] (1)` | The target setting was about 8x slower in the recorded Pastel probe. |
| One-pass encode (`--passes=1`) | Yes, but not necessarily optimal | `[x]` | `[x]` | Multi-pass benefit in fixed-QP still mode is unproven. |
| Row multithreading (`--row-mt=1`) | Operational | `[x]` | `[x]` | Primarily encode speed; determinism remains pinned by locks. |
| Ordinary trellis search effort | Yes | `[x] D` | `[x]` | Target explicitly uses mode 1. |
| Partition/transform ML pruning | Compression-vs-time tradeoff | `[x] D` | `[x] D` | A third profile may profit from less aggressive pruning. |
| `--test-decode=fatal` | Validation | `[x]` | `[x]` | Rejects encoder/decoder mismatch during generation. |

## Recommended decoder target: broad syntax, narrow structure

If the goal is to avoid constraining `avmenc` or another AV2 encoder, the next
decoder target should not be defined as a copy of an encoder command line.
Encoder defaults and search policies change, and an enabled encoder tool may or
may not appear in a particular stream.

The recommended contract is a **complete spatial-still subset**: accept every
normative AV2 coding choice that can improve compression or reconstruction
inside a tightly bounded one-picture, 8-bit still-image structure. Define the
boundary mostly by what is excluded, rather than maintaining a fragile allow
list of syntax combinations.

### Required envelope

| Area | Recommended decoder requirement | Reason |
| --- | --- | --- |
| Picture structure | Exactly one independently decodable closed-loop key picture using the reduced still-picture sequence header and one operating point | Preserves the simple still decoder model and excludes sequence timing, decoder-model, and multi-operating-point machinery that cannot improve the image. |
| Bit depth and chroma | 8-bit I400, I420, and I444 using the applicable AV2 profiles | Covers the prepared corpus and grayscale, ordinary photographic, and sharp-chroma use cases. Keep 4:2:2 and higher bit depths in separate profiles. |
| Spatial structure | All legal superblock, block, prediction, and transform partition shapes for the admitted dimensions | Partition-shape restrictions directly constrain encoder rate-distortion search and create a costly combination matrix. |
| Tiles and tile groups | All legal one-picture spatial tile layouts, tile-group divisions, context resets, and loop-filter boundary choices | Permitting tiles does not require an encoder to use them. An encoder optimizing only compression will normally choose one tile, while other encoders remain free to trade some compression for parallel decode or bounded memory. |
| Intra prediction | All normative still-applicable intra modes, including directional modes, MRL, Intra-DIP, IBP, CfL/MHCCP, palette, FSC/identity-transform paths, and IntraBC with extended search | These are the main prediction choices available when temporal references are forbidden. IntraBC is content-specific and relatively complex, but remains in core because it is sample-preserving, already implemented, and may help repeated photographic structure. |
| Transforms | All normative primary, secondary, cross-component, transform-skip, transform-type, and transform-partition choices applicable to a key picture | An encoder should be free to select the best residual representation per block. |
| Coefficient coding | Ordinary coefficient syntax, single-picture TCQ, and parity hiding across their legal still-picture combinations | TCQ can materially improve coding; parity hiding is comparatively low decoder cost and should not force an encoder fallback. |
| Quantization | Lossless and lossy base Q, luma/chroma delta-Q, predefined and custom quantization matrices, and legal matrix selection | Spatial frequency weighting is directly relevant to visual-quality-per-byte optimization. |
| Spatial bit allocation | Delta-Q, AQ-produced signaling, and all still-applicable segmentation features, not only the currently reached ALT_Q subset | A general encoder may choose different spatial allocation strategies; decoder admission should follow legal syntax rather than AVM's current preference. |
| In-loop filters | Complete still-applicable 8-bit deblock, CDEF, CCSO, GDF, and restoration syntax, including PC-Wiener, nonseparable Wiener, switchable restoration, legal planes/classes for the selected chroma format, and per-unit choices | Filters are among the strongest quality-per-byte tools and must not be artificially narrowed to modes emitted by today's corpus. |
| Lossless | Every legal base-Q-zero combination inside this envelope, with filters and transforms constrained as the specification requires | Lossless should be a property of the same still profile, not a collection of observed special cases. |
| Cropping | Legal still-picture cropping/display-window syntax | Lets encoders choose efficient coded alignment without changing requested display dimensions. |
| Color description | Legal 8-bit color-description and chroma-location signaling | Required to interpret pixels correctly; it is orthogonal to entropy coding but part of a useful image contract. |
| Unknown combinations | Accept every legal cross-product inside the envelope; reject malformed or out-of-envelope syntax explicitly | Feature-by-feature support is insufficient if ordinary combinations still fail closed. |

This is intentionally broader than the current decoder admission in
[`../docs/capabilities.md`](../docs/capabilities.md). In particular, the target
adds parity hiding, custom quantization-matrix OBUs, segmentation beyond ALT_Q,
complete 8-bit restoration combinations, chroma FSC, and currently unreached
geometry/transform/coefficient cross-products.

### Deliberate exclusions

| Excluded family | Why it is outside the core profile |
| --- | --- |
| Inter pictures, reference pictures, hidden/show-existing pictures, alt-ref/overlay, motion tools, and DPB management | They add the bulk of video-decoder state and attack surface but cannot improve a genuinely independent one-picture encode. |
| Full sequence-header timing/decoder-model syntax, multiple temporal/spatial layers, additional operating points, operating-point-set OBUs, LCR, atlas, and multi-frame-header structures | Scalability and multi-picture carriage do not improve compression or quality of one still. |
| S-frames, frame-rate control, temporal error resilience, and frame-parallel video signaling | Video delivery features, not spatial image coding tools. |
| 10/12-bit and 4:2:2 syntax | Valuable, but they should be separately advertised capability profiles so an 8-bit still decoder remains a bounded implementation target. |
| Film-grain parameter signaling and synthesis | Potentially valuable for noisy photographs, but intentionally deferred. It changes decoded-output semantics and requires a separate raw-reconstruction versus display-output API contract. |
| Container transforms, alpha, depth/auxiliary images, grids, animation, and metadata containers | Carriage/application features rather than AV2 coding efficiency; specify them independently. |
| Encoder search controls such as `cpu-used`, trellis search mode, ML pruning, threading, passes, and metric tuning | They can improve an encoder without changing the normative syntax a conforming decoder sees. |

### Why tiles are part of the profile

`spatial-still-8bit` permits both single-tile and multi-tile pictures. This is a
decoder capability, not an encoder default. A best-compression encoder can and
normally should emit one tile so entropy contexts, prediction, partitioning,
and filtering are not divided at tile boundaries. No `+tiles` profile flavour
is necessary.

Tiles remain worth accepting because they are spatial one-picture syntax and do
not introduce reference-picture or temporal state. An encoder may reasonably
choose them for:

- parallel decoding;
- lower peak working memory;
- independent tile delivery or region access;
- implementation-specific latency limits.

Implementations may impose documented resource limits on dimensions, tile
count, tile-group count, and allocation size. Those limits must be independent
of whether a legal coding-tool combination is understood. Film grain is not
part of this profile for now and no grain-capable profile flavour is defined.

### Conformance rule

The profile is complete only when conformance vectors cover both individual
families and representative cross-products. Exact parity with one pinned AVM
corpus remains useful evidence, but it must not define the normative boundary.
Use streams from AVM plus independent or hand-authored vectors, and require:

- exact reconstructed planes for the sample-preserving core;
- malformed-input rejection at each variable-length/count boundary;
- native, Linux no-libc, and WebAssembly parity, plus supported Windows build
   and import checks;
- explicit `spatial-still-8bit` capability advertisement so callers can reject
  out-of-profile streams before decoding.

## `spatial-still-8bit` encoder profile

This decoder contract is deliberately independent of an encoder preset. The
third encoder profile is now defined as `spatial-still-8bit` in
[`recipes/encodings.json`](recipes/encodings.json). Its references have not
been generated, while decoder admission is locked independently by
[`../tests/spatial-still-profile.json`](../tests/spatial-still-profile.json).
The recipe opens the broad still-image tool set while retaining
explicit choices where AVM requires a strategy rather than a capability switch.

Recommended experiment order:

1. Start from `target-baseline-8bit`: `cpu-used=1`, TCQ, ordinary trellis mode
   1, Intra-DIP, the full admitted filter pipeline, and perceptual delta-Q.
2. Test predefined quantization matrices independently. Compare PSNR, SSIM, and
   a perceptual metric at matched size and matched quality.
3. Test variance AQ independently against perceptual delta-Q. They are competing
   spatial bit-allocation strategies; do not assume that stacking them helps.
4. Test parity hiding independently, with decoder conformance fixtures before
   adopting it.
5. Test chroma delta-Q on images with saturated detail and sharp chroma edges.
6. Keep IntraBC enabled for measurement, but retain it in the final
   photographic profile only if corpus-wide or identifiable content-class gains
   justify the extra decoder surface.
7. Explore less aggressive ML partition/transform pruning only after the major
   syntax choices are settled; it raises encode time without requiring decoder
   work.
8. Keep film grain outside a sample-preserving profile until output semantics,
   metadata carriage, deterministic synthesis, and decoder support are defined.

For each candidate, record encoded bytes, encode time, decoded hashes, and
quality metrics over all prepared photographs. A winning profile must also
produce streams accepted byte-for-byte by native and WebAssembly decoders.

### Baya Weaver QP 152 pilot

An initial single-image pilot was run on 18 August 2026 using the pinned AVM
revision, Baya Weaver `le896` I420 input, QP 152, 16 encoder threads, and one
concurrent job. All three profiles were encoded on the same Apple M4 Max host
with the same clean release build. Wall time is one run, not a stable
performance benchmark. The benchmark executables were native arm64 binaries;
the AVM build reported `AVM_ARCH_AARCH64=1` and NEON support. It was not tuned
for the M4 specifically with an `-mcpu` flag.

| Profile | OBU bytes | Wall time | Average PSNR | SSIM |
| --- | ---: | ---: | ---: | ---: |
| `photographic-8bit` | 5,454 | 12.86 s | 37.2754 dB | 0.969834 |
| `target-baseline-8bit` | 11,144 | 739.09 s | 41.2219 dB | 0.982897 |
| `spatial-still-8bit` | 10,369 | 944.05 s | 40.9131 dB | 0.982575 |

Although each command requested 16 threads and row multithreading, user CPU
time divided by wall time was only 1.22, 1.16, and 1.14 average cores for the
legacy, target, and spatial runs respectively. Pinned AVM exposes little
parallel work for this one-frame high-effort encode, so the spatial wall time
primarily reflects single-core performance.

The regenerated legacy and target OBU and decoded-plane hashes exactly matched
their tracked locks. The spatial output was decoded by the clean pinned AVM
decoder but remains a benchmark artifact outside `images/reference`; no partial
reference lock was created.

At the same QP, `spatial-still-8bit` was 6.95% smaller than the target baseline
and took 27.7% longer, while average PSNR was 0.309 dB lower and SSIM was
0.000322 lower. Against the legacy profile it was 90.1% larger and 73.4 times
slower, with average PSNR 3.638 dB higher and SSIM 0.012741 higher.

These results do not establish rate-distortion superiority. AV2 QP does not
hold visual quality or output size constant across different tool and
bit-allocation configurations. The next meaningful comparison should sweep QP
for the target and spatial profiles and compare interpolated quality at matched
bytes, plus bytes at matched quality, across the complete photographic corpus.

## Maintenance checklist

When the pinned AVM revision or either recipe changes:

1. Diff `av2_key_val_args`, `global_args`, and their defaults in the pinned AVM.
2. Update rows for added, removed, renamed, or default-changed controls.
3. Keep explicit recipe settings distinct from inherited defaults.
4. Regenerate references and verify which allowed tools actually occur.
5. Update decoder-admission links when a feature subset changes.

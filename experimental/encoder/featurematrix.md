# AV2 still-image encoder feature matrix

This matrix compares the AV2 v1.0.0 coding tools relevant to a single still
picture with the production surface below `experimental/encoder/`. It reflects
the current implementation, not the eventual milestone plan. A generated CDF,
an experimental model, or a sequence flag by itself does not count as an
implemented coding tool.

Status meanings:

- **Implemented**: reachable from a production API or CLI path and covered by
  local reconstruction plus the pinned AVM and project decoders where it
  affects decoded pixels.
- **Partial**: a validator-backed, deliberately bounded form is reachable.
- **Disabled**: the encoder emits a conforming fixed/inferred off value and has
  no implementation for selecting the tool.
- **Not implemented**: no production encoding path exists.
- **Out of scope**: intentionally excluded by the single-picture experiment.

The primary evidence is [README.md](README.md), [plan.md](plan.md), and the
append-only [log.md](log.md). Detailed syntax derivations are under
[spec-notes/](spec-notes/), and the public production boundaries are in
[include/encoder/](include/encoder/).

| Area | AV2 feature | Status | Current encoder behavior and boundary |
|---|---|---|---|
| Stream | Raw AV2 OBU stream | **Implemented** | Emits one deterministic `.obu` stream; no complete golden stream is embedded. |
| Stream | Temporal Delimiter OBU | **Implemented** | Emitted before the coded picture. |
| Stream | Sequence Header OBU | **Implemented** | Named fields, extensible-OBU termination, and trailing bits are generated. |
| Stream | Frame/Closed Loop Key OBU | **Implemented** | Frame header and one tile payload are emitted in a Closed Loop Key frame OBU. |
| Stream | Separate Frame Header and Tile Group OBUs | **Not implemented** | The encoder uses the combined frame OBU form only. |
| Stream | OBU extension, temporal layer, and spatial layer IDs | **Disabled** | Single layer only; OBU extension fields are not used. |
| Stream | Canonical outer LEB128 lengths | **Implemented** | Every OBU length is derived and bounded. |
| Stream | Metadata OBU | **Not implemented** | No codec metadata OBU is emitted. |
| Sequence | Single-picture sequence | **Implemented** | `single_picture_header_flag` is fixed on; timing and reference-related state is inferred accordingly. |
| Sequence | Sequence profile | **Partial** | Fixed profiles are emitted for the admitted 8-bit I400/I420/I444 formats; profile selection is not a user-facing general facility. |
| Sequence | Sequence level | **Partial** | A fixed derived level index is selected for the admitted picture-size classes; general level/tier conformance selection is absent. |
| Sequence | Timing info, decoder model, and operating points | **Disabled** | Omitted by the single-picture/layer-zero profile. No frame rate or presentation duration is declared. |
| Sequence | Scalability and dependency layers | **Out of scope** | One independently decodable picture and one layer only. |
| Sequence | 8-bit samples | **Implemented** | Production I400, I420, and I444 paths are 8-bit. |
| Sequence | 10-bit and 12-bit samples | **Not implemented** | Higher-bit-depth input, transforms, quantization, reconstruction, and signaling are absent. |
| Sequence | I400 monochrome | **Implemented** | Every width and height from 1 through 1440 at qindex 1 through 255. |
| Sequence | I420 4:2:0 | **Implemented** | Every width and height from 1 through 1440, including odd ceil-divided chroma geometry. |
| Sequence | I444 4:4:4 | **Implemented** | Every width and height from 1 through 1440. |
| Sequence | Other chroma formats/subsampling | **Not implemented** | No additional `chroma_format_idc` surface is admitted. |
| Sequence | Color description, range, and chroma sample position choices | **Partial** | The encoder emits one fixed color configuration per admitted format; arbitrary colorimetry and siting controls are absent. |
| Sequence | Maximum frame dimensions | **Implemented** | Dimensions are encoded from the input; the validated production bound is 1..1440 on each axis. Streams through 896 retain their frozen level selection; the expanded band uses level index 4. |
| Sequence | Conformance/display cropping | **Disabled** | The coded boundary is edge-extended internally, but no independent sequence crop window is selected. Decoder output uses the declared active dimensions. |
| Frame | Closed Loop Key still picture | **Implemented** | One intra-only, immediately displayed, independently decodable frame. |
| Frame | Other key/intra frame arrangements | **Not implemented** | No alternative frame lifecycle or multi-frame sequence is produced. |
| Frame | Inter frames and inter prediction | **Out of scope** | No references, motion vectors, compound prediction, interpolation filters, or inter modes. |
| Frame | Reference slots, primary reference, and refresh control | **Disabled** | Inferred/fixed by the single Closed Loop Key picture. |
| Frame | Order hints and display reordering | **Disabled** | Single immediately displayed picture only. |
| Frame | Show-existing-frame and hidden frames | **Out of scope** | No multi-frame display model. |
| Frame | Frame-size override and reference-derived size | **Disabled** | Frame dimensions come directly from the sequence/input. |
| Frame | Super-resolution/upscaling | **Disabled** | Coded and displayed dimensions are not related by super-resolution. |
| Frame | Screen-content tool selection | **Partial** | Required fixed screen-content syntax is emitted, but no screen-content coding search is implemented. |
| Frame | Intra block copy (IntraBC) | **Disabled** | `allow_intrabc` is off; no block-vector search or syntax exists. |
| Frame | CDF updates | **Implemented** | Updates remain enabled and mutable CDF state persists across transforms, leaves, and superblocks in the tile. |
| Frame | Segmentation and sequence segmentation | **Disabled** | No segments, feature data, or segment maps are emitted. |
| Frame | Film grain synthesis parameters | **Disabled** | `film_grain_params_present` is off and no grain model is produced. |
| Tiling | One tile | **Implemented** | The whole picture is one tile with tile-persistent CDF and above state. |
| Tiling | Multiple tile rows/columns and tile groups | **Not implemented** | No parallel tile layout, tile-size signaling, or cross-tile state choices. |
| Blocks | 64x64 superblocks | **Implemented** | Superblocks are visited row-major; above state persists and left state resets by superblock row. |
| Blocks | 128x128 and 256x256 superblocks | **Disabled** | Sequence selection is fixed to 64x64 superblocks. |
| Blocks | Boundary-implied partitioning | **Implemented** | Active dimensions are rounded to the normative MI boundary and out-of-frame branches are inferred. |
| Blocks | Rectangle partition traversal | **Implemented** | The fixed depth-first tree uses legal horizontal/vertical splits and 8x8, 8x16, 16x8, and 16x16 luma-reference leaves. |
| Blocks | General partition search | **Not implemented** | There is one deterministic legal tree, not RD selection among legal partition candidates. |
| Blocks | Extended and uneven partition choices | **Partial** | Sequence syntax and the fixed tree use the required rectangle grammar, but general extended/uneven candidate search is absent. |
| Blocks | Square split at 128x128/256x256 | **Not implemented** | Those superblock sizes and their square-split choices are not used. |
| Blocks | Partition contexts across superblocks | **Implemented** | Above/left partition state and normative row resets are implemented for the fixed production traversal. |
| Intra prediction | Luma DC prediction | **Implemented** | Prediction uses reconstructed top/left neighbors; unavailable edges use the normative neutral behavior. |
| Intra prediction | Chroma DC prediction | **Implemented** | Separate U/V reconstructed-neighbor prediction is implemented for I420 and I444. |
| Intra prediction | Directional/angular modes | **Partial** | Explicit production `--search directional` and `--search directional-exact` select exact luma DC/V/H trials for I400/I420/I444 across the 1..896 production bound. Arbitrary angles, chroma directional modes, and default enablement remain deferred. |
| Intra prediction | Smooth, Paeth, and other non-DC modes | **Partial** | Explicit `--search intra` admits luma Smooth/Smooth-V/Smooth-H/Paeth for the fixed 8x8 transform leaves in images up to 64x64. The 224-case full-stream gate has 32 incremental wins, 5,314 vs 5,326 bytes, Y SSE 43,593,650 vs 43,744,028, exact AVM/project parity, and all four modes selected. Chroma-mode search and larger-image smooth trials remain deferred; PNG's Paeth filter is unrelated. |
| Intra prediction | Directional intra prediction extensions (DIP) | **Disabled** | No DIP mode is selected or searched. |
| Intra prediction | Edge filtering/upsampling and MRLS | **Partial** | Cardinal V/H returns before filtering/upsampling; directional syntax explicitly emits MRLS index 0. Filtered/upsampled angular prediction and nonzero MRLS are absent. |
| Intra prediction | Chroma from luma (CfL) | **Disabled** | Color syntax explicitly selects non-CfL; chroma is coded independently. |
| Intra prediction | Multi-hypothesis cross-component prediction (MHCCP) | **Disabled** | No cross-component predictor is generated. |
| Intra prediction | Intra block prediction (IBP/edge IBP) | **Disabled** | The production M3 sequence disables IBP; no IBP references are formed. |
| Intra prediction | Palette coding | **Disabled** | Palette mode is never selected. A materialized palette CDF row is not a palette encoder. |
| Intra prediction | Forward skip coding (FSC) | **Disabled** | Production mode syntax selects FSC off. |
| Intra prediction | Spatially divided prediction and related chroma partitions | **Not implemented** | Luma/chroma use the fixed separate DC trees only. |
| Transform | 8x8 transform grid for luma | **Implemented** | M3 reconstructs every fixed leaf as one or more 8x8 DC transforms. |
| Transform | Chroma square and rectangular transforms | **Implemented** | I420 uses the admitted 4x4/4x8/8x4 classes and I444 uses 8x8/8x16/16x8 classes as required by leaf geometry. |
| Transform | Transform partition syntax | **Implemented** | Fixed square/horizontal/vertical transform splitting is emitted in decoder order. |
| Transform | Transform partition search | **Not implemented** | Transform subdivision follows fixed leaf geometry. |
| Transform | DCT_DCT | **Partial** | Full DC behavior is implemented. Explicit I400/I420/I444 `--search bounded` reaches general luma 8x8 DCT_DCT through 896; chroma remains DC-only and the mode is non-default because its streams are larger. |
| Transform | ADST, flip-ADST, identity, and other transform types | **Not implemented** | No general transform-type search or kernels. |
| Transform | Intra secondary transform (IST) | **Not implemented** | Some sequence paths enable availability, but production coding never selects or reconstructs IST. |
| Transform | Reduced transform set | **Disabled** | Fixed frame syntax is emitted; there is no transform-set selection policy. |
| Coefficients | All-zero transform block/skip syntax | **Implemented** | Context-coded all-zero decisions and state updates are covered. |
| Coefficients | DC coefficient coding | **Implemented** | Signed levels, EOB, base, BR, Golomb/remainder, sign contexts, and immediate neighbor-state commits are implemented. |
| Coefficients | AC coefficient coding | **Partial** | Explicit I400/I420/I444 `--search bounded` codes signed luma 8x8 coefficients with EOB/base/BR/remainder/sign syntax and exact reconstruction. U/V remain the existing DC-only path. |
| Coefficients | General scans, EOB classes, base/BR/remainder AC syntax | **Partial** | General luma 8x8 syntax is production-reachable in all three formats; other transform sizes and chroma AC remain absent. |
| Coefficients | U-to-V coefficient context coupling | **Implemented** | Chroma skip/context selection includes normative U-to-V coupling. |
| Coefficients | Transform coefficient quantization (TCQ) | **Disabled** | No TCQ state, decisions, or reconstruction path. |
| Coefficients | Coefficient parity hiding | **Disabled** | No parity-hiding search or syntax. |
| Quantization | Frame base qindex | **Implemented** | Configurable qindex 1..255; default 100. The value drives frame syntax, dequantization, search bounds, and CDF group. |
| Quantization | Lossless qindex 0 | **Not implemented** | Rejected because lossless transform and frame semantics are not emitted. |
| Quantization | Plane and segment quantizer deltas | **Disabled** | One base qindex is used; no per-plane or per-segment delta selection. |
| Quantization | Delta-Q maps | **Disabled** | No block-level quantizer variation. |
| Quantization | Quantization matrices | **Disabled** | Default no-matrix behavior only. |
| Quantization | Equal AC/DC quantizer mode | **Implemented** | The bounded DC profile uses the fixed equal-quantizer configuration it signals. |
| Entropy | Forward range encoder and legal termination | **Implemented** | Bounded arithmetic output, carry handling, exact capacity failure, and finish behavior are independently modeled. |
| Entropy | Raw syntax helpers | **Implemented** | Literals, signed values, UVLC/SVLC, `ns`, subexponential coding, recentering, alignment, and trailing bits are covered. |
| Entropy | Normative default CDF materialization | **Partial** | Hash-locked rows cover every symbol used by M1/M3/M4, not the complete AV2 syntax universe. |
| Entropy | Adaptive CDF update parameters | **Implemented** | All 125 parameter combinations and update-count boundaries are tested at the entropy-foundation level. |
| Entropy | Syntax context modeling | **Partial** | Partition, transform, DC coefficient, sign, and chroma contexts for the admitted profile are implemented; contexts for absent tools are not. |
| Filtering | Deblocking filter | **Disabled** | The production profile does not apply a reconstructed-pixel deblocking stage or search filter levels. |
| Filtering | Guided deblocking filter (GDF) | **Disabled** | Sequence tool is off; no kernel or signaling path. |
| Filtering | Constrained directional enhancement filter (CDEF) | **Disabled** | Sequence tool is off; no strength search or local reconstruction. |
| Filtering | Loop restoration | **Disabled** | Restoration is off; Wiener and self-guided restoration kernels/search are absent. |
| Filtering | Cross-component sample offset (CCSO) | **Disabled** | Sequence tool is off; no filter or parameter search. |
| Filtering | Cross-tile filter control | **Disabled** | Only one tile exists and no in-loop filter is enabled. |
| Inter-only tools | Reference mode, skip mode, BAWP, warp, and global motion | **Out of scope** | Inferred off for the intra-only Closed Loop Key picture. |
| Search | Pixel-dependent DC level search | **Implemented** | Every active input sample contributes to deterministic SSE minimization; edges are replicated only outside the active crop. |
| Search | General transform, intra-mode, and partition RD search | **Partial** | Explicit complete-stream intra search selects among DC/V/H and, through 64x64, Smooth/Smooth-V/Smooth-H/Paeth with fresh writer/CDF state, byte and per-plane-SSE non-regression, and stable ties. Bounded AC search exists separately; combined transform/mode search and partition search are absent. |
| Search | Rate control and target-size encoding | **Not implemented** | Qindex is supplied directly. A tools-only bounded equal-size qindex experiment is validator-backed but blocked by the 29-byte AC-subset floor; it does not change the production CLI. |
| Reconstruction | Local closed-loop reconstruction | **Implemented** | Prediction, dequantization, inverse DC transforms, clipping, chroma state, edge extension, and active crop match both validators. |
| Reconstruction | General AC inverse reconstruction | **Partial** | Explicit I400/I420/I444 `--search bounded` has scalar inverse luma 8x8 DCT_DCT reconstruction for all 64 coefficient positions and matches both validators. Other transform sizes/types and chroma AC reconstruction are absent; the older fixed M4 experiment remains separate. |
| Metrics | Per-plane SSE and PSNR | **Implemented** | Active-pixel distortion is reported; chroma results retain per-plane SSE. |
| Metrics | SSIM | **Not implemented** | No local deterministic SSIM implementation. |
| Container | AVIF/HEIF (BMFF) muxing | **Out of scope** | Output is elementary `.obu` only. |
| Container | IVF muxing | **Out of scope** | No alternate stream container. |
| Container | ICC, Exif, XMP, alpha, depth, and auxiliary images | **Out of scope** | PNG alpha is ignored at input; no alpha plane or still-image metadata is preserved. |
| Input (non-bitstream) | Raw planar I400/I420/I444 | **Implemented** | Exact-size, checked-stride 8-bit plane loading with odd I420 geometry. |
| Input (non-bitstream) | PNG and BMP decoding | **Partial** | Dependency-free documented 8-bit subsets convert grayscale to I400 and color/palette/RGBA to I444; unsupported variants fail explicitly. |
| Input (non-bitstream) | Other image and video input formats | **Not implemented** | No JPEG, WebP, AVIF, Y4M, or video sequence input. |

## Current practical profile

The useful production profile is one 8-bit, one-tile, intra-only Closed Loop
Key picture in I400, I420, or I444, with both axes in 1..1440 and qindex in
1..255. It uses a deterministic rectangle-only 64x64-superblock traversal,
DC prediction, DC-only transform coefficients by default, no in-loop filters,
and no metadata or container. General 8x8 luma AC is reachable for
I400/I420/I444 only through the explicit default-off bounded search; the
locked quality/rate gate has not admitted it as the default. U/V remain
DC-only.

When a new coding tool becomes reachable, this matrix should be updated only
after its local reconstruction and emitted syntax pass the required validator
gate. Planned work in [plan.md](plan.md) and retained research under
[spec-notes/](spec-notes/) remains **Not implemented** until that point.

## Prioritized still-image quality roadmap

Scores are 1 (low) through 5 (high). Complexity, portability/no-libc risk, and
validator burden are costs; expected RD benefit is upside. These are planning
rankings only and do not change any implementation status above.

| Priority | Feature | Complexity | Expected RD benefit | Portability / no-libc risk | Validator burden | Decision |
|---:|---|---:|---:|---:|---:|---|
| 1 | Broader scalar 8x8 DCT_DCT AC coding | 4 | 5 | 1 | 4 | Validator-proven behind `--search bounded`; default admission remains blocked because the equal-rate and hard-budget gates miss their required wins and byte budget. |
| 2 | Horizontal/vertical intra prediction | 2 | 4 | 1 | 3 | Implemented explicitly by `--search directional`; the 90-case gate has 12 committed wins, lower bytes and plane SSE, exact parity, and a 262.186-second runtime. Default remains off. |
| 3 | U/V plane quantizer deltas | 2 | 3 | 1 | 2 | Next unimplemented tranche item: low syntax/search cost and direct chroma allocation control at fixed total bytes. |
| 4 | Smooth and Paeth prediction | 3 | 3 | 1 | 3 | Implemented explicitly by `--search intra`: the full-stream 224-case gate has 32 wins, 12 fewer bytes, lower luma SSE, exact decoder parity, and about 1.02x runtime. Chroma-mode search and trials above 64x64 remain deferred. |
| 5 | Chroma from luma (CfL) | 4 | 4 | 2 | 4 | Defer until broader luma AC and plane quantizer control provide a trustworthy chroma comparator. |
| 6 | Deblocking | 4 | 3 | 2 | 5 | Defer: frame-wide filtered reconstruction, edge rules, level search, and three-way hash parity add high validation cost. |
| 7 | General partition search | 5 | 5 | 2 | 5 | Defer until several coding modes have exact costs; otherwise search complexity grows before useful choices exist. |
| 8 | Rate control / target bytes | 4 | 1 | 2 | 3 | Defer: it selects qpoints but supplies little intrinsic RD gain before the constant-q curve improves. |

The frozen comparator is
`metrics/encoder-quality-baseline.json` (90 cases: 36 full prepared and 54
targeted). The first tranche and exact measurable gates are defined in
[plan.md](plan.md). Baseline inclusion does not imply that any roadmap feature
is implemented.

### Reachable 8x8 AC surface

| Surface | Status | Evidence |
|---|---|---|
| I400/I420/I444, explicit `--search bounded` | **Production-reachable / validator-proven** | Scalar forward/inverse luma DCT_DCT, all 64 coefficient positions, adaptive syntax, and caller workspace are reachable. The focused 12-case gate has exact local, pinned-AVM, and project-decoder reconstruction parity with 11 luma wins. U/V remain DC-only. |
| I400 default | **Legacy DC-only** | Default is `--search off`; legacy streams remain selectable. |
| I420/I444 explicit luma AC | **Implemented** | The CLI and typed caller-workspace API apply bounded AC search to Y while preserving the existing DC-only U/V reconstruction. |
| Explicit luma AC color gate | **Passed** | All 12 I400/I420/I444 cases pass with exact local/AVM/project parity and 11 luma wins. The streams cost 1,686 vs 777 baseline bytes, so the feature remains explicit and default-off; see `metrics/ac8x8-color-gate.json`. |
| Default-on quality feature | **Blocked** | Locked byte and minimum-win gates fail; see `metrics/ac8x8-production.json`. |
# Encoder experiment log

This is an append-only record of decisions, commands, pins, hashes, gate
results, blockers, and restart points. Do not replace exact values with links
or “latest”.

## 2026-08-16 — Smooth/Paeth production intra search

- **Status:** DONE. Todo `encoder-smooth-paeth-production`.
- **Surface:** explicit `--search intra`, default remains `off`;
  `directional` and `directional-exact` remain compatible.
- **Admitted:** luma DC/V/H/Smooth/Smooth-V/Smooth-H/Paeth for fixed 8x8
  transform leaves in images up to 64x64. Larger images use the directional
  runtime policy; legacy 8x8 and small-I420 syntax classes use DC fallback.
  Chroma mode search is deferred and remains DC.
- **Syntax/tools:** exact luma mode-index contexts, FSC=0, MRLS=0 for V/H
  only, CfL=0, deterministic stable ties, fresh complete-stream writer/CDF
  trials, per-plane SSE and byte non-regression.
- **224-case gate:** 5,314 vs 5,326 bytes; Y SSE 43,593,650 vs 43,744,028;
  U/V unchanged at 23,596,698/23,493,845; 32 wins. Winners are Smooth 12,
  Smooth-V 14, Smooth-H 2, Paeth 4. Runtime 783,160,668 vs 768,759,740 ns.
- **90-case gate:** 276,756 bytes, Y/U/V SSE
  9,609,637,634/916,732,746/910,298,990, six byte wins over directional,
  deterministic and dual-decoder exact. Aggregate median runtime 263.108 s,
  per-case median 158.129 ms, worst 52.981 s, max workspace 7,263,744 bytes.
- **Evidence:** `metrics/encoder-smooth-paeth-production.json`,
  `metrics/encoder-intra-production.json`, and
  `metrics/smooth-paeth-model.json`.

## 2026-08-16 — whole-image AC hard-budget model

- **Status:** BLOCKED for todo `encoder-ac8x8-hardbudget-model`; production
  core was not changed.
- The deterministic model evaluates complete streams after every block
  position-prefix choice, including decode-order prediction, adaptive CDFs,
  range flush, and OBU framing, then applies an aggregate knapsack.
- Locked selection: 786 bytes, Y SSE 191,088,000, 0/9 subset wins, 0% gain.
  It is the frozen DC selection with local/pinned-AVM/project parity.
- Best reachable 8%-gain frontier: 7/9 wins, subset SSE 164,950,576,
  1,048 aggregate bytes, 262 bytes over budget. The qindex-180 horizontal and
  vertical line cases have no winning candidate at any modeled rate.
- Runtime: 288 frontier states in 56,962,202,090 ns; worst case
  6,849,728,178 ns.
- Exact restart requirement: add wins for both qindex-180 line cases and save
  at least 262 bytes, or find another 9-win, >=8%-gain frontier within 786
  bytes.

## 2026-08-16 — Equal-size AC qindex trials

- **Status:** BLOCKED. Todo `encoder-ac8x8-equalsize-qtrial`.
- **Policy:** per comparison, reproduce the requested-qindex DC-only stream,
  then try at most 16 monotonically increasing qindices through 255 with
  explicit `--search bounded`. Admit only complete streams at or below that
  case's DC byte count; rank by dual-decoder Y SSE; retain strict DC fallback.
- **Quality semantics:** requested qindex is a quality anchor and byte-budget
  reference, not a promise that the selected stream signals the same qindex.
  Any selected higher qindex is reported as nominally lower quality.
- **Evidence:** `metrics/ac8x8-equalsize-qtrial.json`; 18 targeted and 12
  prepared I400 comparisons from the locked 90-case baseline, complete OBU
  bytes, pinned-AVM/project reconstruction parity, selected-stream reruns,
  trial counts, q shifts, and runtime.
- **Gate:** every selected case is individually no larger than DC-only and no
  prepared case regresses. The locked nine-case AC subset does not reach 8%
  aggregate SSE gain or nine wins.
- **Blocker:** checkerboard and two-pixel line DC streams are only 29 bytes.
  Every fitting tested AC trial has equivalent DC-only reconstruction, so
  qindex movement cannot clear the complete-stream syntax-rate floor. In
  addition, pinned AVM rejects 33 byte-fitting non-selected AC trials,
  concentrated in prepared images and diagonal-edge q60/q100.

## 2026-08-15 — M3 chroma boundary half-grid completion

- **Status:** PASS. Todo `encoder-chroma-boundary-halfgrid`.
- **Bounds:** every 8-bit I420 and I444 width/height pair in 1..896 at every
  qindex 1..255: 802,816 dimension pairs per format. Only zero dimensions,
  unsupported formats, qindex 0/256, and an axis above 896 are rejected.
- **Root cause:** the separate chroma tree used `4*ceil(axis/16)` MI cells.
  Pinned AVM uses the frame's 8-pixel-aligned boundary,
  `2*ceil(axis/8)`. The difference exists exactly for `16k+1..16k+8`.
  Those classes introduce 16×8, 8×16, and 8×8 luma-reference chroma leaves.
- **Normative syntax:** CHROMA `BLOCK_8X8` is forced `PARTITION_NONE` and
  consumes no `do_split` symbol. I420 rectangular transforms are 4×8/8×4,
  skip context 1, EOB-32; I444 rectangular transforms are 8×16/16×8,
  skip context 2, EOB-128. Exact-width above and exact-height left state feed
  skip/sign and DC prediction. Rectangular inverse input uses 2896/4096
  scaling. Existing U-to-V coupling, tile-persistent above/CDF state, and
  superblock-row left resets remain unchanged.
- **Evidence:** required probes 8×9, 17×16, 24×24, and 64×65 pass generated
  non-flat I420/I444 parity. Required real sizes 896×424, 896×504, 896×598,
  and 598×896 pass both formats. The matrix contains 118
  local == pinned AVM == project parity cases, seven deterministic reruns, and
  exhaustive admission checks through 897 on both axes.
- **Frozen vectors:** 20 streams; manifest SHA-256
  `fbbd85fbb088a2824b58ce9f068674cc3fb154371bd5ec73c5a5d68aefcb3987`.
  The original I400, four 8×8 chroma, and four odd-I420 hashes are unchanged.
- **Independent model:** `tools/m3_chroma_boundary_model.py` freezes forced
  axes, leaf shapes, rectangular transform context/EOB classes, and the
  802,816-pair count independently of the C implementation.
- **Gates:** generated CDF/input checks, full encoder tests and audits, all 32
  project CTests, and deterministic reruns pass.
- **Blockers:** none within the bounded 8-bit DC-only profile.

## 2026-08-15 — Encoder input integration

- **Status:** PASS. Todo `encoder-input-integration`.
- **CLI:** exactly one ordinary `.i400`, `.i420`, `.i444`, `.bmp`, or `.png`
  input succeeds within the admitted subset, defaults to qindex 100, and
  replaces the final extension with sibling `.obu`; `-o`/`--output`,
  `--qindex`, explicit raw dimensions, and every legacy mode remain supported.
- **Formats:** 8-bit I400, I420, and I444, with every width/height pair in
  1..896 admitted by the DC-only encoder. Raw I420 uses ceil-divided chroma.
  BMP/PNG grayscale maps to I400; RGB, palette, and RGBA map deterministically
  to full-resolution I444. Alpha is ignored, not composited.
- **Loading/lifecycle:** exact-size raw mmap, bounded arbitrary-size image
  mmap, checked decode/output/scratch pages, typed plane validation, workspace
  query/encode wiring, atomic rename, input inode collision rejection, and
  complete mmap/page cleanup on every exit. The production ELF remains
  freestanding, static, no-libc, and has no undefined symbols.
- **Discovery/errors:** prepared names and sibling manifests supply raw
  geometry; missing, ambiguous, or conflicting metadata is explicit and
  dimensions can be supplied manually. Unsupported image subsets, malformed
  files, exact-size failures, bounds, output collisions, allocation/encoding,
  and write failures have distinct diagnostics/status classes.
- **Coverage:** five extension paths plus grayscale/color image mapping, odd
  3×3/3×5 geometry, qindex/output overrides, deterministic reruns, atomic
  failures, and real prepared 896×424 PNG/I420. Integration tests contain
  2 methods and the mandatory Python gate totals 130 tests. Local, pinned AVM,
  and project reconstruction are exact for generated cases; real prepared
  outputs have pinned AVM/project exact parity.
- **Gates:** generated checks; full mandatory encoder gate; 32/32 project
  CTests; dependency, static ELF, dynamic-segment, and undefined-symbol audits;
  legacy golden preservation; and cleanup all pass.
- **Blockers:** none.

## 2026-08-15 — M3 full chroma admission

- **Status:** PASS with unsupported boundary classes kept closed.
- **Milestones:** `encoder-chroma-full-admission`,
  `encoder-chroma-i420-i444`.
- **Public bounds:** qindex 1..255 and axes at most 896. The general I420/I444
  axis set is `16k+9..16k+16`, `k=0..55`; axes are independent. I420 also
  admits every 1..8 by 1..8 dimension, while I444 additionally admits exactly
  8×8. This is 200,768 I420 and 200,705 I444 dimension pairs.
- **Reconciliation:** retained the odd-I420 single-leaf path and byte-exact
  8×8 path; generalized active/coded layout and multileaf routing; made
  chroma search use the syntax forced-axis rule; and selected whole versus
  horizontally split chroma roots from each partial superblock's remaining
  rounded extent. The latter fixes 48-, 16-, and 32-pixel superblock tails.
- **Coverage:** 94 local == pinned AVM == project parity cases (53 I420,
  41 I444), 22 closed-profile negatives, and four deterministic reruns.
  Cases cross mixed odd/even axes, 8/16/64/128 boundaries, qindex contexts
  1/90/91/140/141/190/191/255, flat/generated-spatial/color-asymmetric
  inputs, separate U/V residuals, U-to-V coupling, partial/full superblocks,
  crop and per-plane SSE, workspace alignment/capacity/alias/error atomicity,
  and the 896 bound.
- **Prepared dimensions:** 896×448, 896×672, and 896×896 pass. 896×424,
  896×504, 896×598, and 598×896 remain closed.
- **Workspace maxima at 896×896:** I420 4,855,296 bytes with 1,204,224-byte
  active reconstruction; I444 7,263,744 bytes with 2,408,448-byte active
  reconstruction. Both reserve an 803,072-byte stream and report transform
  counts 12,544/3,136/3,136.
- **Frozen evidence:** manifest SHA-256
  `36fe698ac6f8d78c056eaf9caa720099fa288122fe11fde95a47442fc9437e7e`.
  New OBU hashes are `feaef728740314cfedce5d666c4a71b92ec3a25a69e99a93fd88c5c84f6f33df`
  (I420 25×32 spatial),
  `5beae6fd123147481b4f4ff35f6f8c096caab83af9963c14827b2b0b48f31914`
  (I444 47×48 asymmetric),
  `3593db3a1d7b468f5d9efcac6c5b79efa4be9be3afaba0436e293f0dfa47b099`
  (I420 896×672), and
  `8311fefc5898230395899bd18565b7427674a3384792123fde8b8e9fae9a1414`
  (I444 896×896).
- **Pins:** AVM decoder
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`;
  project decoder
  `7d17f598f2cdffe7dfb4ab46982cf5c4822215a5f429cb39e7748a60ce5ef535`.
- **Preservation:** mandatory encoder tests retain all I400 goldens; the
  original four 8×8 and four odd-I420 frozen stream hashes are unchanged.
- **Gates:** generated CDF/input checks; native chroma workspace/metric tests;
  full encoder gate (125 Python tests plus native, oracle, static ELF, and
  dependency audits); all 32 project CTests; two deterministic chroma reruns.
- **Blocker:** general axes `16k+1..16k+8` produce a chroma transcript rejected
  by pinned AVM (project-only acceptance is not admission evidence). Those
  profiles and the affected prepared sizes remain `ENC_UNSUPPORTED`.

## 2026-08-15 — M3 multileaf chroma contexts

- **Status:** PASS.
- **Milestone:** `encoder-chroma-multileaf-contexts`.
- **Formats:** even-dimension I420 and I444, preserving the byte-exact legacy
  8×8 path and the odd-I420 single-leaf API.
- **Traversal:** luma tree then chroma tree per 64×64 superblock; search,
  reconstruction, and syntax share depth-first leaf/transform order.
- **State:** reconstructed-neighbor DC prediction; separate U/V above/left
  coefficient and sign contexts; normative U-to-V skip coupling; adaptive
  CDF and above persistence; left resets at superblock rows.
- **Parity:** flat and generated non-flat 64×64, 128×64, and 128×128 I420/I444
  plus qindex 1, 90, 91, 140, 141, 190, 191, and 255 are byte-identical in
  local, pinned AVM, and project reconstruction. The chroma matrix contains
  58 parity cases: 35 I420 and 23 I444.
- **Project decoder:** chroma DC IBP is now conditional on `edge_ibp`; disabled
  sequence IBP no longer changes multileaf I420 reconstruction. Updated
  project decoder SHA-256:
  `7d17f598f2cdffe7dfb4ab46982cf5c4822215a5f429cb39e7748a60ce5ef535`.
- **Preservation:** frozen I400, legacy 8×8 chroma, and odd-I420 vectors remain
  unchanged.
- **Gates:** generated checks, the full experimental encoder gate (122 Python
  tests plus native/audit/oracle checks), and all 32 project CTests pass.

## 2026-08-15 — Raw planar input abstractions

- **Status:** PASS.
- **Boundary:** dependency-free input metadata and loading, separate from AV2
  syntax and the I400-only CLI integration.
- **Formats:** typed 8-bit I400, I420, and I444 plane descriptors with explicit
  byte spans, width, height, and stride.
- **Geometry:** checked products and accumulated offsets; odd I420 dimensions
  use ceil division independently on both axes. The existing 16384-per-axis and
  256 MiB input bounds remain authoritative.
- **Files:** read-only platform mappings require the exact packed planar size;
  short inputs and trailing bytes both fail. Close unmaps once and clears all
  borrowed pointers. External image validation rejects malformed geometry,
  size disagreement, overlapping plane spans, and pointer-range overflow.
- **Discovery:** only the existing prepared `manifest.json` layout and exact
  `source-WxH-{gray8,yuv420p8,yuv444p8}.{i400,i420,i444}` names carry implicit
  dimensions. Generic raw filenames are never guessed.
- **Provenance:** reused the already recorded encoder-local CC0 newos checked
  arithmetic/span/ownership patterns; copied no new external source.
- **Validation:** native C geometry/load/cleanup/alias/overflow tests, Python
  manifest/name tests, CLI short/excess checks, full encoder unit suite, strict
  dependency audit, and static ELF audit pass.

## Entry template

```text
Date/time:
Status: PLANNED | IN PROGRESS | PASS | BLOCKED
Milestone:
Source revision:
Compiler and flags:
Pinned avmdec identity/hash/command:
Pinned repository decoder identity/hash/command:
Prepared input description and SHA-256:
Encoder command:
Output .obu SHA-256 / bytes / bpp / bitrate:
Decoded plane SHA-256 (avmdec):
Decoded plane SHA-256 (our decoder):
PSNR by plane / weighted:
SSIM by plane / weighted, or "not implemented":
Median/worst time and pixels/s:
Workspace peak and static binary bytes/SHA-256:
Malformed-output checks:
Gate result:
Decision or blocker:
Next restart command/boundary:
```

## 2026-08-15 — Freestanding BMP/PNG input module

- **Status:** BLOCKED (implementation and targeted gates pass; full shared
  suite has unrelated generated-lock mismatches).
- **Milestone:** `encoder-input-bmp-png`.
- **Implementation:** dependency-free BMP/PNG, CRC-32, zlib/DEFLATE,
  unfiltering, and deterministic typed I400/I444 conversion. Storage is
  caller-owned and CLI/codec integration remains separate.
- **Subset:** BMP BI_RGB grayscale palette/24-bit BGR with both row
  directions and padding; PNG non-interlaced 8-bit grayscale/RGB/RGBA/palette,
  all filters, split IDAT, and stored/fixed/dynamic DEFLATE.
- **Alpha:** ignored, with documented full-range integer RGB conversion.
- **Provenance:** newos CC0 revision
  `972ca695b3f4b401a00f596ed78172dd1ceb03cf`; exact files and local boundary
  are recorded in `PROVENANCE.md`.
- **Tests:** reproducibly generated fixtures, malformed/truncated/CRC/filter/
  DEFLATE/capacity/overflow/unsupported cases, both BMP orientations, and an
  optional prepared 896×896 RGB PNG.
- **Isolation:** the module compiles under freestanding warning-as-error flags;
  existing static nolibc ELF and dependency audits remain mandatory.
- **Gate result:** image-input tests, prepared PNG decode, generated-fixture
  check, freestanding compile, and dependency/static ELF audits pass.
  `make test-unit` reaches 117 Python tests but currently fails four unrelated
  CDF/vector locks changed concurrently (`test_cdf_subset`,
  `test_m3_boundary_partition_transcript`, `test_m3_multileaf_cdf_model`, and
  `test_m3_edge_coeff_model`). No image-input test fails.

## 2026-08-15 — Architecture plan

- **Status:** PASS
- **Milestone:** planning boundary before M0
- **Result:** `plan.md` defines strict encoder/decoder isolation, CC0,
  freestanding C11, explicit caller-owned allocation, dependency-free static
  nolibc Linux x86-64 first, portable future backends, prepared planar input,
  and `.obu`-only output.
- **Coverage:** concrete gates and modules are planned from bit/OBU writing and
  range/CDF syntax through constant/DC encoding, transforms/quantization,
  chroma, partition/mode search, filters/restoration choices, rate control,
  validation, and metrics.
- **Validation contract:** pinned `avmdec` and repository decoder identities,
  exact input/output/decoded hashes, bitrate and byte count, local PSNR and
  optional local SSIM, speed, workspace, binary size, determinism, and malformed
  stream checks are mandatory.
- **Files changed:** only `experimental/encoder/plan.md` and
  `experimental/encoder/log.md`.
- **Restart boundary:** begin M0 by creating only the proposed local substrate,
  build, and tests. Do not integrate with any decoder or repository build.

## 2026-08-15 — Exact M1 raw syntax research

- **Status:** PASS for raw-bit syntax; entropy writer remains the next boundary.
- **Milestone:** M1 syntax prerequisite.
- **Normative source:** AV2 Bitstream & Decoding Process Specification v1.0.0,
  §§4.11.3, 4.11.8, 5.2, 5.4, 5.18, 5.20, 8.2, and 8.3, including official
  attachments where referenced.
- **Target:** 8×8, 8-bit I400, one constant-128 Closed Loop Key still. AVM also
  accepted 1×1 through 4×4, but 8×8 is the smallest practical real leaf block.
- **Result:** `spec-notes/m1-syntax.md` resolves UVLC/ns, Annex-B LEB128 length
  semantics, OBU headers, complete transmitted Sequence Header and frame-header
  order, tile/quant/filter ordering, forced 64→32→16→8 partition traversal,
  intra skip/mode inference, and range-decoder initialization/exit rules.
- **Corrected assumption:** AV2 has no AV1-style in-header OBU size flag. The
  outer LEB128 covers header plus payload. The decoding specification permits
  multiple valid forward range-writer terminations; AVM byte identity is not a
  normative requirement.
- **Executable evidence:** `tools/m1_syntax.py` and
  `tests/test_m1_syntax.py`. Every Sequence Header bit and every 32-bit aligned
  frame-header bit is mutation-checked.
- **Black-box validation:** pinned AVM and the independent project decoder both
  accepted the field-derived 21-byte stream and emitted 64 bytes of value 128.
- **Command:** `python3 -m unittest experimental/encoder/tests/test_m1_syntax.py`
- **Gate result:** 9 tests pass.
- **Remaining boundary:** implement the forward CDF/range writer and only the
  normative default-CDF subset reached by the 8×8 leaf; acceptance by both
  decoders, rather than reproduction of AVM's `8d80`, is the conformance gate.

## 2026-08-15 — M1 generated constant-128 encoder

- **Status:** PASS.
- **Milestone:** M1, exact 8×8 8-bit I400 constant-128 Closed Loop Key still.
- **Compiler:** GCC 15.2.0; freestanding C11, warnings as errors, disabled
  builtins/stack protector/PIC/PIE/unwind, static `-nostdlib` link.
- **Encoder command from repository root:**
  `experimental/encoder/build/av2-encoder-experiment
  --constant-128-8x8 experimental/encoder/build/m1-final.obu`.
- **Implementation:** named Sequence Header/frame fields, canonical derived
  Annex-B LEB128 lengths, local normative CDF materialization/adaptation, and
  forward range coding for every tile decision. Production embeds neither a
  fixture nor a complete golden stream.
- **Output:** 21 bytes, 2.625 bits/pixel, SHA-256
  `4c13e82fce9975d573b92b8ca2da25e1fed90308ac6857ec4d2d92dba0268b11`.
  Bitrate is undeclared because M1 has no presentation duration.
- **Pinned AVM:** revision
  `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`; executable SHA-256
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`.
- **Repository decoder:** executable SHA-256
  `c0c6329cc85de13c92d04a00deff8124c9b45dbe74da8a86ddc60d9d7b4de9df`.
- **Acceptance commands from repository root:**
  `build/reference/avm/avmdec --codec=av2 --rawvideo --limit=1
  --output-bit-depth=8 -o experimental/encoder/build/m1-final-avm.raw
  experimental/encoder/build/m1-final.obu`; and
  `build/host-cmake/av2heif-decoder
  experimental/encoder/build/m1-final.obu -o
  experimental/encoder/build/m1-final-project.i400`.
- **Decoded result:** both validators PASS; each emitted 64 bytes, all `0x80`,
  SHA-256
  `1df1b7ce1fd8fcbe20cde61646875e54fe38d8945ea7911afd59e025cc520a68`.
  Luma PSNR is infinite/exact; SSIM is not implemented.
- **Difference from the pinned AVM encode fixture:** AVM's retained 8×8
  lossless fixture is 19 bytes with SHA-256
  `3fd4a46a30554e31d4fd1da790782ac44008973a809a28d1b52549df191861bc`;
  this encoder emits a different valid 21-byte stream and ends its generated
  tile payload with `8d80`. Both streams decode to the same plane hash. Byte
  identity with AVM is not the gate because valid field choices and range
  termination need not be unique.
- **Determinism/errors:** repeated outputs are byte-identical; capacities 0
  through 20 report output exhaustion without partial success; invalid CLI
  mode creates no output.
- **Tests:** `make -C experimental/encoder test` passes 20 Python tests, three
  native test executables (bit writer, range writer, and M1
  field/CDF/exhaustion), one CLI smoke test, the dependency audit, and the ELF
  audit. Separately, the three independent-model tests pass while preserving
  the explicit blocker below.
- **ELF:** 15,392 bytes, SHA-256
  `b220e4ddb0773a7e86a5fccf9bcc7022786afad22b05ca4e9a7ca79a3ea8bc81`;
  static, no interpreter, dynamic segment, `DT_NEEDED`, or undefined symbol.
- **Timing:** 31 process runs: median 264,001 ns, worst 544,346 ns, median
  242,423 pixels/s; observed maximum RSS 392 KiB. Codec storage is fixed
  caller/stack memory with no allocation. A separate peak-workspace value is
  not instrumented yet; maximum RSS is a whole-process measurement.
- **Gate result:** PASS; M1 remains isolated below `experimental/encoder`.

## 2026-08-15 — Hash-locked M1/M3 CDF syntax subset

- **Status:** PASS.
- **Milestone:** M2 entropy syntax foundation for M1 and fixed-DC grayscale M3.
- **Normative source:** 14 official AV2 v1.0.0 CDF attachments plus
  `para_adjustment_list.h`, all SHA-256 locked by
  `tools/generate_cdf_subset.py`.
- **Generated data:** `src/normative_cdfs.inc`, 788 compact threshold rows.
  Generation verifies hashes, exact counts, threshold ordering/ranges,
  parameter indices, adaptation counts, and all 125 parameter adjustments.
- **C API:** `EncSyntaxCdfs` owns mutable per-encode rows. Checked context
  selectors cover do-split, delta-Q, luma mode set/index, FSC, palette
  presence, txb all-zero, 8x8 EOB, normal/LF base-at-EOB, DC sign, and intra
  transform selection. M1 now consumes these generated rows.
- **Tests:** selected normative row values, mutable isolation, selector bounds,
  fixed-DC range transcript `00c0`, stale-generation detection, and the
  unchanged M1 stream hash
  `4c13e82fce9975d573b92b8ca2da25e1fed90308ac6857ec4d2d92dba0268b11`.
- **Command:** `make -C experimental/encoder test`.
- **Gate result:** PASS; dependency and static ELF audits, C tests, smoke/input
  checks, and 22 Python tests pass.
- **Limitations:** this intentionally covers only 8x8 EOB/DC coefficient
  syntax. Other EOB sizes, non-EOB/AC base and remainder coding, chroma,
  palette colors, and non-DC transform paths remain blocked on later M3/M4
  syntax and reconstruction work.

## 2026-08-15 — Raw 8-bit I400 input boundary

- **Status:** PASS for the M3 input boundary; arbitrary-pixel encoding remains
  explicitly unsupported.
- **CLI:** `--i400-8 INPUT --width W --height H [--stride S] OUTPUT`.
- **Validation:** decimal parsing, 16384 dimension limits, stride validation,
  checked `stride * height`, a 256 MiB input cap, exact file-size matching, and
  read-only mapping ownership with deterministic release.
- **Output policy:** accepted input exits 5 before opening the output; invalid
  metadata exits 3, input I/O/size errors exit 4, and existing output remains
  unchanged. The M1 `--constant-128-8x8` path remains available.
- **Fixtures:** tests generate tiny raw planes under ignored `build/`.
  `tests/i400_prepared_reference.txt` contains metadata only for an
  `images/prepared/` photographic input; no licensed bytes were copied.
- **Tests:** no-libc build, dependency audit, static ELF audit, input error
  suite, and M1 smoke test pass.
- **Remaining boundary:** M3 may consume `EncI400Input` pixels only after it can
  encode every declared pixel and commit complete output.
- **Resolved model issue:** later correction to `EC_PROB_SHIFT=7` and explicit
  separation of official cumulative from inverse writer CDFs makes
  `tools/m1_entropy.py` decode `8d80` as `do_split=0`,
  `delta_q_abs=1`. Byte-independent Python validation now passes.
- **Capability boundary:** M1 accepts no image input and encodes only the fixed
  8×8 constant-128 mode. It does not encode arbitrary dimensions or pixels.
- **Next active milestones:** finish the independent-model correction and M1
  final review; continue M2 CDF/syntax transcript coverage; add bounded raw
  I400 input validation as groundwork only. M3 arbitrary prepared-image DC
  encoding remains pending until input pixels affect prediction, residuals,
  reconstruction, and emitted syntax.
- **Restart boundary:** rerun `make -C experimental/encoder test`, regenerate
  with the exact encoder command above, and require both decoded hashes to
  remain exact before extending M2 or starting the M3 encoding gate.

## 2026-08-15 — Bounded M3 DC grayscale encoder subset (superseded)

- **Status:** BLOCKED for the full M3 gate; PASS for arbitrary-pixel 8×8 I400.
- **Implemented:** input-derived 8×8 sequence dimensions, one legal 8×8 block,
  unavailable-neighbor DC prediction 128, deterministic SSE selection of
  qindex-200 levels `0, ±1..±4`, normative dequant/inverse-DC reconstruction,
  all-zero or EOB=1/base/DC-sign entropy syntax, and CLI `.obu` output.
- **Local reconstruction levels:** 30, 55, 79, 104, 128, 153, 177, 202, 226.
  The generic reconstructed top/left DC predictor is unit-tested, but the
  single-block stream has no available neighbor.
- **Bounds:** exactly 8×8 pixels; arbitrary stride at least 8 subject to the
  existing 256 MiB input cap; at most 32 output bytes; 64 caller-owned
  reconstruction bytes; fixed stack/CDF storage and no allocation.
- **Metrics:** the CLI deterministically reports bytes, luma SSE, luma PSNR to
  0.01 dB using integer fixed-point log2, and reconstruction workspace bytes.
  SSIM remains not implemented.
- **Generated flat:** 20 bytes, infinite PSNR, OBU SHA-256
  `7ac28a55187109f67095a00c5ea24e588c003b636100d9ca7d3b667a31e06be5`,
  reconstruction SHA-256
  `1df1b7ce1fd8fcbe20cde61646875e54fe38d8945ea7911afd59e025cc520a68`.
- **Generated ramp:** 22 bytes, SSE 21984, PSNR 22.77 dB, OBU SHA-256
  `1ee82c6c5450936d77d5b560d4cff6d2f50703c1d95d7b1dbad9e48748d2b2d2`,
  reconstruction SHA-256
  `999b533a76b2e3cb95952f6adffd271d46389dcfb91dc58e614531fe3615d478`.
- **Generated impulse:** 22 bytes, SSE 107325, PSNR 15.89 dB; its selected
  reconstruction and OBU equal the ramp case because DC-only coding retains
  only the block mean decision.
- **Prepared luma:** first 8×8 crop of the metadata-locked 896×598 source;
  crop SHA-256
  `edface5925ddaac5a7aece7daaa318c250decc04cc481cc59857be71e0fc8f7e`,
  20 bytes, SSE 9767, PSNR 26.29 dB, OBU SHA-256
  `34a0c0a487795afe6c8764ee55219da3ac32a9a8bb6bd6459d8a61bcb6ac7f1d`,
  reconstruction SHA-256
  `fc5ba0c1064081d6a87b52e95212798a9a1ac8bb51e57e85eec5fa2b1b3b89d0`.
- **Validators:** pinned `avmdec` SHA-256
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`
  and project decoder SHA-256
  `c0c6329cc85de13c92d04a00deff8124c9b45dbe74da8a86ddc60d9d7b4de9df`
  produced byte-identical output for every case.
- **Tests/audits:** `make -C experimental/encoder test` passes 45 Python tests,
  four native test executables, smoke/input-error tests, generated-CDF check,
  dependency isolation, and static ELF/no-undefined-symbol audits.
- **Binary:** 31,952 bytes, SHA-256
  `0e08bd4daf0452de4841fd6ac03be23977ce1384c887ced770b3cbee5d42304e`.
- **Superseded blocker:** the next entry adds a multi-transform fixed grid.

  Odd/broader dimensions and the locked Q sweep remain open. No fixture bytes
  or input-independent frame dimensions are embedded to conceal those limits.

## 2026-08-15 — Expanded M3 16×16 reconstructed-neighbor grid

- **Status:** BLOCKED for the full M3 gate; PASS for arbitrary-pixel 8×8 and
  16×16 I400.
- **Entropy-model correction:** the independent model uses
  `EC_PROB_SHIFT=7`, distinguishes official cumulative CDFs from writer-side
  inverse thresholds, and decodes `8d80` as `delta_q_abs=1`. The former model
  blocker is closed.
- **Grid:** 16×16 is one legal intra prediction block with TX mode select and
  a four-way 8×8 transform split. Transforms reconstruct in raster order;
  right/bottom transforms predict from locally reconstructed left/top edges.
  Sequence and frame dimensions, partition syntax, and contexts derive from
  the input configuration.
- **Coefficient range:** qindex 100, all useful signed DC levels, EOB=1,
  low-frequency base-at-EOB and BR symbols, literal quotient/Golomb remainder
  bits, DC-sign contexts, normative dequantization/inverse-DC scaling, and
  clipping. IBP and loop filtering are disabled so the exact local
  reconstruction is the displayed reconstruction.
- **8×8 flat:** 19 bytes, exact/infinite PSNR, OBU SHA-256
  `870bd45c5a6cb2251b404c63ab6f44d75286329f94f8f5195f839cf6f5daf48c`.
- **8×8 ramp:** 23 bytes, SSE 21856, PSNR 22.80 dB, OBU SHA-256
  `d31d693e42310988dda1231773cbffb3bdbebe022b43b9250f7d7ce8a11e041d`.
- **8×8 impulse:** 23 bytes, SSE 64009, PSNR 18.13 dB, OBU SHA-256
  `e4eb3bf7cd64cceb51eec6fdf38c0b71d1a72cb71875c632d6ef0903cc6f65a5`.
- **16×16 four-quadrant grid:** exact 128/200/50/150 reconstruction, 30
  bytes, infinite PSNR, input/reconstruction SHA-256
  `d2385fc319b51e5ae699c0403c94c84f2af4b13b2db1e58d8ed72f4d26098e92`,
  OBU SHA-256
  `78d52ff8190a8dd34c18df49c1e6995c49a18c1dda4752cce89cae115a57efa1`.
- **Prepared 16×16 luma crop:** 25 bytes, SSE 219, PSNR 48.81 dB, crop
  SHA-256
  `7431a9d64ac17cf99f5e96c9edb5e652bb787a92c1ebf8aea4f2d0b4e28ed823`,
  OBU SHA-256
  `fc8ded01a346428f1003242b81fdd7a3a14563e5c95bf2a6bfa36862ec64e025`,
  reconstruction SHA-256
  `4c7d29b0be67996dcf4a5276c0269b7140225a09e96b41850bbd7288c278c98b`.
- **Validation:** pinned `avmdec` and the project decoder exactly equal local
  reconstruction for every generated and prepared case. `make -C
  experimental/encoder test` passes 46 Python tests, four native executables,
  generated-CDF checks, deterministic/capacity/input checks, decoder gates,
  dependency isolation, and static no-libc ELF audits.
- **CDF subset:** 936 hash-locked rows, adding TX partition and LF BR tables.
- **Binary:** 38,136 bytes, SHA-256
  `2d0af0dfff3fa077d5168c802c82b62b940126fc225b23f3fd8179943e8bc744`.
- **Remaining blocker:** rectangular, odd, and larger dimensions and the
  locked Q sweep remain unimplemented, so the full M3 exit criteria are not
  claimed.

## 2026-08-15 — Rectangular M3 DC grid expansion

- **Status:** BLOCKED for the requested 32×32 target; PASS for 8×16 and 16×8.
- **Implemented:** the supported raw-I400 set is now 8×8, 8×16, 16×8, and
  16×16. Rectangles use the boundary-implied AV2 prediction partition and a
  legal horizontal/vertical transform partition into two 8×8 transforms.
  Reconstruction remains raster-ordered from reconstructed top/left edges.
- **Bounds/storage:** width and height must each be 8 or 16; stride and
  products are checked; output and reconstruction are caller-owned bounded
  spans. Production stack arrays have fixed compile-time bounds of four DC
  levels, 224 tile bytes, and 232 frame-payload bytes. No allocation, libc,
  compiler runtime, or unbounded stack storage was added.
- **Rectangular benchmark:** generated `ramp-x-16x8`, source SHA-256
  `01292f14b6a98e99bad04755108ec11182e59962003783effeefdc2a8d9c744f`,
  emits 27 bytes (1.6875 bpp), OBU SHA-256
  `34f1b120a9bd676e0f4ed2dea36c46c1ed111d04db724efc95bf4623b06e0977`.
  SSE is 194240 and PSNR is 16.320 dB. AVM and the project decoder emit the
  identical 128-byte reconstruction SHA-256
  `14a1a1e7fb7825c1c9ac9b3be102893bc850ac17c04012224522df6a82cc6a23`.
- **Validation:** `make -C experimental/encoder test` passes four native
  suites, shell smoke/input/atomic-output gates, 47 Python tests, generated
  CDF verification, dependency isolation, and static ELF/no-undefined-symbol
  audits. Rectangular tests cover padded strides, deterministic output,
  undersized output, local reconstruction, and both decoders.
- **Binary:** 38,336 bytes, SHA-256
  `b8afe416f47a658ba6c318d94922963c9e1e0d5d7693e53891c8c4a58cc82945`.
- **Blocker:** a 32×32 square-partition prototype was rejected by both
  validators before reconstruction. It was removed rather than retaining an
  unverified stream. Completing 32×32 requires a verified square-partition
  transcript and multi-prediction-block context model; no M4 AC model or
  rectangular-transform coefficient files were introduced.
- **Remaining constraints:** 32×32, dimensions beyond 16 per axis, odd sizes,
  and the locked Q sweep remain unsupported and exit 5 before output commit.

## 2026-08-15 — Bounded M4 DC plus vertical-AC C stream

- **Status:** PASS for the requested bounded milestone; broader M4 remains
  active.
- **Input:** 8×8 I400 vertical gradient with row values
  `128 128 128 129 129 129 130 130`, SHA-256
  `fa5e2dc3cd94a1e52711093de4db4bc9d7d1124d86c2c4389534521761edbf93`.
- **Mode:** `--experimental-m4-ac`; qindex 50, DC prediction 128, DCT_DCT,
  scan positions 0 and 1, selected levels DC `+3` and vertical AC `-2`.
- **Syntax:** normative adaptive CDFs for all-zero, EOB=2, DCT_DCT selection,
  LF base-at-EOB and non-EOB base, DC sign; equiprobable AC sign. Neither BR
  symbol is required for the selected magnitudes.
- **Output:** 21 bytes (2.625 bpp), SHA-256
  `9d84278811a478aaece5b87bf9340e47d1dfd20badc6af0765776840f8d98c15`.
  SSE is 0, luma PSNR is infinite, SSIM is not implemented.
- **Reconstruction:** local C, pinned `avmdec`, and the project decoder each
  emit the exact 64-byte input hash above.
- **Validators:** `avmdec` SHA-256
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`;
  project decoder SHA-256
  `c0c6329cc85de13c92d04a00deff8124c9b45dbe74da8a86ddc60d9d7b4de9df`.
- **Isolation:** production remains freestanding C11, caller-bounded, static,
  no-libc, and decoder-independent. The existing M1/M3 modes are unchanged.
- **Binary:** 61,760 bytes, SHA-256
  `80995f98b9d6dab78088d1d225488bb7e902f43afeb65496812f810f2661cf43`.
- **Gate:** focused C/model/decoder tests plus the complete encoder test,
  generated-CDF, dependency, static ELF, and no-undefined-symbol audits pass.

## 2026-08-15 — Final bounded M3/M4 validation and robustness record

- **Status:** PASS for the named bounded submilestones; full M3 and M4 remain
  active.
- **Supported M3 set:** arbitrary-pixel 8×8, 8×16, 16×8, and 16×16 I400 at
  qindex 100, with legal 8×8 transform grids and exact local/validator
  reconstruction.
- **257-case validation:** all 256 constant 8×8 sample values plus one
  four-quadrant 16×16 grid encoded twice identically and decoded identically by
  pinned `avmdec`, the project decoder, and the local reconstruction. Constant
  streams ranged from 19 to 23 bytes. Flat-128 was 19 bytes, SSE 0, infinite
  PSNR, OBU SHA-256
  `870bd45c5a6cb2251b404c63ab6f44d75286329f94f8f5195f839cf6f5daf48c`.
  The 16×16 grid was 30 bytes, SSE 0, infinite PSNR, OBU SHA-256
  `78d52ff8190a8dd34c18df49c1e6995c49a18c1dda4752cce89cae115a57efa1`.
- **Representative lossy results:** 8×8 ramp is 23 bytes at 22.80 dB;
  prepared 16×16 luma is 25 bytes at 48.81 dB; generated 16×8 horizontal ramp
  is 27 bytes at 16.320 dB. Both validators match local reconstruction.
- **Robustness:** M3 checks stride multiplication and the final active input
  offset before reading; output uses an exclusive sibling temporary and atomic
  rename with cleanup on failure; the full test gate requires executable AVM
  and project validators and permits no silent decoder skips.
- **First M4 AC stream command:**
  `experimental/encoder/build/av2-encoder-experiment
  --experimental-m4-ac input.i400 output.obu`.
  It emits 21 bytes, SHA-256
  `9d84278811a478aaece5b87bf9340e47d1dfd20badc6af0765776840f8d98c15`,
  and local C/AVM/project all reconstruct the exact 64-byte source hash
  `fa5e2dc3cd94a1e52711093de4db4bc9d7d1124d86c2c4389534521761edbf93`
  (SSE 0, infinite PSNR).
- **32×32 diagnosis/status:** the rejected generalized leaf emitted
  `tx_do_partition` without `tx_partition_type` and then treated the result as
  sixteen 8×8 transforms, although a partitioned 32×32 prediction block yields
  four 16×16 transforms. The simpler legal recipe splits the 32×32 prediction
  block into four 16×16 prediction blocks and reuses four 8×8 transforms in
  each. A production attempt is present in the shared tree, but its current
  23-byte flat-128 stream is rejected by pinned `avmdec` at tile data despite
  reporting exact local reconstruction. It remains unsupported and blocked on
  focused dual-decoder parity.
- **Still open:** odd and broader dimensions, the multi-Q sweep, broad AC
  positions/magnitudes/transforms, chroma, partition/intra search, filters and
  restoration, and quality/rate control. These bounded PASS results do not
  complete full M3 or M4.

## 2026-08-15 — M2 rigorous validator division

- **Status:** PASS.
- **Milestone:** M2 entropy-writer exit gate.
- **Transcript gate:** the production C writer emits four compact syntax
  transcripts and 75 exhaustive adaptive transcripts covering every alphabet
  size 2 through 16 at initial counts 0, 15, 16, 31, and 32. Every exhaustive
  case writes all symbols ascending and descending.
- **Independent models:** `tests/range_writer_test.c` and
  `tools/m2_transcript_matrix.py` independently decode that exhaustive family,
  reproduce all symbols and final CDF states, and validate legal range exits.
  The Python matrix additionally freezes bytes, SHA-256, post-operation tells,
  exact-capacity success, and one-byte-short exhaustion. Fixture SHA-256:
  `255555cbbf2c1503cc65a1be68fc5f811d9707e558bc2d048f9d2e09b480a7f0`.
- **Technical boundary:** arbitrary range transcripts are not AV2 image
  streams and are not passed to image decoders.
- **Full-stream gate:** six generated complete M1/M3/M4 streams regenerate
  byte-for-byte, then decode to byte-identical planes in pinned `avmdec`
  SHA-256
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`
  and the project decoder SHA-256
  `c0c6329cc85de13c92d04a00deff8124c9b45dbe74da8a86ddc60d9d7b4de9df`.
  Full-stream manifest SHA-256:
  `f332e305823f67e52426576410d07df73e6177e8894ec90e40fb9d83d878b0bd`.
- **Command:** `make -C experimental/encoder test`.
- **Result:** PASS: exhaustive native range model, five transcript/syntax
  Python tests, M1/M3/M4 native tests, 59 integration/model tests including
  dual-decoder stream parity, generated-CDF check, input/atomic-output checks,
  dependency audit, and static ELF audit.
- **Gate result:** M2 PASS under the explicit transcript-model versus complete
  image-stream validator division; later codec breadth remains M3/M4 work.

## 2026-08-15 — M2 final reconciled completion gate

- **Status:** PASS; M2 is complete. M3 and M4 remain active bounded
  submilestones and were not extended by this work.
- **Source boundary:** repository revision
  `f10aefa0552e3cf249edc5a92128fb16dc80b90e` plus the recorded dirty M2
  changes below.
- **Normative source:** AV2 Bitstream & Decoding Process Specification v1.0.0,
  28 May 2026, <https://av2.aomedia.org/v1.0.0/index.html>, especially
  §§4.11.3, 4.11.8, 5.20, 8.2, and 8.3. Normative attachments used by the
  hash-locked CDF generator include `prob_inc.h`,
  `para_adjustment_list.h`, and the CDF attachments enumerated by
  `tools/generate_cdf_subset.py`.
- **Compiler/build:** GCC 15.2.0; production objects use freestanding C11,
  warnings as errors, disabled builtins/stack protector/PIC/PIE/unwind, and a
  static `-nostdlib` link. Final executable: 66,544 bytes, SHA-256
  `f6856b21dc084acfe1cdb6adb784405f9bb2ea2214bdaff71c1355aa1d05f64d`;
  ELF and dependency audits pass with no interpreter, dynamic dependency,
  undefined symbol, decoder include, or decoder build dependency.
- **Range/CDF exit mapping:** all supported alphabet sizes 2 through 16 and
  every symbol pass independent C and Python decoding. The frozen matrix has
  75 adaptive cases (15 alphabets × counts 0/15/16/31/32), ascending and
  descending symbols, exact tell changes, final CDF equality, and legal exits.
  Native tests additionally cover all 125 parameter combinations, adaptation
  through count saturation, literals 0 through 32 bits, deterministic empty
  and nonempty finish, long pending-`0xff` and carry paths, and a
  6,000-operation mixed stress transcript at five split positions.
- **Syntax-helper exit mapping:** raw UVLC/SVLC, two's-complement signed
  literals, `ns`, subexponential coding, signed/unsigned reference
  recentering, zero alignment, trailing bits, arithmetic signed literals and
  `ns`, and general coefficient base/BR/remainder coding round-trip through an
  independent Python reader. Invalid domains, sticky failures, overflow
  boundaries, and writes beyond declared capacities are checked.
- **Capacity record:** compact transcript outputs are 1/3/1/3 bytes and pass
  both exact capacity and one-byte-short exhaustion. The 75 exhaustive
  transcripts range from 1 to 18 bytes under the same exact/short check.
  Stress encoding passes with an 8,192-byte roomy buffer, its exact size, and
  rejects exact-minus-one, one-byte, and zero-byte capacities. The exhaustive
  helper vectors occupy 71,644 raw bytes and 481 range-coded bytes; compact
  boundary encodes test every insufficient capacity while preserving bytes
  outside the caller span.
- **Transcript manifest:** four named compact transcripts plus 75 exhaustive
  transcripts are frozen in `fixtures/m2-transcript-matrix.json`, SHA-256
  `255555cbbf2c1503cc65a1be68fc5f811d9707e558bc2d048f9d2e09b480a7f0`.
  Generator/model identities: `tools/m2_transcript_matrix.py`
  `c1f31ae200dfaee8804b2713a8bebaea97daf6b8800a3c059374c5930e22a920`
  and `tools/syntax_reference.py`
  `8614c4c79b37fcd3af40623d691c338f81350d1f4c3a64ea495f1dfdf48447c0`.
- **Complete-stream manifest:** six generated streams cover M1
  coefficient-free, M3 DC base/BR/remainder and multi-block adaptation, and M4
  AC/sign paths. Their OBU sizes are 21/20/21/21/27/21 bytes.
  `fixtures/m2-full-streams/manifest.json` SHA-256 is
  `f332e305823f67e52426576410d07df73e6177e8894ec90e40fb9d83d878b0bd`;
  `tools/m2_full_stream_matrix.py` SHA-256 is
  `e078353fced672e1aceaf2d3167ca1d6c7251f3c98cc544efac056f062155782`.
- **Validator identities:** pinned AVM v1.0.0 revision
  `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`, `avmdec` SHA-256
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`;
  project decoder SHA-256
  `c0c6329cc85de13c92d04a00deff8124c9b45dbe74da8a86ddc60d9d7b4de9df`.
  Both independently decode all six complete streams to byte-identical planes
  matching the manifest. Arbitrary range transcripts are correctly validated
  by the independent transcript models, not misrepresented as image streams.
- **Commands/results:** the integrator ran
  `make -C experimental/encoder clean test-unit` and
  `make -C experimental/encoder check-generated`: PASS, with five M2
  transcript/syntax Python tests, 59 broader Python tests with 14 explicit
  validator skips, five native executables, three shell gates, generated-data,
  dependency, and ELF checks. A separate task agent then ran
  `make -C experimental/encoder clean test`,
  `make -C experimental/encoder check-generated`, and `git diff --check`:
  PASS with 64 Python tests, zero skips, all five native executables, all three
  shell gates, both validators, generated checks, and audits.
- **Post-M2 limitations:** the entropy foundation does not add chroma, general
  AC scans or transforms, broader/odd frame and partition syntax, partition or
  intra search, filters, restoration, quality selection, or rate control.
  The supported image encoder remains the previously documented bounded
  M1/M3/M4 surface; full M3 and M4 exit criteria remain open.
- **Gate decision:** all M2 plan requirements and exit criteria are satisfied;
  `plan.md` is marked `[x]`. Restart from
  `make -C experimental/encoder clean test` before any later entropy change.

## 2026-08-15 — M3 complete 1..16 dimension grid

- **Status:** PASS for the requested grid expansion; 32×32 remains excluded.
- **Dimensions:** every width and height pair in 1..16 is accepted, including
  odd rectangles. AV2 partition boundaries use dimensions rounded to the
  four-pixel grid and select legal 8×8, 8×16, 16×8, or 16×16 prediction
  blocks with one, two, or four square 8×8 transforms.
- **Edges/crop:** source samples outside the active plane replicate the final
  active row or column for transform-level selection. Reconstruction uses a
  bounded 16×16 padded workspace, then copies only `width * height` active
  pixels to the caller. Reported SSE likewise covers only active pixels.
- **Ordering/contexts:** transform records and coefficient neighbors use
  ceil-divided 8×8 grid coordinates in raster order; the previous floor-width
  indexing hazard for widths below or just above eight is covered.
- **Bounds:** dimensions, stride, final active input offset, output capacity,
  reconstruction capacity, level count, and syntax spans are checked. The
  maximum remains four transform records and 256 active/padded pixels; no
  32×32 allocation or syntax path was enabled.
- **Matrix validation:** `test_all_dimensions_one_through_sixteen` runs an
  odd-sensitive generated pattern for all 256 dimension pairs through the
  encoder, local reconstruction, pinned `avmdec`, and the project decoder.
  Every decoded active plane is byte-identical. Padded-stride input is used
  throughout the matrix.
- **Focused command:** `make -C experimental/encoder
  build/m3-test build/av2-encoder-experiment && experimental/encoder/build/m3-test
  && AVMDEC=$PWD/build/reference/avm/avmdec
  PROJECT_DECODER=$PWD/build/host-cmake/av2heif-decoder python3 -m unittest
  experimental/encoder/tests/test_m3_encoder.py`.
- **Focused result:** PASS, eight Python tests in 13.707 seconds plus the
  native M3 executable. Test work directories include the process ID so
  concurrent validation agents cannot delete each other's decoder outputs.
- **Full gate:** `make -C experimental/encoder test` PASS: generated-data,
  dependency and static-ELF audits; five native executables; three shell
  gates; five entropy/syntax Python tests; and 65 integration/model tests with
  both decoders. The prepared 30-case multi-Q benchmark also reports exact
  parity and aggregate monotonic SSE.

## 2026-08-15 — Prepared-image bounded M3 gate

- **Status:** PASS.
- **Scope:** benchmark and evidence only; no prepared image bytes are tracked.
  `metrics/fixtures.json` locks seven offset crops from metadata-locked square
  Pitti, landscape Baya, and portrait Elbrus I400 sources. Dimensions are 1×1,
  15×15, 16×16, 8×7, 16×8, 7×8, and 8×16, covering odd/even edges and the
  complete 16×16 bound.
- **Reproduction:** `tools/m3_benchmark.py` verifies each preparation manifest,
  source dimensions/hash, crop offset/bounds, and crop hash before extracting
  packed bytes only under ignored `experimental/encoder/build/`.
- **Q sweep:** every crop passed at qindex 50, 100, 150, and 200: 28/28
  encodes, 28/28 byte-identical pinned-AVM/project decodes, and 28/28 matches
  between encoder-local bytes/SSE/workspace and decoded evidence.
- **Aggregate evidence (850 pixels per Q):** q50: 177 bytes, 1.665882 bpp,
  SSE 468727, PSNR 20.716 dB; q100: 169 bytes, 1.590588 bpp, SSE 468869,
  PSNR 20.714 dB; q150: 160 bytes, 1.505882 bpp, SSE 469615, PSNR 20.708 dB;
  q200: 146 bytes, 1.374118 bpp, SSE 516655, PSNR 20.293 dB.
- **Timing evidence:** encoder median/worst by Q was 979,878/1,723,622 ns,
  877,211/1,043,992 ns, 790,746/987,491 ns, and 730,334/747,445 ns.
  Median pinned-AVM decode was 36.285–38.212 ms; median project decode was
  1.376–1.513 ms. Per-case command and timing records are in JSONL.
- **Tool identities:** encoder SHA-256
  `dc46f1e0a1332e5a5881214b0fcb11ebb4ac86989710bb57a6f34140bb01495b`;
  pinned `avmdec`
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`;
  project decoder
  `c0c6329cc85de13c92d04a00deff8124c9b45dbe74da8a86ddc60d9d7b4de9df`.
- **Commands:** `python3 experimental/encoder/tools/m3_benchmark.py --output
  experimental/encoder/results/m3-prepared-gate.jsonl --work-dir
  experimental/encoder/build/m3-prepared-gate`; and `python3 -m unittest
  experimental/encoder/tests/test_m3_benchmark.py`.
- **Mandatory tests:** seven tests pass, including locked fixture coverage,
  reproducible extraction, and all four-Q local/AVM/project parity for all
  seven prepared crops. The complete `make -C experimental/encoder test` gate
  also passes 65 Python tests plus all native, shell, generated-data,
  dependency, static-ELF, and dual-decoder checks.
- **Out-of-bound evidence:** generated 17×15 remains clearly recorded as
  `unsupported`; it is not one of the chosen 1..16 prepared gate cases.
- **Gate decision:** PASS; all chosen bounded prepared cases encode.

## 2026-08-15 — Configurable M3 qindex and locked monotonic sweep

- **Status:** PASS for `encoder-m3-q-sweep`.
- **CLI/API:** `--qindex Q` accepts the documented non-lossless range 1..255.
  The default remains 100 and produces the same stream as explicit qindex 100.
  Backward-compatible C entry points wrap the new q-aware encoder, frame-header,
  and tile APIs.
- **Derived behavior:** the frame `base_q_idx`, fixed-zero delta-Q behavior,
  dequant lookup, useful signed-level search bound, and coefficient CDF group
  all derive from the selected qindex. Sequence syntax remains invariant.
  Qindex 0 is rejected because it requires lossless transform semantics.
- **Locked direction:** `metrics/m3-q-sweep.json` orders
  `124,100,76,52,28,4`; decreasing qindex means increasing quality, therefore
  aggregate SSE must be non-increasing.
- **Evidence:** 30/30 generated/prepared-crop cases have byte-exact
  local-model, pinned AVM, and project reconstruction parity. Aggregate SSE is
  `953467, 953055, 952871, 952871, 952871, 952871`; aggregate OBU bytes are
  `126, 135, 136, 140, 144, 149`.
- **Artifacts:** locked inputs are in `metrics/m3-q-sweep.json`; sorted JSON
  evidence is in `metrics/m3-q-sweep-results.json`.
- **Command:** `python3 experimental/encoder/tools/m3_q_sweep.py`.

## 2026-08-15 — Exact M3 odd-edge syntax and reconstruction model

- **Status:** PASS for the requested 1..16 odd-edge derivation and probes.
- **Normative geometry:** `MiCols/MiRows = 2*((FrameWidth/Height+7)>>3)`,
  hence the coded canvas rounds each axis to 8 or 16 while decoder output
  remains exactly the active width×height.
- **Forced path:** the top-left path is implied, with no symbols:
  64×64 HORZ, 64×32 VERT, 32×32 HORZ, 32×16 VERT. At 16×16 the boundary
  further implies HORZ+VERT for 8×8, VERT for 8×16, HORZ for 16×8, or
  nothing for 16×16. Only the resulting block's `PARTITION_NONE` decision is
  coded.
- **Forward recipe:** extend the active source by repeating the final active
  row/column to the coded canvas; use full 8×8 residual inputs and 8×8
  transforms. Out-of-display coded samples are reconstructed samples, not
  unavailable neighbors. Prediction uses reconstructed raster neighbors,
  with divisors 8 for one edge and 16 for both, never the active crop count.
- **Contexts/crop:** a lone 8×8 transform uses all-zero context 0; transforms
  in larger prediction blocks use `(Min(4,|topDC|)+Min(4,|leftDC|)+3)>>1`,
  so the first split transform uses context 1. DC-sign context uses the
  signed neighboring DC categories. Reconstruction fills the coded canvas,
  then copies only the active rectangle.
- **AVM probes:** pinned `avmenc` accepted 1×1, 7×9, 9×7, 15×15, 1×16, and
  16×1 block-flat QP-100 probes. Pinned `avmdec` returned exact visible
  samples and exactly 1/63/63/225/16/16 bytes respectively. The project
  decoder was exercised too and rejected these broader AVM-encoder tile
  streams; its separate bounded experimental-stream parity remains covered
  by the M3 gates.
- **Artifacts:** `spec-notes/m3-odd-edge-model.md`,
  `tools/m3_odd_model.py`, `tests/test_m3_odd_model.py`,
  `tests/m3_odd_model_vectors.json`, and
  `tests/m3_odd_avm_probes.json`.
- **Validation:** five focused model tests pass; AVM probe generation and
  decode pass for all six representative dimensions.

## 2026-08-15 — Bounded M3 1..16 orchestration completion

- **Status:** PASS for todo `encoder-m3-dc-gray`; full unbounded M3 remains
  active.
- **Scope:** every 8-bit I400 width/height pair from 1 through 16, explicit
  packed or padded stride, qindex 1 through 255, legal edge-extended 8×8
  transform coverage, and exact active-pixel reconstruction.
- **Ownership/bounds:** the C API accepts caller-owned stream and compact
  reconstruction spans. It stages the complete stream and reconstruction
  locally, checks `width*height`, final input offset, stream capacity, and
  reconstruction capacity, and leaves both caller outputs unchanged on
  exhaustion. The CLI maps exact input bytes and atomically replaces only a
  complete `.obu`.
- **Exactness:** the odd-sensitive exhaustive 256-dimension matrix and
  generated flat/ramp/impulse cases match the independent local model, pinned
  `avmdec`, and the project decoder byte-for-byte. Prepared 1×1, 15×15,
  16×16, 8×7, 16×8, 7×8, and 8×16 photographic crops pass at qindex 50, 100,
  150, and 200. No dimension inside 1..16 is blocked.
- **Quality/determinism:** the locked 30-case q sweep has exact three-way
  reconstruction parity and aggregate SSE
  `953467,953055,952871,952871,952871,952871` for qindex
  `124,100,76,52,28,4`; repeated representative encodes and metrics are
  identical.
- **Metrics:** active-pixel SSE and fixed-point PSNR, `.obu` bytes,
  reconstruction/workspace bytes, and qindex are reported. SSIM remains not
  implemented.
- **Failure tests:** short stream and reconstruction capacities preserve
  sentinel bytes and zero the result; invalid dimensions, stride overflow,
  qindex 0/256, short/excess input, output I/O failure, and temporary-name
  exhaustion fail without partial success.
- **Validation command:** `make -C experimental/encoder clean test`.
  Result: PASS, including six native/model executables, shell input/atomic
  checks, 71 Python tests, dependency audit, and static no-libc ELF audit.
- **Binary:** 66,968 bytes; SHA-256
  `7896d785fab7366bac12133074971b4e2047a58a5acf379f9540f6548ff237a2`;
  no interpreter, dynamic dependencies, or undefined symbols.
- **Locked q evidence:** `metrics/m3-q-sweep-results.json`, 22,274 bytes,
  SHA-256
  `74ca7a27b71e382615eab63ea6b3d5acdbaee29323a6c65281fefb2ae8a8be5b`.
- **Remaining boundary:** dimensions above 16, broader partition/mode search,
  filters, and rate control remain outside this bounded task.

## 2026-08-15 — Final bounded M3 gate review

- **Status:** PASS after independent review and clean mandatory validation.
- **Review fix:** `tools/m3_benchmark.py` previously classified rejection of a
  supported generated stream by either validator as nonfatal `unsupported`.
  Such rejection now records `error`, preserves the validator return code, and
  forces a failing process status. A `/bin/false` regression test locks this
  behavior.
- **Validation:** the mandatory gate passes with zero skips or failures:
  generated-data checks, five compiled unit binaries, 76 Python tests, smoke,
  input-error and atomic-output integration checks, the exhaustive 256-pair
  dimension matrix, 28 prepared crop/qindex cases, and the locked 30-case
  aggregate quality sweep.
- **Audits:** dependency and ELF audits pass; the encoder remains statically
  linked with no interpreter, dynamic dependencies, or undefined symbols.
- **Milestone boundary:** this closes the reviewed 1..16 M3 submilestone only.
  Full M3 remains active for dimensions above 16, broader partition and mode
  search, filtering, and rate control.

## 2026-08-15 — M3 scalable storage boundary

- **Status:** PASS for todo `encoder-m3-scalable-storage`.
- **Core API:** `enc_m3_query_workspace` computes padded dimensions, active and
  padded pixels, transform-block count, alignment, and total bytes with checked
  add/multiply/round-up operations. The workspace encoder stages padded and
  compact reconstruction, signed levels, and the complete stream there.
- **Ownership:** the codec core remains freestanding and allocation-free. The
  Linux runtime provides anonymous read/write page mappings and deterministic
  unmapping for CLI workspace, reconstruction, and stream storage.
- **Compatibility:** the legacy 1..16 APIs wrap the workspace entry point and
  preserve existing qindex-100 bitstreams. Dimensions above 16 now reach
  scalable geometry accounting but return `ENC_UNSUPPORTED`; no partition
  syntax was invented.
- **Failure contract:** short/misaligned workspace, reconstruction exhaustion,
  stream exhaustion, stride/geometry overflow, and unsupported syntax leave
  stream and reconstruction outputs unchanged. Atomic file replacement remains
  the only output commit.
- **Coverage:** native tests cover 16384×16384 sizing, odd padded geometry,
  overflow, padded strides, every short span, mmap success/failure/reset, and
  legacy compatibility. Dependency auditing rejects allocation calls in core
  sources and fixed M3 geometry arrays.
- **Validation:** `make -C experimental/encoder test` passes generated checks,
  native tests, input/atomic shell gates, dependency/static-ELF audits, both
  pinned decoders, and all 80 Python tests. Existing 1..16 streams and
  reconstructions remain exact.

## 2026-08-15 — Full prepared-image corpus gate preparation

- **Status:** EXPECTED BLOCKED; no complete prepared image is claimed encoded.
- **Inventory:** the generated M3 corpus manifest covers all 28 locked
  preparations for 14 sources and all four prepared outputs (`master`,
  `gray8`, `yuv420p8`, `yuv444p8`). It records source/preparation manifest
  hashes, dimensions, packed I400 stride and bytes, and every output hash.
- **Selection:** minimum pixel area then preparation ID selects exactly one
  full image per source; this is the 14-image `le896` set. All 28 local I400
  planes were present and lock-verifiable during this work.
- **Gate behavior:** each selected plane is hash-checked and encoded twice.
  Two clean exit-5 rejections without output are `expected_blocked`.
  Inconsistent reruns, unexpected encoder failures, missing inputs, or any
  validator rejection after encoder success fail the gate. Successful future
  records require repeated OBU hashes plus local/AVM/project reconstruction
  hashes, SSE/PSNR, and command/timing evidence.
- **Current result:** 14/14 selected full images are expected-blocked because
  each axis is outside the current 1..16 M3 limit; 0 are passes and 0 reached
  validators. This records unsupported dimensions honestly and does not close
  full M3.
- **Commands:** `python3 experimental/encoder/tools/m3_prepared_corpus.py
  --inventory-only`; `python3 experimental/encoder/tools/m3_prepared_corpus.py`;
  and `python3 -m unittest
  experimental/encoder/tests/test_m3_prepared_corpus.py`.

## 2026-08-15 — Legal 17..64 partition traversal

- **Status:** PASS for grammar/model/probes; production M3 remains bounded.
- **Correction:** AV2 v1.0 square split is eligible only at 128x128 and
  256x256. The rejected coded `BLOCK_32X32` square split was invalid. The legal
  four-leaf path is 32x32 HORZ, then each 32x16 VERT.
- **Model:** `tools/m3_large_partition_model.py` implements 64x64-superblock
  depth-first traversal, boundary implications, rectangular grammar,
  partition contexts, exact active coverage, and reconstruction dependencies.
- **Local evidence:** all 2,304 width/height pairs 17..64 have exact MI-grid
  coverage, no square-split symbol, legal 8x8/8x16/16x8/16x16 leaves, and
  previously reconstructed external top/left edges.
- **Validator probes:** forced min/max partition size 16, extended/uneven
  partitions disabled, flat-128 I400. All 36 rounded dimension classes passed
  pinned AVM and the project decoder with identical exact visible planes.
- **Pins:** AVM revision
  `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`; `avmenc`
  `830c5f7a82e071560501183fb85ea9e2632ce7665c9033232ac64b546efb7e64`;
  `avmdec`
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`;
  project decoder
  `c0c6329cc85de13c92d04a00deff8124c9b45dbe74da8a86ddc60d9d7b4de9df`.
- **Representative streams:** 32x32
  `2eb6389d684515545c0e89aae56f76902b562537ec95c29b791846e62c9f08dd`;
  64x64
  `822275bad706991e50c950e511b0a36dfe5a9f34cac3749e20205177f80b414c`.
- **Raster conclusion:** global leaf order is not always raster, but one raster
  reconstruction surface is safe because external top/left dependencies
  precede each leaf. Emit syntax depth first and transforms leaf-locally.
- **Production decision:** no syntax/storage changes were made. The encoder
  remains exactly 1..16, avoiding conflicts in `src/m3.c` and storage. Next:
  materialize proved partition CDF rows, validate a project-generated non-flat
  17..64 stream, then extend across superblocks for full prepared images.
- **Commands:** `python3 -m unittest
  experimental/encoder/tests/test_m3_large_partition_model.py`; and
  `python3 experimental/encoder/tools/m3_large_partition_model.py
  --probe-all-classes`.
## 2026-08-15 — full-image integration boundary

Attempted to connect the verified 17..64 rectangle traversal to production
entropy coding. A 32x32 prototype reached the intended
64x64→64x32→32x32→32x16→16x16 depth-first tree in the project decoder, but
non-flat reconstruction diverged from encoder-local SSE. Tightening
coefficient contexts then desynchronized at the second transform. No >16
path is exposed: the exact next boundary is multi-leaf adaptive
transform/coefficient traversal at 17 pixels on either axis. Beyond 64 also
requires independently verified multi-superblock row resets. The 14 selected
prepared images begin at 896x424, so all remain explicit unsupported cases.

## 2026-08-15 — M3 multi-leaf CDF state

- **Status:** PASS for model/transcript; production integration unchanged.
- **Milestone:** exact transform/coefficient entropy state for 17..64 I400.
- **Result:** complete before/after inverse-CDF transcripts are frozen for
  17x17, 17x32, 32x17, 32x32, 48x64, and 64x64 non-flat DC-only models, plus
  flat-128 and all four base-q context controls.
- **Key finding:** adaptive CDFs persist for the tile across transforms,
  prediction leaves, and superblock positions. Coefficient neighbor state is
  committed after every transform. The 32x32 second transform changes skip
  context 1→3 and sign context 0→1 after the first level -46.
- **Pinned AVM:** revision
  `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`; all six sources encode/decode.
  Exact hashes are in `tests/m3_multileaf_cdf_avm_probes.json`.
- **Project decoder blocker:** AVM non-flat probes currently fail at the first
  coefficient with `integer overflow (cause 7, stage 2, tell 25)`; its source
  still independently establishes state lifetime and context update order.
- **Validation:** focused 31-test M3 model suite, `make -C
  experimental/encoder test-unit`, and full `make -C experimental/encoder
  test` all pass.
- **Restart boundary:** integrate the frozen traversal/context/CDF sequence
  into production only in a separate change; do not alter the current 1..16
  support bound here.

## 2026-08-15 — M3 multi-leaf non-entropy reconstruction

- **Status:** PASS; production `src/m3.c` and entropy integration unchanged.
- **Order proof:** rectangular leaves are visited depth first and each 8x8
  transform is visited in leaf-local raster order. Prediction is per
  transform from already reconstructed top/left samples.
- **32x32 root cause:** the old local path selected levels in global raster
  order while the decoder consumed them in rectangular leaf order. The first
  mismatch is transform index 2. Frozen non-flat SSE is `5376226` in correct
  order versus `5609644` when the legacy level sequence is replayed by the
  decoder.
- **Edges/crop:** source edges repeat the final active row/column to the
  eight-pixel coded canvas. A full local canvas and decoder-visible-only
  commits produce identical active crops for ten odd, rectangular, and square
  representatives through inverse DC reconstruction and clipping.
- **Contexts:** all-zero and DC-sign contexts are derived from decoder-style
  MI state and committed after every transform.
- **Evidence:** `tests/m3_multileaf_reconstruction_vectors.json`,
  `tests/m3_multileaf_avm_probes.json`, and
  `tools/m3_large_partition_model.py`. Pinned AVM accepts all ten non-flat
  probes; the project decoder's frozen nonzero inverse-transform
  `integer overflow` remains an implementation limitation.
- **Validation:** 34 focused M3 tests, `make -C experimental/encoder
  test-unit`, full `make -C experimental/encoder test`, and
  `git diff --check` pass.

## 2026-08-15 — M3 production integration remains blocked

- Integrated the frozen depth-first leaf order, adaptive transform/coefficient
  CDF lifetime, coefficient MI contexts, scalable staging, edge extension,
  crop, and qindex range into a private production prototype.
- Exact 32x32 alternating-DC probes passed pinned AVM and the project decoder
  at qindex 1, 90, 91, 100, 141, 191, and 255. The qindex-100 output also
  matched the frozen local reconstruction hash.
- The same prototype was not validator-complete: pinned AVM rejected all eight
  17x17 general-pattern q probes (1, 2, 10, 50, 90, 100, 191, 255), and rejected
  non-flat rounded classes 24 and 56 while accepting 32, 40, 48, and 64.
  The project decoder accepted those generated streams, so its known overflow
  on AVM-generated non-flat probes is not the blocker observed here.
- The public maximum remains 16 and existing streams remain on the legacy
  sequence/tool path. Hash-locked rectangle and extension-partition CDF
  defaults and the private traversal code are retained only to make the next
  transcript investigation reproducible.
- Follow-on work must freeze the exact range/CDF state at the first 24x24 and
  56x56 divergence, then verify edge 8x16/16x8/8x8 coefficient contexts before
  another production-support attempt.

## 2026-08-15 — M3 boundary partition transcript resolved

- **Root cause:** private large-partition syntax measured geometry in MI units
  but mapped a 2x2-MI (8x8) `NONE` leaf to split adjustment 1/context 4.
  AV2 and pinned AVM require adjustment 0/context 0.
- **Exact divergence:** 24x24 event 34 at `(16,16)`, tell 21, range 43952
  became 35320 instead of 37712. 56x56 event 181 at `(48,48)`, tell 78,
  range 54888 became 49864 instead of 47080. 40x40 also selected the wrong
  CDF row, but probability quantization produced the same range and bytes.
- **Ruled out:** rectangle symbols, extension enablement/signaling,
  boundary-implied decisions, extension contexts, and all observed
  partition CDF defaults/updates matched pinned AVM.
- **Legacy rejection:** flat 24 overflowed at tell 158; non-flat 24 rejected
  padding at tell 204. Flat 56 rejected its marker at tell 83; non-flat 56
  rejected padding at tell 830.
- **Correction:** the private helper now maps 8x8 to split adjustment 0.
  Public support remains 1..16, whose legacy syntax path is unchanged.
- **Parity:** pinned AVM and the project decoder accept and exactly agree on
  all flat/non-flat 24/32/40/48/56/64 probes.
- **Evidence:** `spec-notes/m3-boundary-partition-transcript.md`,
  `tools/m3_boundary_partition_transcript.py`, three locked JSON reports, and
  `test_m3_boundary_partition_transcript.py`.
- **Validation:** focused tests, `make test-unit`, full `make test`, all 32
  project CTest regressions, and generated-CDF/dependency/ELF audits pass.

## 2026-08-15 — M3 17x17 edge coefficient context parity

- **Status:** complete after rerunning the frozen matrix against the corrected
  partition range state.
- **Coverage:** 14 cases: flat/non-flat 17x17 at qindex 1, 90, 91, 100, 141,
  191, and 255; 3 edge leaves and 5 edge transforms per case.
- **Derivation:** exact adaptive skip/eob/base/BR/sign/remainder rows, clipped
  two-by-two MI spatial contexts, immediate `cul_level` commits, predictions,
  and reconstruction updates are locked in
  `tests/m3_edge_coeff_context_vectors.json`.
- **Validator result:** 14/14 local == pinned AVM == project. The pinned
  `avmdec` hash is
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`.
- **Root-cause separation:** all 11 former mismatches were downstream of the
  terminal 8x8 `PARTITION_NONE` split-context error (context 4 instead of 0).
  With the corrected range state, no edge coefficient-context error remains.
- **Decision:** public maximum remains 16 and production syntax is unchanged,
  preserving the exact 1..16 matrix.
- **Evidence:** `spec-notes/m3-edge-coeff-context-parity.md`,
  `tools/m3_edge_coeff_model.py`,
  `tests/m3_edge_coeff_context_vectors.json`, and
  `tests/m3_edge_coeff_avm_probes.json`.
- **Validation:** focused edge/boundary tests, `make test-unit`, full
  `make test`, and all 32 project CTest regressions pass.

## 2026-08-15 — M3 17..64 production promotion

- **Status:** PASS. The corrected rectangle-only one-superblock path is public
  for dimensions whose axes are both 17..64; 1..16 remains on the unchanged
  legacy branch and mixed ranges remain unsupported.
- **Production behavior:** terminal 8x8 partition split context 0, depth-first
  rectangle traversal, leaf-local transform order, tile-wide adaptive CDF
  lifetime, per-transform coefficient context commits, edge extension, compact
  crop, and exact local reconstruction are integrated.
- **Exhaustive parity:** all 2,304 width/height pairs 17..64 encoded the locked
  odd-sensitive non-flat pattern at qindex 100. All 2,304 local
  reconstructions equal pinned AVM and the project decoder (4,608 successful
  decoder runs). The deterministic aggregate transcript SHA-256 is
  `ea6d6a2919c2ef35e70e3c78774c76226a83b28793002a10e923c42a39816047`;
  `tests/m3_production_matrix.json` SHA-256 is
  `edc0bcd2a6d761b4a8bd71a43690a76245afb8312c069965fd80d82d705c5662`.
- **Pattern/Q coverage:** 17x17 flat, ramp, impulse, and odd-sensitive cases
  pass, plus qindex 1, 100, 141, and 191 covering q-contexts 0, 1, 2, and 3.
  The frozen 14-case edge matrix remains exact.
- **Maximum case:** 64x64 qindex-100 produces 81 bytes, OBU SHA-256
  `d622c094be17589bd43c186953f35ba9701607118e7cb88fc6dc022fdd16f97d`,
  and reconstruction SHA-256
  `9b4f6dfdfcbf8fe70776bd6146924640e76ba5f869c011a95bcc9faa31c0c4ef`
  with SSE 22,496,640 and workspace 21,376 bytes.
- **Compatibility:** the legacy flat-128 stream remains 19 bytes with its
  prior SHA-256
  `870bd45c5a6cb2251b404c63ab6f44d75286329f94f8f5195f839cf6f5daf48c`.
  The complete 256-pair 1..16 validator matrix and existing qindex/default
  compatibility tests pass without syntax-path changes.
- **Memory/failure gates:** public maxima are 64 pixels/axis, 4,096 pixels,
  64 transforms, 4,352 stream bytes, and 21,376 workspace bytes. Exact maximum
  layout, padded stride, short workspace/stream/reconstruction, overlap,
  stride overflow, wrapper parity, and failure atomicity tests pass.
- **Validation:** full `make -C experimental/encoder test` passes 106 Python
  tests plus all native/shell gates; generated-CDF, dependency, static-ELF,
  input, and atomic-output audits pass. All 32 project CTests pass. A separate
  second exhaustive 2,304-case run reproduces the frozen aggregate hash.
  Encoder, pinned AVM, and project decoder SHA-256 values are respectively
  `4a3f1422fc1c5b0a84e9f7c5aad7311ddf460fe18995e645cf6ce15df8499a8c`,
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`,
  and `c0c6329cc85de13c92d04a00deff8124c9b45dbe74da8a86ddc60d9d7b4de9df`.
- **Remaining boundary:** dimensions above 64 and multi-superblock prepared
  images require row/above context extension; they are not part of this
  production promotion.

## 2026-08-15 — M3 multi-superblock and prepared-image promotion

- **Status:** PASS; `encoder-m3-multisuperblock-traversal` is complete.
- **Supported production workspace dimensions:** every width and height in
  1..896, including mixed axes. The bounded convenience wrappers retain their
  prior 64x64 arena limit.
- **Traversal/state:** 64x64 roots are row-major; boundary nodes use the
  rounded frame MI grid. Partition and coefficient left contexts reset when
  traversal wraps to a new superblock row. Above contexts, adaptive CDFs, and
  the reconstruction surface persist tile-wide. DC prediction therefore uses
  reconstructed neighbors across superblock boundaries.
- **Frame syntax boundary:** level index 0 is valid through the normative
  552,960-sample picture limit. Initial 896x672/896x896 probes correctly
  exposed that boundary in pinned AVM; level index 1 resolves it without
  changing any 1..64 stream.
- **Requested representatives:** 65x1, 1x65, 65x65, 127x64, 64x127,
  127x127, 128x128, and 129x129 pass non-flat qindex-191 local/AVM/project
  reconstruction parity. A separate 65x65 sweep passes qindex 1, 100, 141,
  and 191, covering all four coefficient q contexts. The independent model
  also covers 896x424, 598x896, and 896x896.
- **Prepared corpus:** 14/14 selected `le896` planes encode twice
  deterministically and 14/14 decode byte-exactly in pinned AVM and the
  project decoder. Dimensions range from 896x424 to 896x896. Frozen evidence
  is `tests/m3_multisuperblock_parity.json`.
- **Smallest selected case:** 896x424 qindex 100 emits 4,287 bytes, OBU
  SHA-256
  `3543639fdafcdeba3e0b8c4bb0c1bdb606ec02ed1b37494f3ede95fe728438b3`,
  decoded SHA-256
  `aa788a972d1c612dde892d5fc3f86a6d02e8b6e256e08592cf36de01e9ee0b3d`,
  SSE 225,686,847, and uses 1,912,160 workspace bytes.
- **Maximum selected case:** 896x896 uses 4,039,936 workspace bytes; all
  three selected 896x896 sources pass both decoders.
- **Compatibility/safety:** exact legacy tests and the frozen 2,304-case
  17..64 transcript remain unchanged. Workspace products, sums, alignment,
  overlaps, capacities, and stride reachability remain checked before output
  commit.
- **Evidence:** `tools/m3_multisuperblock_model.py`,
  `tests/test_m3_multisuperblock_model.py`,
  `tests/m3_multisuperblock_parity.json`, and
  `spec-notes/m3-multisuperblock-traversal.md`.
- **Validation:** full encoder gate passes 111 Python tests plus all native,
  shell, generated-CDF, dependency, static-ELF, input, and atomic-output
  checks. All 32 project CTests pass; freestanding syntax-only audits and
  `git diff --check` pass.

## 2026-08-15 — M3 complete prepared-image gate rerun

- **Status:** PASS; `encoder-m3-full-image` is complete.
- **Inventory:** the locked 28-preparation inventory has all 28 local complete
  I400 planes. The deterministic 14-source selection is 896x424 (1), 896x448
  (1), 896x504 (1), 896x598 (5), 598x896 (2), 896x672 (1), and 896x896 (3).
  Packed stride equals width, byte size equals width times height, and every
  preparation/source manifest and plane SHA-256 matched the lock/catalog.
- **Double gate:** two independent complete-corpus runs at qindex 100 each
  returned `pass=14, expected_blocked=0, blocked=0, error=0`. All 28 encoder
  invocations succeeded; all OBU rerun hashes, reconstruction hashes, and
  distortion records repeated exactly. The canonical timing-independent
  evidence SHA-256 was
  `5d19556ca20d6daeaaf4c0a048b2df0357932ff3fe5fe3a026b89eff630f3744`
  for both runs.
- **Parity/coverage:** all 14 local reconstructions equal pinned AVM and the
  project decoder byte-for-byte. Distortion sample counts equal active
  `width*height`; no padding enters SSE/PSNR. OBU sizes are 2,651..10,014
  bytes, SSE is 52,891,312..655,562,711, and PSNR is 17.761..28.187 dB.
- **Tool identities:** encoder
  `80669d241481f387333f14af3c9f653cb8d94ca0c87dc1f2b2e52a0257c7f54f`,
  pinned avmdec
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`,
  project decoder
  `c0c6329cc85de13c92d04a00deff8124c9b45dbe74da8a86ddc60d9d7b4de9df`,
  and prepared manifest
  `3fafa92351e97555180b8d42acbc55605a60c12dd1d6b65e8e1b9c36a5a381ce`.
- **Integration fix:** the corpus runner now resolves its work directory before
  invoking commands from the repository root; relative `--work-dir` values no
  longer produce false output-write failures. Nonzero `expected_blocked`
  counts now fail the complete-image gate.
- **Validation:** targeted M3 tests pass (18), full encoder tests pass (112
  Python plus native/shell/generated-CDF/dependency/static-ELF audits), all 32
  project CTests pass, and `git diff --check` passes. Default qindex 100 versus
  explicit 100 remains bit-identical; qindex 0/256 remain rejected.
- **Boundary:** this completes 8-bit I400 through 896 pixels per axis only.
  Arbitrary larger dimensions and chroma remain future work.

## 2026-08-15 — bounded 8-bit chroma DC admission

- Added a typed caller-owned multi-plane API without changing the I400 entry
  points or their streams.
- Admitted only I420 8×8/4×4/4×4 and I444 8×8/8×8/8×8, qindex 1..255.
- Generated the required chroma CDF subset from hash-locked AV2 v1.0
  attachments in a separate file so the frozen I400 normative subset and
  model hashes remain exact.
- Froze four flat/spatial vectors and passed 18 local == pinned AVM == project
  parity cases at qindex 1, 90, 91, 100, 140, 141, 190, 191, and 255.
- Passed `make -C experimental/encoder test`, the static/dependency audits,
  and all 32 project CTests.
- General I420/I444 remains blocked on odd plane geometry, multi-transform
  chroma traversal, reconstructed-neighbor prediction, and cross-leaf
  coefficient contexts/CDF adaptation.

## 2026-08-15 — odd I420 single-leaf geometry

- Derived active I420 chroma as ceil-divided dimensions and exposed active
  plus coded plane geometry and transform counts in the workspace query.
- Admitted square coded luma canvases 8×8 and 16×16: active 1..8 by 1..8 and
  9..15 by 9..15. Mixed coded axes and exact 16+ remain unsupported.
- Added edge-replicated luma search, raster four-node mapping, active crop,
  2-byte aligned extended workspaces, alias/capacity/atomicity checks, and
  active per-plane SSE. Extended U/V is deliberately context-neutral
  zero-residual coding; nonzero extended chroma belongs to the sibling
  adaptive-context task.
- Froze four independent odd/rectangle vectors and passed local == pinned AVM
  == project parity for 1×1, 2×7, 7×2, 7×7, 9×9, 9×15, 15×9, and 15×15.
- Preserved all I400 behavior and the four frozen 8×8 chroma hashes.

## 2026-08-16 — encoder quality baseline and roadmap

- **Status:** PASS for todo `encoder-quality-baseline`; no coding feature was
  implemented and no codec core source was changed.
- **Ranking:** first tranche is broader scalar 8x8 DCT_DCT AC coding, then
  horizontal/vertical intra prediction, then U/V plane quantizer deltas.
  Smooth/Paeth, CfL, deblocking, general partition search, and rate control are
  deferred in that order of dependency/validation cost. Exact 1..5 planning
  scores are in `featurematrix.md`.
- **Corpus:** qindices 60, 100, and 180; four full prepared sources
  (church, Milky Way, pastel chalks, and ceiling) in I400/I420/I444 produce 12
  full format images and 36 qindex cases. Six generated 64x64
  horizontal/vertical/diagonal edge, checkerboard, and two line patterns in all
  formats produce 18 targeted format images and 54 cases: 90 total.
- **Frozen result:** all 90 cases are deterministic across two encodes and
  pinned AVM/project decoded bytes agree exactly. Total OBU bytes are 278,134;
  decoded-plane aggregate SSE is Y 9,609,641,256, U 916,777,060, and V
  910,828,388. Maximum workspace is 7,263,744 bytes; median per-case median
  encode time is 8,192,059 ns and worst observed encode is 4,214,024,477 ns.
- **Hashes:** baseline JSON SHA-256
  `e25f70c5f32764684fbce76a2a3a7c3207892dfa5dbc5745bf6b002bdecce0f3`;
  encoder
  `7404f51cca977a5b583651733077626c16492ea321eb18446228f56a00bc6be6`;
  pinned AVM
  `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`;
  project decoder
  `7d17f598f2cdffe7dfb4ab46982cf5c4822215a5f429cb39e7748a60ce5ef535`.
- **Metric note:** seven chroma cases have small differences between
  encoder-reported SSE and SSE recomputed from the mutually exact validator
  output. The frozen comparator records both and uses decoded active-plane SSE
  and derived PSNR as authoritative; no core metric fix is claimed here.
- **Acceptance:** every candidate must fit the matching baseline byte count,
  retain the old mode, preserve default-off hashes, and pass exact validators.
  AC requires at least 8% targeted texture/line Y-SSE reduction and 9/27 wins;
  H/V requires at least 10% oriented-edge/line reduction and 12/36 wins; U/V
  deltas require at least 5% full-image chroma reduction and 6/24 wins. The
  combined tranche permits no aggregate Y/U/V regression or byte increase.
  Workspace is capped at +25% and median time at 4x unless baseline effort
  remains selectable.
- **Commands:** `make -C experimental/encoder all`;
  `python3 experimental/encoder/tools/encoder_quality_baseline.py`;
  `python3 -m unittest
  experimental/encoder/tests/test_encoder_quality_baseline.py`.

## 2026-08-16 — general 8x8 AC production candidate

- **Status:** BLOCKED for todo `encoder-ac8x8-production`; explicit I400
  candidate proven, locked production admission gate not met.
- Added scalar 8x8 forward/inverse DCT_DCT, complete scan/EOB/base/BR/
  remainder/sign syntax, adaptive CDFs, exact writer-tell RD costs, local
  reconstruction, bounded search, and DC/all-zero fallback.
- `--search off` is the default and preserves legacy streams.
  `--search bounded` is capped at 256 transforms, 16 positions, 8 nonzeros,
  and magnitude 63. I420/I444 fail explicitly pending chroma context work.
- Complete streams pass local == pinned AVM == project over four q contexts,
  8x8 patterns, a 9x15 crop, 16x16 multileaf, and 72x64 multi-superblock
  coverage. Caller workspace capacity/overflow/alias atomicity and the static
  no-libc ELF are tested.
- Locked targeted-I400 report: 18 cases, 9 overall wins; AC subset gain
  66.06%, 7/9 wins; bytes 4,459 versus 786 baseline. Maximum workspace is
  59,521 bytes and worst deterministic rerun encode is 187,888,069 ns.
- Follow-on: add deterministic whole-corpus q/rate-budget selection or a
  sparse exact-rate trellis that reaches the minimum wins while funding AC
  bits without aggregate byte or plane-SSE regression; then prove I420/I444
  luma/chroma context integration.
## 2026-08-16 — Cardinal directional production integration

- **Status:** PARTIAL/BLOCKED for default enablement; explicit search passes.
- **Admitted:** `--search directional`, I400/I420/I444, 1..896 axes; fixed
  production leaves 8x8/8x16/16x8/16x16 and their 8x8 luma transforms.
- **Syntax:** exact luma mode-set/index reordering, FSC=0, MRLS=0; color
  CfL=0 and DC UV syntax. Search-off remains byte-identical.
- **Targeted gate:** 64x64 vertical-edge streams win in all three formats,
  58→41, 95→80, and 98→82 bytes with unchanged plane SSE. Pinned AVM and
  project complete-stream outputs match exactly.
- **Runtime blocker:** the 90-case prepared/targeted baseline was attempted
  repeatedly; the best bounded run completed 34 cases in 600 seconds.
  The final implementation completed 4 case directories in a clean 60-second
  probe (about 22.5 minutes projected before decoding/rerun variance).
  Default search therefore remains off. Follow-on: reduce full-image trial
  cost or add a transform-local exact-writer state trial before rerunning the
  locked aggregate baseline.

## 2026-08-16 — Bounded directional fast-search model

- **Status:** DONE for the requested model/tool-only surface; production core
  intentionally unchanged.
- **Policy:** source-edge predictor SSE prefilter, transactional exact
  shortlisted stream trial, and commit only with no byte or per-plane SSE
  regression. The independent leaf model snapshots writer/CDF state and safely
  prunes candidates whose distortion lower bound already loses.
- **Agreement:** 90/90 locked stream decisions and 140/140 independent model
  decisions; 167 of 420 model candidates prune early.
- **Runtime:** exhaustive locked-case measurement 584.227 s; measured policy
  projection 268.155 s against the 600 s gate.
- **RD:** aggregate 278134→276706 bytes with no aggregate plane-SSE regression;
  the nine-case I400 directional subset is 525→436 bytes (16.952%) at identical
  SSE. All 72 shortlisted exact results reproduced deterministically.

## 2026-08-16 — Directional fast-search production gate

- **Status:** DONE. `--search directional` now applies the locked predictor-SSE
  prefilter before exact transactional DC/V/H stream trials;
  `--search directional-exact` preserves the exhaustive oracle path.
- **Safety:** 90/90 production decisions agree with the exact oracle. Explicit
  search-off hashes remain locked. Stable ties retain DC, then V, then H.
- **Gate:** 72 shortlisted, 18 early-pruned, 12 committed wins; aggregate
  278134→276771 bytes with no aggregate plane-SSE regression. The directional
  subset is 525→456 bytes (13.143%) with seven wins.
- **Parity/determinism:** all shortlisted reruns are deterministic; pinned AVM
  and project validation are exact for all 90 I400/I420/I444 cases. The final
  production matrix is 278134→276771 bytes; decoded aggregate Y/U/V SSE changes
  by -3622/-44314/-529398. Peak workspace remains 7263744 bytes.
- **Runtime/default:** 262.186 s under the 600 s gate. Ordinary default remains
  search-off because the production policy is still an expensive quality mode.

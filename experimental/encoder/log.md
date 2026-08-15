# Encoder experiment log

This is an append-only record of decisions, commands, pins, hashes, gate
results, blockers, and restart points. Do not replace exact values with links
or “latest”.

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

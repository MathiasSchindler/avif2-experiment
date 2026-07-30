# Decoder status snapshot

Runtime snapshot verified on 2026-07-30 at revision `03274c2` plus the current
Q-context-0 coefficient, primary-transform, cross-chroma-transform, and palette
work. The production Windows decoder was rebuilt and run against all 21
artifacts, with every emitted I420 compared by SHA-256 to its locked AVM
reference. Native CTest passes all nine Windows tests, including structured
diagnostic checks for luma inverse transform and chroma prediction.
The strict hosted artifact-profile harness builds and runs, but its temporary
bounded walks currently leave 64 failed assertions. The hosted tile fixture
also runs but retains 24 sparse-prefix CDF sample/hash assertions. The detailed
incomplete-file endpoints below are therefore diagnostic frontiers, not a
passing current hosted suite.

The public decoder completes 2 of the 21 `.obu` files in this directory.
`emerald-192x96-qp208.obu` and `chinnathurai-333x333-qp224.obu` decode to I420
files that are byte-identical to their locked AVM references. The other 19
files fail closed without output. `Tell` counts bits from the beginning of the
tile entropy stream. Block positions are expressed in 4x4 units.

## Current production blockers

These groups name the surface where the production decoder currently stops;
an earlier entropy divergence can still be the underlying cause.

| Current stopping surface | Files |
| --- | --- |
| Luma inverse transform | `chalk-320x240-qp128.obu`, `cuckoo-431x287-qp080.obu`, `flycatcher-223x149-qp176.obu`, `flycatcher-397x265-qp048.obu`, `laarim-448x299-qp192.obu`, `louis-301x222-qp144.obu`, `pastell-512x341-qp160.obu`, `schwaebisch-353x234-qp112.obu`, `schwaebisch-417x277-qp208.obu`, `silene-479x383-qp176.obu` |
| Luma reconstruction setup or special predictor | `chinnathurai-128x128-qp112.obu`, `cuckoo-157x105-qp160.obu` |
| Chroma prediction | `chimp-240x287-qp192.obu`, `chimp-93x111-qp064.obu`, `emerald-480x240-qp128.obu`, `laarim-256x171-qp096.obu`, `louis-463x342-qp224.obu`, `pastell-320x213-qp064.obu`, `silene-384x307-qp080.obu` |

| File | Current endpoint | Blocker |
| --- | --- | --- |
| `chalk-320x240-qp128.obu` | 15 luma and one chroma leaf committed; `(0,16)` 64x64 luma leaf reaches inverse transform at tell 554 | Later Q128 luma IST tuple |
| `chimp-240x287-qp192.obu` | 58 luma and one chroma leaf committed; `(0,16)` 64x64 U leaf reaches prediction at tell 1537 | Q192 chroma prediction profile |
| `chimp-93x111-qp064.obu` | 13 luma leaves committed; first 64x64 U block reaches chroma prediction at tell 173 | Q64 chroma prediction profile |
| `chinnathurai-128x128-qp112.obu` | 13 luma leaves committed; shell-set-1 IntraBC parsing advances `(8,12)` 16x8 from tell 588 to reference-availability validation at tell 630 | Earlier local syntax divergence produces an unavailable IntraBC reference; AVM has no IntraBC block at this local coordinate |
| `chinnathurai-333x333-qp224.obu` | **Complete:** emitted 166,667-byte I420 is byte-identical to the AVM reference (SHA-256 `6f1868d8945e87350414baa1a49829b21dbcb8c4e0e44dee6633d844ef5d7182`) | None for the accepted profile |
| `cuckoo-157x105-qp160.obu` | Seven luma leaves committed after DC 8x16 secondary set 6/type 1 clears; local `(0,8)` 32x32 reaches IntraBC source-bounds validation at tell 284 | Earlier syntax divergence: AVM's sole IntraBC block is `(0,6)` with vector `(-64,0)` and completes at tell 62 |
| `cuckoo-431x287-qp080.obu` | Three luma and two chroma leaves committed after the 64x64 D67 predictor clears; `(4,32)` 32x16 reaches inverse transform at tell 471 | Later Q80 luma IST tuple |
| `emerald-192x96-qp208.obu` | **Complete:** 20 luma and 11 chroma leaves, tile done at tell 731, filtered I420 byte-identical to the AVM reference (SHA-256 `403c2e7245ec484984e60c1eaf8bd1b47487c0ba6b60afc916b9532025fc51ad`) | None for the accepted profile |
| `emerald-480x240-qp128.obu` | First 64x64 luma predictor/output match AVM (`d4ea93e1129d09a3`/`2bc3d4c50c228831`); first 32x32 U/V blocks match through tell 197, range 53184, with prefilter hashes `57a7c38258f413ff`/`15846509fdb0b4f1` | Next luma/chroma traversal profile after the first exact leaf pair |
| `flycatcher-223x149-qp176.obu` | Two luma and one chroma leaf committed; next luma leaf reaches inverse transform at tell 132 | Later Q176 luma IST tuple |
| `flycatcher-397x265-qp048.obu` | 44 luma and six chroma leaves committed after the 8x4 D203 and 8x16 Smooth-Vertical predictors clear; `(0,48)` 64x16 reaches inverse transform at tell 1468 | Later Q48 luma IST tuple |
| `laarim-256x171-qp096.obu` | Seven luma and one chroma leaf committed; first chroma leaf reaches prediction at tell 1992 | Q96 chroma prediction profile |
| `laarim-448x299-qp192.obu` | 17 luma and four chroma leaves committed after the 4x16 D45 predictor clears; `(2,66)` 8x32 reaches inverse transform at tell 642 | Later Q192 luma IST tuple |
| `louis-301x222-qp144.obu` | First 64x16 luma leaf committed; `(4,0)` 16x16 FSC block clears coefficient levels and reaches inverse transform at tell 87 | Q144 FSC inverse transform |
| `louis-463x342-qp224.obu` | 78 luma and 39 chroma leaves committed; `(32,48)` 32x64 U leaf reaches prediction at tell 1318 | Q224 chroma prediction profile |
| `pastell-320x213-qp064.obu` | 14 luma leaves committed after the 64x32 Smooth-Horizontal predictor clears; first 64x64 U block reaches prediction at tell 960 | Q64 chroma prediction profile |
| `pastell-512x341-qp160.obu` | 20 luma and five chroma leaves committed; next luma leaf reaches inverse transform at tell 776 | Later Q160 luma IST tuple |
| `schwaebisch-353x234-qp112.obu` | Ten luma leaves committed after the 4x8 Smooth-Vertical predictor clears; `(2,8)` 16x8 reaches inverse transform at tell 374 | Later Q112 luma IST tuple |
| `schwaebisch-417x277-qp208.obu` | 200 luma and 30 chroma leaves committed; `(56,56)` 32x32 luma leaf reaches inverse transform at tell 3678 | Later Q208 luma IST tuple |
| `silene-384x307-qp080.obu` | Three luma and two chroma leaves committed; `(0,32)` 64x64 U leaf reaches prediction at tell 311 | Q80 chroma prediction profile |
| `silene-479x383-qp176.obu` | First luma and chroma leaves committed; `(0,16)` 64x64 luma leaf reaches inverse transform at tell 103 | Later Q176 luma IST tuple |

Summary:

- the public Windows decoder completes 2/21 artifacts. Of the 19 streams that
  fail closed without output, 12 report `integer overflow`, six report
  `invalid data`, and one reports `invalid argument`. Those retained statuses
  are coarse compatibility buckets, not root-cause categories. Structured
  diagnostics identify zero luma-parse, 12 luma-reconstruction, zero
  chroma-parse, and seven chroma-reconstruction stops. Every emitted I420
  matches its locked AVM reference;
- normative FSC geometry now admits every 4--32 dimension in luma mode,
  transform derivation, and coefficient-prefix parsing. Chimp Q64 advances
  from its first 8x8 mode at tell 30 to chroma prediction at tell 173, and
  Louis Q144 advances from mode parsing at tell 76 to FSC coefficient levels
  at tell 81;
- IntraBC mode parsing now supports both shell sets for integer and quarter-pel
  vectors, with revision-matched class and full 16-bit offset CDF banks. Its
  reconstruction path applies the normative two-tap bilinear predictor.
  Chinnathurai Q112 advances beyond shell-set-1 parsing to reconstruction;
- all six former luma inverse-transform blockers now advance through eight
  newly admitted live IST tuples: Horizontal 64x16 and 32x16 set 1/type 1,
  D157 16x16 set 4/type 3, Vertical 32x32 and DC 32x8 set 6/type 3,
  DC 8x16 set 6/type 1, D203 64x32 set 5/type 3, and Smooth 16x32 set 1/type 3. Structured
  diagnostics now report the secondary symbol and set. The chroma-prefix work
  exposes two later luma IST tuples that remain unsupported;
- all six former luma-prediction blockers now advance. Zero-angle D45, D113,
  D135, D157, and Vertical tuples use the existing IDIF predictor for the
  newly observed legal geometries; cardinal, Smooth, and Paeth prediction now
  synthesize missing boundary edges instead of rejecting legal frame-edge
  blocks. The chroma-prefix work exposes a later Q80 64x64 luma prediction
  tuple in Cuckoo;
- complete normative ordinary coefficient-level CDF banks now cover all five
  transform contexts for Q contexts 0--3. All nine former luma
  coefficient-level blockers advance to later syntax or reconstruction
  surfaces; no production artifact currently stops in luma coefficient levels;
- complete normative luma transform-block skip CDF banks now cover all ten
  skip contexts for every Q and transform-size context. All-zero blocks and
  large transforms use descriptor geometry instead of artifact whitelists;
  all genuine geometry/CDF luma coefficient-prefix blockers advance to later
  surfaces. Schwaebisch Q112 still surfaces there, but its range is already
  divergent before the prefix (`52000` local versus `60960` AVM);
- chroma coefficient prefixes now use the complete mutable intra skip bank for
  U, the independent normative V skip bank, and independent plane-context-2
  EOB-point banks for all Q contexts and transform capacities. Descriptor
  geometry replaces artifact/frame gates. All nine former chroma-prefix
  blockers advance, and no production artifact currently stops there;
- chroma primary transform type is now derived from all 13 intra chroma modes
  for compact transforms and aligned one-dimensional ADST on 64-long
  rectangles, with normative DCT_DCT fallback for reduced, DCT-only, and
  32-long rectangular geometries. U reads the adaptive CCTX symbol only for
  nonzero blocks with EOB greater than one; V never reads it. All five former
  chroma transform-type blockers advance, and no production artifact stops
  there;
- complete normative UV EOB/base/base-LF/BR coefficient-level CDF banks now
  cover Q contexts 0--3. Chroma blocks use descriptor geometry, including
  legal all-zero large transforms, instead of frame/Q/transform whitelists.
  All six former chroma coefficient-level blockers advance, and no production
  artifact currently stops in chroma coefficient levels;
- Laarim Q96 no longer fails in first-block prediction. The existing 32x32
  D45/0 no-neighbor IBP fallback was shadowed by the generic directional
  branch; ordering it first advances production from tell 471 with zero leaves
  to tell 584 after two committed luma leaves. Production now reaches the first
  32x32 U coefficient levels at tell 1786;
- Emerald Q208 is the first complete artifact: all locked luma/chroma handoffs
  reach tile completion at tell 731, frame filtering completes, and CTest
  verifies the emitted I420 byte-for-byte against the AVM reference;
- Chinnathurai Q224 is the second complete artifact. Its emitted I420 is
  byte-for-byte identical to the AVM reference; the former `(32,58)`
  EOB-15 coefficient frontier is no longer a production-decoder blocker;
- Palette prediction implements the two observed luma profiles: 8x8 blocks
  with palette sizes 6 and 2, 8-bit color tables, transverse and row-identity
  map syntax, adaptive neighbor contexts, exact predictor filling, and
  residual reconstruction. Pinned AVM runs confirm both neighbor color caches
  are empty and provide exact color-table/map hashes. Silene Q80 now reaches
  its later IntraBC coefficient-prefix divergence before either observed
  palette block, so palette support does not increase the production
  completion count;
- Q-context-0 now has complete luma TXS0--TXS4 and UV coefficient-level CDF
  banks, geometry-based coefficient admission, the ordinary intra primary
  transform table, the observed 32x32 chroma coefficient path, and inverse
  cross-chroma-transform integration. This advances Cuckoo Q80 through its
  complete 26-leaf hosted luma walk and Silene Q80 through its first chroma
  leaf, but neither stream completes in the production decoder;
- the reviewed census now classifies every named feature value observed in all
  21 artifacts as supported. This is feature coverage, not end-to-end decode
  coverage: coefficient-profile, entropy-context, traversal, and reconstruction
  frontiers still keep 19 files fail-closed;
- MRL index 2 is admitted across every directional mode, luma angle delta,
  legal prediction geometry, multi-line state, and IBP state. Chinnathurai Q224
  pins a single-line 16x32 predictor/output pair, while Schwaebisch Q208 pins a
  multi-line 16x8 pair at tell 706;
- MRL index 3 is admitted across every directional mode, luma angle delta,
  legal prediction geometry, multi-line state, and IBP state. Normative and
  pinned AVM derivative/filter tables cover the 4x4 D67/-3 case, while Chimp
  Q192 pins a live multi-line predictor/output pair and Schwaebisch Q208
  advances through its D203/+3 MRL-3 leaf at tell 4232;
- Multi-line MRL is admitted across every directional mode, luma angle delta,
  nonzero MRL index, legal prediction geometry, and IBP state. The selected
  reference line is blended with line 0 using AVM's rounded average, including
  exact cardinal directions and the normative 4x4 exception;
- IntraBC derives DRL references from decoded spatial block vectors, the
  normative four-entry recency bank, and ordered fallback vectors instead of
  coordinate-specific tuples. Both IntraBC modes and every legal skipped-copy
  geometry are covered by a 44-case matrix; the ordinary 22-leaf fixture pins
  six IntraBC leaves, including bank-shifted DRL 3 and chained vectors, through
  exact tells, source pixels, and reconstructed output at tell 762;
- Luma angle delta +1 is admitted through the shared IDIF predictor across
  every directional mode, legal prediction geometry, MRL index, multi-line
  state, and IBP state. Laarim Q96 pins a live 16x16 Horizontal/+1 DCT_ADST
  leaf through EOB 25, coefficient tells, predictor hash
  `75290844cee19be7`, and sample-exact reconstruction;
- Chroma angle delta -2 is admitted through the shared IDIF predictor across
  every directional mode and legal chroma prediction geometry. Schwaebisch
  Q208 pins a live 32x16 Horizontal/-2 leaf at tell 1372 with exact U/V
  predictor and reconstruction hashes `fc417191e6d1c7b6` and
  `3cfa5b1dcb7cf69b`;
- chroma angle deltas -3, -1, +1, and +3 are now admitted through the shared
  IDIF predictor across every directional mode and every 4/8/16/32 chroma
  prediction geometry. Together with the pinned +/-2 paths, all six nonzero
  chroma angle deltas are covered;
- Primary IDTX is admitted across all six signaled 32-long transform
  geometries for every intra mode, all five FSC geometries, and IntraBC 4x4.
  AVM identity scales, inverse shifts, rectangular scaling, signed rounding,
  and clipping are checked sample-by-sample; the ordinary tile fixture retains
  exact parsing and reconstruction hashes for three live IDTX leaves;
- Primary ADST_DCT is admitted across all 15 legal geometries for every intra
  mode: compact symbol 2 with the normative 4-class Paeth exception, and
  long-side-DCT symbol 1 for wide 32/64 transforms. Vertical ADST coefficient
  admission is matrix-tested in the complete ordinary Q1/Q3 banks; fixed
  reconstruction hashes cover every geometry/mode combination, including the
  newly completed 64x4 path, while Emerald retains its live 32x8 vector;
- Primary ADST_FLIPADST follows AVM's mode-dependent compact symbol tables
  across the 4, 8, and 16 size classes. Both transform axes are admitted only
  through 16 samples: complete Q1/Q3 coefficient matrices and pinned all-mode
  reconstruction hashes cover all nine legal geometries, every larger shape
  fails closed, and the live 8x4 D45 vector retains exact syntax and
  coefficient tells;
- Primary FLIPADST_ADST is admitted across all nine observed 4/8/16
  geometries. Complete Q1/Q3 coefficient matrices and fixed all-mode
  reconstruction hashes cover the shared FLIPADST-column/ADST-row inverse
  path;
- Primary H_ADST is admitted across the six observed 4x4, 4x8, 8x4, 8x8,
  16x4, and 16x8 geometries. Complete Q1/Q3 coefficient matrices and fixed
  all-mode reconstruction hashes cover horizontal ADST with vertical
  identity;
- Luma FSC is admitted across all five legal geometries and all four
  coefficient Q contexts. Complete normative BOB, base, range, and sign CDF
  banks are checked row-by-row through pinned hashes, and a 20-case synthetic
  matrix verifies coefficient decoding and dequantization alongside the live
  ordinary tile fixture;
- CfL prediction supports derived alpha, plane-specific explicit alpha, and
  MHCCP directions 0/1/2 across all 16 legal chroma coefficient geometries.
  Downsample filters 0/1/2, both chroma planes, no-neighbor fallback, and
  interior reference derivation are covered exhaustively; Emerald Q208,
  Silene Q176, and Chinnathurai Q224 retain their pinned live evidence;
- the hosted artifact-profile harness currently has 64 failed assertions.
  Its bounded walks and first-leaf expectations still encode sparse CDF
  substitutions and earlier stopping surfaces. Those expectations need to be
  reconciled with the complete normative banks before this can again be
  described as a passing suite;
- the hosted tile fixture currently has 24 stale sample/hash assertions after
  switching its ordinary path to the complete normative prefix CDF bank. Its
  entropy walk completes, but those pinned expectations still describe the
  previous sparse substitution state;
- Emerald Q128 and Silene Q176 now match AVM through their first 64x64 luma
  and 32x32 U/V reconstruction surfaces. Emerald reaches tell 197/range 53184;
  Silene reaches tell 71/range 53320 with exact plane-specific explicit-CfL
  predictors. Both still fail closed before completing their next profile;
- Schwaebisch Q208 advances through superblock index 31 and tell 4236 after
  correcting TX_8X16 entropy context, admitting three exact predictor
  geometries, clipping coefficient contexts at frame edges, and admitting
  MRL index 3 generically; its next gap is the `(66,48)` 32x8 D203/0
  reconstruction admission;
- Chimp Q192 now matches the pinned AVM entropy path through superblocks 11,
  12, and 13 and advances deep into superblock 14. Its `(56,38)` 4x8
  Vertical/-3 leaf with MRL index 3, multi-line MRL, and IBP enabled has exact
  predictor/output hashes `8cf672b166c4e9ab`/`8cf672b166c4e9ab`. The current
  frontier is `(56,39)` at tell 8033/range 39832 after 271 luma and 16 chroma
  leaves. The current production decoder fails closed with `integer overflow`.
  This work adds the required FSC 8x4 path, exact Q3 coefficient CDF rows,
  pinned primary-transform rows, IST combinations, directional and multi-line
  predictors, and inverse transform admissions while retaining fail-closed
  tuple checks;
- the Q64 first-block frontiers advance to tells 30 and 34 for Chimp and
  Pastell respectively;
- Cuckoo Q160, Pastell Q160, and Louis Q224 retain their locked sample-exact
  partial reconstructions; and
- the remaining artifacts retain their earlier Q-profile, large-transform
  coefficient, prediction, or inverse-reconstruction blockers.

The census now identifies Chinnathurai Q224, Emerald Q208, and Louis Q224 as
feature-complete candidates at its named-feature granularity. Only the first
two complete end-to-end; Louis Q224 still fails closed at its documented
profile/traversal frontier.

The ordinary 22-leaf/tell-762 fixture is not part of this artifact directory.
Frame-wide decode and filter integration is now demonstrated for Emerald and
Chinnathurai, but general artifact coverage remains Milestone 8/9 work.

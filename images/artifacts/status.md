# Decoder status snapshot

Runtime snapshot verified on 2026-07-30 at revision `03274c2` plus the current
prediction, inverse-transform, Q-context-0 coefficient, primary-transform,
cross-chroma-transform, and palette work. The production Windows decoder was
rebuilt and run against all 21
artifacts, with every emitted I420 compared by SHA-256 to its locked AVM
reference. Native CTest passes all nine Windows tests, including structured
diagnostic checks for luma inverse transform and luma parsing. The focused
hosted reconstruction unit retains exactly 17 established baseline failures.
The detailed incomplete-file endpoints below are therefore diagnostic
frontiers, not a claim that every hosted fixture passes.

The public decoder completes 2 of the 21 `.obu` files in this directory.
`emerald-192x96-qp208.obu` and `chinnathurai-333x333-qp224.obu` decode to I420
files that are byte-identical to their locked AVM references. The other 19
files fail closed without output. `Tell` counts bits from the beginning of the
tile entropy stream. Block positions are expressed in 4x4 units.

The [decoder blocker map](blocker-map.md) complements the next-stop snapshot
with complete-frame AVM accounting exposure for luma/chroma partition, mode,
coefficient-prefix, coefficient-level, transform, and frame-filter surfaces.

## Current production blockers

These groups name the surface where the production decoder currently stops;
an earlier entropy divergence can still be the underlying cause.

| Current stopping surface | Files |
| --- | --- |
| Luma coefficient prefix | `schwaebisch-417x277-qp208.obu` |
| Luma prediction | None |
| Luma IntraBC source bounds | `chinnathurai-128x128-qp112.obu` |
| Luma IntraBC reference availability | `silene-384x307-qp080.obu` |
| Luma inverse transform | `chalk-320x240-qp128.obu`, `chimp-240x287-qp192.obu`, `cuckoo-157x105-qp160.obu`, `cuckoo-431x287-qp080.obu`, `emerald-480x240-qp128.obu`, `flycatcher-223x149-qp176.obu`, `flycatcher-397x265-qp048.obu`, `laarim-256x171-qp096.obu`, `laarim-448x299-qp192.obu`, `louis-301x222-qp144.obu`, `pastell-320x213-qp064.obu`, `pastell-512x341-qp160.obu`, `silene-479x383-qp176.obu` |
| Chroma parse | None |
| Chroma deblock map | `schwaebisch-353x234-qp112.obu` |
| Frame filter | `chimp-93x111-qp064.obu` |

| File | Current endpoint | Blocker |
| --- | --- | --- |
| `chalk-320x240-qp128.obu` | 20 luma and six chroma leaves committed; `(16,16)` 32x64 Y leaf reaches inverse transform at tell 875 | Q128 luma inverse-transform profile |
| `chimp-240x287-qp192.obu` | 78 luma and two chroma leaves committed; `(12,34)` 8x16 Y leaf reaches inverse transform at tell 1958 | Q192 luma inverse-transform profile |
| `chimp-93x111-qp064.obu` | Tile reconstruction completes with 26 luma and nine chroma leaves at tell 950; decoding reaches the multi-strength CDEF gate, but all three reconstructed-plane hashes already differ from AVM prefilter hashes | Upstream reconstruction parity before frame filtering |
| `chinnathurai-128x128-qp112.obu` | 41 luma and ten chroma leaves committed; `(2,16)` 16x8 Y reaches IntraBC source-bounds validation at tell 2124; local DRL 0 falls back to candidate `(0,-512)` and produces `(-160,-504)`, while AVM produces `(-160,-24)` | Upstream IntraBC candidate-stack/traversal divergence |
| `chinnathurai-333x333-qp224.obu` | **Complete:** emitted 166,667-byte I420 is byte-identical to the AVM reference (SHA-256 `6f1868d8945e87350414baa1a49829b21dbcb8c4e0e44dee6633d844ef5d7182`) | None for the accepted profile |
| `cuckoo-157x105-qp160.obu` | 26 luma and two chroma leaves committed; `(8,17)` 8x32 Y leaf reaches inverse transform at tell 629 | Q160 luma inverse-transform profile |
| `cuckoo-431x287-qp080.obu` | 29 luma and three chroma leaves committed; `(0,40)` 32x16 Y leaf reaches inverse transform at tell 1908 | Q80 luma inverse-transform profile |
| `emerald-192x96-qp208.obu` | **Complete:** 20 luma and 11 chroma leaves, tile done at tell 731, filtered I420 byte-identical to the AVM reference (SHA-256 `403c2e7245ec484984e60c1eaf8bd1b47487c0ba6b60afc916b9532025fc51ad`) | None for the accepted profile |
| `emerald-480x240-qp128.obu` | 13 luma and nine chroma leaves committed; `(24,8)` 16x32 Y advances through prediction and reaches inverse transform at tell 1077 | Q128 luma inverse-transform profile |
| `flycatcher-223x149-qp176.obu` | 24 luma and two chroma leaves committed; `(0,32)` 32x16 Y reaches inverse transform at tell 711 | Q176 luma inverse-transform profile |
| `flycatcher-397x265-qp048.obu` | First two chroma leaves match pinned AVM through tell 1235; 44 luma and four chroma leaves commit before `(6,40)` 32x8 Y reaches inverse transform at tell 2580 | Q48 luma inverse-transform profile |
| `laarim-256x171-qp096.obu` | 19 luma and 13 chroma leaves committed; `(10,38)` 8x16 Y reaches inverse transform at tell 4399 | Q96 luma inverse-transform profile |
| `laarim-448x299-qp192.obu` | 22 luma and four chroma leaves committed; `(8,72)` 32x8 Y leaf reaches inverse transform at tell 800 | Q192 luma inverse-transform profile |
| `louis-301x222-qp144.obu` | 16 luma and five chroma leaves committed; `(8,64)` 64x32 Y leaf reaches inverse transform at tell 955 | Q144 luma inverse-transform profile |
| `louis-463x342-qp224.obu` | **Complete:** emitted 237,690-byte I420 (SHA-256 `f9492abd68347c32e64255a8dc3788ed3720f1ea3700b0ad8e17567192ad27ef`) | Output is not yet byte-identical to AVM; the first difference is luma sample `(0,88)`, before the Louis-specific syntax additions |
| `pastell-320x213-qp064.obu` | 28 luma and seven chroma leaves committed; `(8,48)` 16x16 Y reaches inverse transform at tell 1396 | Q64 luma inverse-transform profile |
| `pastell-512x341-qp160.obu` | 27 luma and ten chroma leaves committed; `(20,0)` 32x16 Y leaf reaches inverse transform at tell 928 | Q160 luma inverse-transform profile |
| `schwaebisch-353x234-qp112.obu` | 72 luma and 21 chroma leaves committed; narrow chroma transform geometry advances past the `(8,72)` U prefix to `(8,73)` deblock-map commit at tell 2521 | Upstream chroma partition divergence: repeated vertical decisions at `(8,72)` produce a non-conformant 4x32 leaf with an odd chroma origin |
| `schwaebisch-417x277-qp208.obu` | 242 luma and 45 chroma leaves committed; `(66,96)` 32x8 Y reaches coefficient-prefix parsing at tell 4374 | Q208 luma coefficient-prefix profile |
| `silene-384x307-qp080.obu` | Seven luma and one chroma leaf committed; local `(4,24)` 32x16 Y decodes IntraBC and reaches reference validation at tell 489, but pinned AVM has no IntraBC block at that position | Upstream luma mode entropy/traversal divergence |
| `silene-479x383-qp176.obu` | 11 luma and six chroma leaves committed; `(8,36)` 8x32 Y reaches inverse transform at tell 462 | Q176 luma inverse-transform profile |

Summary:

- the public Windows decoder completes 2/21 artifacts. The other 19 streams
  fail closed without output. The fresh production census identifies zero
  one luma coefficient-prefix stop, zero luma-prediction stops, one IntraBC
  source-bounds stop, one IntraBC-reference stop, 13 luma inverse-transform
  stops, one chroma-parse stop, one chroma-deblock-map stop, and one
  frame-filter stop. Every emitted I420 matches
  its locked AVM reference;
- normative FSC geometry now admits every 4--32 dimension in luma mode,
  transform derivation, and coefficient-prefix parsing. Chimp Q64 advances
  from its first 8x8 mode at tell 30 to chroma prediction at tell 173, and
  Louis Q144 advances from mode parsing at tell 76 to FSC coefficient levels
  at tell 81;
- IntraBC mode parsing now supports both shell sets for integer and quarter-pel
  vectors, with revision-matched class and full 16-bit offset CDF banks. Its
  reconstruction path applies the normative two-tap bilinear predictor.
  Chinnathurai Q112 advances beyond shell-set-1 parsing to reconstruction;
- 15 exact live IST tuples now dispatch through existing normative kernels,
  advancing every affected artifact to a later blocker. Admission remains
  scoped by mode, transform geometry, secondary set, and secondary type;
- all original luma-prediction blockers now advance. Zero-angle D45, D113,
  D135, D157, D203, Horizontal, and Vertical tuples use the existing IDIF
  predictor for newly observed legal geometries; cardinal, Smooth, and Paeth
  prediction synthesize missing boundary edges for legal frame-edge blocks;
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
- generated chroma and inter EOB rows now initialize adaptation from their
  parameter slot rather than the adjacent count slot. Flycatcher Q48's first
  U/V pair matches pinned AVM exactly through EOB 78/7 and tell 1050, and its
  second chroma leaf remains aligned through tell 1235;
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
- Palette prediction now also implements Silene Q80's exact 16x16/two-color
  tuple. Palette indices are bit-packed in the existing 64-byte bounded map,
  preserving freestanding linkage while supporting the 256-sample map.
  Silene advances from tell 553 with 14 luma leaves to tell 1266 with 28;
- the CDEF kernel can select Y/UV strengths from the parsed per-64x64 unit
  map, and the focused filter unit covers selection of strength entry 1.
  Production still rejects multi-strength frames with filter reason 4.
  Chimp Q64 is now the only stream reaching that gate; Chinnathurai Q112 stops
  earlier at IntraBC source bounds. An ungated Chimp trial differed in 15,374
  of 15,587 output bytes, and its local prefilter hashes
  `850f522712b4b74c/42bbd333f17049bf/ae60306cfbedd72c` already differ from AVM
  `16848db5a032b117/9bcde1179fd91d15/32cb3af20552bbfe` on Y/U/V. The first
  parity break is therefore upstream of frame filtering, so the gate remains;
- frame filtering now records deterministic per-plane hashes after
  reconstruction, deblocking, CDEF, CCSO, and final GDF/output. The hosted
  filter unit verifies that a known CCSO pixel change first alters the CCSO
  hash while the disabled-GDF final hash remains identical. Multi-strength
  failures also expose reconstruction-stage hashes before returning reason 4;
- structured Windows failures now retain the latest 32 partition decisions
  and Y/UV leaves with entropy tells. For Schwaebisch Q112 this proves that
  the deblock-map stop is downstream of three vertical chroma decisions at
  `(8,72)`, ending at tell 2519 and producing adjacent 4x32 leaves at columns
  72 and 73. Pinned AVM geometry contains no corresponding invalid chroma
  origin, so production remains fail-closed at the upstream traversal drift;
- Louis Q224 now completes after admitting its observed D135 8x32 predictor,
  existing IST kernels for its additional mode/geometry tuples, and secondary
  transform signaling for DCT_DCT 64x8 and smooth ADST_ADST 16x16 at EOB 12.
  The emitted frame is not byte-identical to pinned AVM: the first difference
  is luma sample `(0,88)`, before these Louis-specific syntax additions;
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
- all 14 former chroma-prediction blockers advance. Zero-angle directional,
  Smooth, Smooth-Vertical, DC, Paeth, and cardinal modes cover every legal
  4/8/16/32 geometry; unavailable frame edges use AVM's 127/129/128 defaults
  or mirror the sole available edge. MHCCP supports downsample filter 0 and
  direction-1 prediction at the frame top. No artifact currently stops in
  chroma prediction;
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
- Cuckoo Q160 and Pastell Q160 retain their locked sample-exact partial
  reconstructions, while Louis Q224 now produces a complete frame; and
- the remaining artifacts retain their earlier Q-profile, large-transform
  coefficient, prediction, or inverse-reconstruction blockers.

The census now identifies Chinnathurai Q224, Emerald Q208, and Louis Q224 as
feature-complete candidates at its named-feature granularity. All three
complete end-to-end; Chinnathurai Q224 and Emerald Q208 are byte-identical to
their pinned AVM outputs, while Louis Q224 still has reconstruction differences.

The ordinary 22-leaf/tell-762 fixture is not part of this artifact directory.
Frame-wide decode and filter integration is now demonstrated for Emerald and
Chinnathurai, but general artifact coverage remains Milestone 8/9 work.

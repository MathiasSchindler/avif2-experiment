# Decoder status snapshot

Runtime snapshot verified on 2026-08-02 from a fresh rebuild of the current
worktree. The production Windows decoder was run against all 21 artifacts, and
every emitted I420 was compared by SHA-256 to its locked AVM reference. The
detailed incomplete-file endpoints below are diagnostic frontiers, not a claim
that every hosted fixture passes.

The public decoder emits I420 for 12 of the 21 `.obu` files in this directory.
Chalk Q128, Chimp Q64, both Chinnathurai artifacts, Cuckoo Q160, Emerald Q208,
Pastell Q64, and Schwaebisch Q208 are byte-identical to their locked AVM
references. Chimp Q192, Cuckoo Q80, Louis Q144, and Silene Q176 decode
completely but remain nonexact. The other nine files fail closed without output,
and no artifact crashes. `Tell` counts bits from the beginning of the tile entropy stream.
Block positions are expressed in 4x4 units.

The [decoder blocker map](blocker-map.md) complements the next-stop snapshot
with complete-frame AVM accounting exposure for luma/chroma partition, mode,
coefficient-prefix, coefficient-level, transform, and frame-filter surfaces.

## Current production blockers

These groups name the surface where the production decoder currently stops;
an earlier entropy divergence can still be the underlying cause.

| Current stopping surface | Files |
| --- | --- |
| Luma coefficient prefix | None |
| Luma coefficient levels | `flycatcher-223x149-qp176.obu` |
| Luma prediction | `laarim-256x171-qp096.obu` |
| Luma partition or geometry | `louis-463x342-qp224.obu` |
| Luma IntraBC source bounds | `schwaebisch-353x234-qp112.obu` |
| Luma IntraBC reference availability | None |
| Chroma IntraBC reference availability | None |
| Luma inverse transform | `emerald-480x240-qp128.obu`, `schwaebisch-353x234-qp112.obu` |
| Chroma coefficient levels | None |
| Chroma parse | `flycatcher-397x265-qp048.obu`, `pastell-512x341-qp160.obu`, `silene-384x307-qp080.obu` |
| Chroma deblock map | None |
| Frame filter | None |
| Completeness | `laarim-448x299-qp192.obu` |
| Output parity | `chimp-240x287-qp192.obu`, `cuckoo-431x287-qp080.obu`, `louis-301x222-qp144.obu`, `silene-479x383-qp176.obu` |

| File | Current endpoint | Blocker |
| --- | --- | --- |
| `chalk-320x240-qp128.obu` | **Complete:** zero-angle D203 8x16 IBP prediction now follows the mapped IDIF path. The emitted 115,200-byte I420 is byte-identical to AVM (SHA-256 `4ba52509cafcc1442db604efbbb30c98f6a79bb3441f6b2803559d0d1399cddf`) | None for the accepted profile |
| `chimp-240x287-qp192.obu` | **Decode complete but nonexact:** the prefilter output already differs from AVM in 64,293 Y, 16,093 U, and 15,842 V samples. The first mismatch is Y `(80,0)`. At that leaf local mode decoding enters at tell 982 and selects mode index 0, while AVM finishes at tell 986 with mode index 1; the visible divergence is downstream of an earlier two-bit entropy drift. Final output has 64,743 Y, 16,215 U, and 15,995 V mismatches (SHA-256 `14a5cd416e973c32dcb9aa7ee330925ec9b613172b6618dd45b045027cd6627a`) | Upstream luma syntax/lifecycle parity before MI `(20,0)` mode decoding |
| `chimp-93x111-qp064.obu` | **Complete:** chroma CDEF now forces direction zero when primary strength is zero, matching AVM. The emitted I420 is byte-identical to the locked reference (SHA-256 `033f9a28603d8e324c51fc7a6f0593876dde613759a7593ff3b4059a5d7ea936`) | None for the accepted profile |
| `chinnathurai-128x128-qp112.obu` | **Complete:** emitted I420 is byte-identical to the AVM reference (SHA-256 `d1c394ab05667a33bd5974a22ceb3eb94267b36990fe64ee12ba2a3c7e51122a`) | None for the accepted profile |
| `chinnathurai-333x333-qp224.obu` | **Complete:** emitted 166,667-byte I420 is byte-identical to the AVM reference (SHA-256 `6f1868d8945e87350414baa1a49829b21dbcb8c4e0e44dee6633d844ef5d7182`) | None for the accepted profile |
| `cuckoo-157x105-qp160.obu` | **Complete:** emitted I420 is byte-identical to the AVM reference (SHA-256 `01fa3b4d41665f9297edc5ad116a65618dac3069adfb6a83f83f7e05aa5010a`) | None for the accepted profile |
| `cuckoo-431x287-qp080.obu` | **Decode complete but nonexact:** the 32x4 Vertical prediction route completes the stream. Output differs from AVM in 1,790 of 185,905 samples, with maximum absolute error 5; SHA-256 is `adcda8afa85971eb49622c9b725eac4aa99e9d24278b9120ad56c208e443983c` | Localized output parity |
| `emerald-192x96-qp208.obu` | **Complete:** 20 luma and 11 chroma leaves, tile done at tell 731, filtered I420 byte-identical to the AVM reference (SHA-256 `403c2e7245ec484984e60c1eaf8bd1b47487c0ba6b60afc916b9532025fc51ad`) | None for the accepted profile |
| `emerald-480x240-qp128.obu` | Production reaches `(34,112)` 32x4 Y IDTX inverse transform at tell 83,079 after 843 luma and 122 chroma leaves | Unsupported IDTX inverse-transform profile |
| `flycatcher-223x149-qp176.obu` | Production reaches `(36,32)` 16x8 Y coefficient levels at tell 3,083 after 145 luma and 45 chroma leaves | Luma coefficient-level divergence |
| `flycatcher-397x265-qp048.obu` | 32x4 D113 prediction and 4x4 IST set3/type2 advance production to escaped UV mode 13 at `(64,36)` 16x8 U, tell 165,656, after 2,419 luma and 738 chroma leaves | Upstream entropy divergence before chroma mode parsing |
| `laarim-256x171-qp096.obu` | MRL-aware zero-angle transform-mode mapping advances production to `(40,4)` 16x16 Smooth Y reconstruction at tell 35,210 after 453 luma and 194 chroma leaves | Unsupported secondary-transform profile at the advanced luma reconstruction frontier |
| `laarim-448x299-qp192.obu` | 4x4 IST set1/type3 and Smooth-Vertical edge extension consume the tile to tell 5,416 after 281 luma and 47 chroma leaves | Reconstruction/readiness maps remain incomplete at end of tile |
| `louis-301x222-qp144.obu` | **Decode complete but nonexact:** output differs from AVM in 2,453 of 100,344 samples, with maximum absolute error 4; SHA-256 is `a82186c742715bfe673d14e68ad35aec8e7ca7049ca8f19d899e6ab3e7f5022e` | Localized output parity |
| `louis-463x342-qp224.obu` | Production fails closed in luma partition/geometry at tell 2,570 after 188 luma and 49 chroma leaves | Upstream partition traversal divergence |
| `pastell-320x213-qp064.obu` | **Complete:** GDF control rows now use the normative `-8` vertical block origin, so rows 120-127 select the second 128-pixel block flag. The emitted 102,400-byte I420 is byte-identical to AVM (SHA-256 `957eaf1ed0c372123eb1e98ac8d279d35834c6d09d48426acf48380d1a5333fb`) | None for the accepted profile |
| `pastell-512x341-qp160.obu` | Production reaches escaped UV mode 14 at `(80,0)` 8x16 U, tell 26,514, after 967 luma and 1,087 chroma leaves | Upstream entropy divergence before chroma mode parsing |
| `schwaebisch-353x234-qp112.obu` | Additional 4x4 IST kernels advance production to `(20,46)` 8x16 Y inverse transform at tell 19,139 after 518 luma and 71 chroma leaves | Unsupported 8x8 IST set9/type1 profile |
| `schwaebisch-417x277-qp208.obu` | **Complete:** stale out-of-frame bottom-left mode context, partial-boundary extension signaling, cropped derived-CfL, and directional/Smooth edge profiles are fixed; local and AVM entropy match through the final logged luma endpoint at tell/range `4360/51200`, and the emitted 173,611-byte I420 is byte-identical to AVM (SHA-256 `6fbc5758a05d727ed05b04542de796cc72ca5dd9c402213c3e2d4ec72ae78428`) | None for the accepted profile |
| `silene-384x307-qp080.obu` | Production reaches escaped UV mode 14 at `(72,24)` 32x8 U, tell 113,538, after 1,257 luma and 501 chroma leaves; the invalid mode remains downstream of earlier luma entropy divergence | Upstream entropy divergence before chroma mode parsing |
| `silene-479x383-qp176.obu` | **Decode complete but nonexact:** prefilter divergence begins at Y `(448,131)` and is broad before filtering. Final output differs in 52,483 Y, 7,984 U, and 20,941 V samples; SHA-256 remains `9863268854dd0b81e5f3c286f197fb8995684645d67ef3c970a848cf9a0ec409` | Upstream luma syntax/reconstruction parity before `(448,131)` |

Summary and implementation history:

The first two bullets describe the current production snapshot. Later bullets
preserve the checkpoint at which each feature was implemented and can mention
frontiers that have since moved.

- the public Windows decoder emits output for 12/21 artifacts; eight are
  byte-identical to AVM and four retain output-parity gaps. The other nine
  streams fail closed without output, and none crashes. The fresh production
  census identifies two luma coefficient-prefix stops, two luma
  coefficient-level stops, two luma-prediction stops, one luma
  partition/geometry stop, one luma IntraBC source-bounds stop, zero luma
  inverse-transform stop, two chroma-parse stops, and three output-parity
  mismatches;
- at an earlier checkpoint, the final luma inverse-transform stop was Cuckoo
  Q80's D203/+1 TX_4X4
  set7/type1 tuple. The pinned AVM 4x4 IST kernel advances production from
  `(46,58)` at tell 46,796 to the later escaped UV mode 14 at `(44,48)` and
  tell 48,022. That all-artifact sweep had zero production luma
  inverse-transform stops before later prediction support exposed the current
  Schwaebisch Q112 tuple;
- zero-angle D203 4x32 and Vertical 8x4 prediction advance Louis Q144 and
  Schwaebisch Q112. Pinned AVM 4x4 IST set8/type2, set0/type1, set12/type1,
  and set4/type1 kernels move Louis to coefficient levels at tell 20,033 and
  Schwaebisch to the next inverse-transform tuple at tell 17,767;
- pinned AVM 4x4 IST set1/type2 and zero-angle D135 64x16 prediction advance
  Schwaebisch Q112 to tell 22,020. The resulting local 8x8 DC IntraBC block at
  `(26,56)` is downstream divergence: AVM has a 4x4 Smooth-Horizontal,
  non-IntraBC block at the same MI, so out-of-frame source rejection remains
  fail-closed;
- luma CDEF now follows AVM's secondary-only path by forcing direction zero
  when primary strength is disabled, matching the existing chroma rule. This
  removes 67 Pastell Q64 Y mismatches (10,687 to 10,620); Chalk, Chimp Q192,
  and Silene Q176 mismatch counts are unchanged;
- normative FSC geometry now admits every 4--32 dimension in luma mode,
  transform derivation, and coefficient-prefix parsing. Chimp Q64 advances
  from its first 8x8 mode at tell 30 to chroma prediction at tell 173, and
  Louis Q144 advances from mode parsing at tell 76 to FSC coefficient levels
  at tell 81;
- IntraBC mode parsing now supports both shell sets for integer and quarter-pel
  vectors, with revision-matched class and full 16-bit offset CDF banks. Its
  reconstruction path applies the normative two-tap bilinear predictor.
  Chinnathurai Q112 advances beyond shell-set-1 parsing to reconstruction;
- observed live IST tuples now dispatch through existing normative kernels,
  advancing every affected artifact to a later blocker. Admission remains
  scoped by mode, transform geometry, secondary set, and secondary type;
- all original luma-prediction blockers now advance. Zero-angle D45, D113,
  D135, D157, D203, Horizontal, and Vertical tuples use the existing IDIF
  predictor for newly observed legal geometries; cardinal, Smooth, and Paeth
  prediction synthesize missing boundary edges for legal frame-edge blocks;
- at an earlier checkpoint, complete normative ordinary coefficient-level CDF banks covered all five
  transform contexts for Q contexts 0--3. All nine former luma
  coefficient-level blockers advance to later syntax or reconstruction
  surfaces; later predictor support has since exposed coefficient-level stops;
- complete normative luma transform-block skip CDF banks now cover all ten
  skip contexts for every Q and transform-size context. All-zero blocks and
  large transforms use descriptor geometry instead of artifact whitelists;
  all genuine geometry/CDF luma coefficient-prefix blockers advance to later
  surfaces. Schwaebisch Q112 still surfaces there, but its range is already
  divergent before the prefix (`52000` local versus `60960` AVM);
- the 2026-08-01 luma coefficient-prefix audit found no missing prefix syntax
  or CDF family among the three stopping surfaces. Their shared cause was an
  IntraBC reference-bank reset performed after the first leaf of a superblock
  row instead of before it. Matching AVM's row lifecycle advances Emerald
  Q128, Pastell Q64, Silene Q176, Louis Q144, and Silene Q80 beyond their
  former mode, EOB, skip, palette, and coefficient-level divergences without
  admitting invalid syntax;
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
  for compact transforms and orientation-compatible ADST on 32/64-long
  rectangles, with normative DCT_DCT fallback for reduced, DCT-only, and
  incompatible rectangular transforms. U reads the adaptive CCTX symbol only for
  nonzero blocks with EOB greater than one; V never reads it. All five former
  chroma transform-type blockers advance, and no production artifact stops
  there;
- complete normative UV EOB/base/base-LF/BR coefficient-level CDF banks now
  cover Q contexts 0--3. Chroma blocks use descriptor geometry, including
  legal all-zero large transforms, instead of frame/Q/transform whitelists.
  All six former chroma coefficient-level profile gaps advance. Cuckoo Q160's
  remaining apparent V-level stop is a phantom block after pinned AVM has
  already completed, so it is tracked as upstream traversal divergence rather
  than an unsupported coefficient profile;
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
- the CDEF kernel selects Y/UV strengths from the parsed per-64x64 unit map,
  and the focused filter unit covers selection of strength entry 1.
  Production now executes multi-strength filtering for Chimp Q64 instead of
  returning its unfiltered reconstruction. Its final luma plane is byte-exact;
  209 U and 147 V samples still differ, rooted in the existing chroma
  reconstruction gap before frame filtering;
- IntraBC now consumes the adaptive morph-pred symbol after its displacement,
  using the normative three-context CDF and immediate above/left morph state.
  Silene Q80 matches pinned AVM through vector `(-256,-136)`, morph range
  `45704`, EOB 30, and final EOB range `35952` at `(7,20)`, advancing from
  seven to 22 committed luma leaves. The later false IntraBC at `(8,21)` is
  downstream entropy divergence rather than reference-availability failure;
- frame filtering now records deterministic per-plane hashes after
  reconstruction, deblocking, CDEF, CCSO, and final GDF/output. The hosted
  filter unit verifies that a known CCSO pixel change first alters the CCSO
  hash while the disabled-GDF final hash remains identical. Multi-strength
  failures also expose reconstruction-stage hashes before returning reason 4;
- intra GDF training-QP selection now follows AVM's Q147 and Q172 thresholds.
  On Flycatcher Q176 this reduces luma mismatches from 9,163 to 7,769 and moves
  the first mismatch from `(92,38)` to `(71,56)`. Reconstruction through CCSO
  matches at the former frontier; the remaining first error is introduced by
  GDF at its first stripe boundary;
- chroma partition axis signaling now matches AVM for 8x32 and 32x8 blocks.
  Their only chroma-valid ordinary axes are implied, so production no longer
  consumes an axis symbol or produces odd-origin 4x32/32x4 chroma leaves.
  Pastell Q64 and Schwaebisch Q112 both advance from chroma deblock-map stops
  to later luma inverse-transform frontiers;
- At an earlier checkpoint, Louis Q224 completed byte-identically after admitting its observed D135
  8x32 predictor, existing IST kernels for its additional mode/geometry tuples,
  and secondary transform signaling for DCT_DCT 64x8 and smooth ADST_ADST
  16x16 at EOB 12. Reconstruction parity additionally required normative MRL
  superblock references, multi-line edge handling, canonical IBP weights,
  wide-angle mapping, coded-edge references, chroma DC availability, and
  orientation-compatible chroma transforms. Normal filtering produces the
  pinned AVM SHA-256 `cbc741b34cc55af162d99e44273a6bb01a2f6152cb10ee5884248ceef33d0427`;
- Q-context-0 now has complete luma TXS0--TXS4 and UV coefficient-level CDF
  banks, geometry-based coefficient admission, the ordinary intra primary
  transform table, the observed 32x32 chroma coefficient path, and inverse
  cross-chroma-transform integration. This advances Cuckoo Q80 through its
  complete 26-leaf hosted luma walk and Silene Q80 through its first chroma
  leaf, but neither stream completes in the production decoder;
- the reviewed census now classifies every named feature value observed in all
  21 artifacts as supported. This is feature coverage, not end-to-end decode
  coverage: coefficient-profile, entropy-context, traversal, and reconstruction
  frontiers kept 16 files fail-closed at that checkpoint;
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
- derived CfL reads the current luma footprint from reconstructed coded storage
  at cropped frame edges while retaining visible-edge clamping for neighbours.
  This advances Schwaebisch Q208 through its bottom TX_32X4 U/V block and
  localizes its later prefix exhaustion to an earlier chroma partition drift;
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
- Schwaebisch Q208 completes byte-identically after correcting TX_8X16 entropy
  context, directional and Smooth prediction, frame-edge coefficient contexts,
  partial-boundary partition signaling, MRL index 3, and cropped derived-CfL.
  The final 173,611-byte I420 has SHA-256
  `6fbc5758a05d727ed05b04542de796cc72ca5dd9c402213c3e2d4ec72ae78428`;
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
  reconstructions, while Louis Q224 produced a complete frame at that checkpoint; and
- the nine reached inverse-transform gaps for Chalk Q128, Chimp Q192,
  Chinnathurai Q112, Cuckoo Q80, Flycatcher Q48, both Laarim streams, Pastell
  Q160, and Schwaebisch Q112 are implemented with hash-locked normative 4x4
  and 8x8 IST kernels. None now stops in inverse transform. Chimp Q192 and
  Laarim Q96 expose later access violations; the other streams complete or
  stop in coefficient-prefix, chroma-parse, or prediction paths; and
- the five upstream state-divergence streams now fail closed at later
  reconstruction surfaces: Emerald Q128 at tell 3,896, Pastell Q64 at 32,105,
  Silene Q176 at 8,501, Louis Q144 at 8,787, and Silene Q80 at 99,039; and
- the remaining artifacts retain their earlier Q-profile, large-transform
  coefficient, prediction, or reconstruction blockers.

Chimp Q64, both Chinnathurai artifacts, Cuckoo Q160, Emerald Q208, and
Schwaebisch Q208 currently complete end-to-end and are byte-identical to their
pinned AVM outputs.

The ordinary 22-leaf/tell-762 fixture is not part of this artifact directory.
Frame-wide decode and filter integration is now demonstrated for Emerald,
Chinnathurai, Louis, and Schwaebisch, but general artifact coverage remains
Milestone 8/9 work.

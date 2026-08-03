# Decoder status snapshot

Runtime snapshot verified on 2026-08-02 from a fresh rebuild of the current
worktree. The production Windows decoder was run against all 21 artifacts, and
every emitted I420 was compared by SHA-256 to its locked AVM reference. The
detailed incomplete-file endpoints below are diagnostic frontiers, not a claim
that every hosted fixture passes.

The public decoder emits I420 for all 21 `.obu` files in this directory, and
all 21 are byte-identical to their locked AVM references.
No artifact crashes. `Tell` counts bits from
the beginning of the tile entropy stream.
Block positions are expressed in 4x4 units.

The [artifact feature exposure map](blocker-map.md) complements this production
snapshot with complete-frame AVM accounting counts for luma/chroma partition,
mode, coefficient-prefix, coefficient-level, transform, and frame-filter
surfaces.

## Current production blockers

These groups name the surface where the production decoder currently stops;
an earlier entropy divergence can still be the underlying cause.

| Current stopping surface | Files |
| --- | --- |
| Luma coefficient prefix | None |
| Luma coefficient levels | None |
| Luma prediction | None |
| Luma partition or geometry | None |
| Luma IntraBC source bounds | None |
| Luma IntraBC reference availability | None |
| Chroma IntraBC reference availability | None |
| Luma inverse transform | None |
| Chroma coefficient levels | None |
| Chroma parse | None |
| Chroma deblock map | None |
| Frame filter | None |
| Completeness | None |
| Output parity | None |

| File | Current endpoint | Blocker |
| --- | --- | --- |
| `chalk-320x240-qp128.obu` | **Complete:** zero-angle D203 8x16 IBP prediction now follows the mapped IDIF path. The emitted 115,200-byte I420 is byte-identical to AVM (SHA-256 `4ba52509cafcc1442db604efbbb30c98f6a79bb3441f6b2803559d0d1399cddf`) | None for the accepted profile |
| `chimp-240x287-qp192.obu` | **Complete:** zero-angle D203 16x16 and D45 16x8 prediction use the normative directional path. The emitted 103,440-byte I420 is byte-identical to AVM (SHA-256 `3d5963929ce8856eb8fb3f620fe7e9c8ab9aa7c7033c690696328825acd2868c`) | None for the accepted profile |
| `chimp-93x111-qp064.obu` | **Complete:** chroma CDEF now forces direction zero when primary strength is zero, matching AVM. The emitted I420 is byte-identical to the locked reference (SHA-256 `033f9a28603d8e324c51fc7a6f0593876dde613759a7593ff3b4059a5d7ea936`) | None for the accepted profile |
| `chinnathurai-128x128-qp112.obu` | **Complete:** emitted I420 is byte-identical to the AVM reference (SHA-256 `d1c394ab05667a33bd5974a22ceb3eb94267b36990fe64ee12ba2a3c7e51122a`) | None for the accepted profile |
| `chinnathurai-333x333-qp224.obu` | **Complete:** emitted 166,667-byte I420 is byte-identical to the AVM reference (SHA-256 `6f1868d8945e87350414baa1a49829b21dbcb8c4e0e44dee6633d844ef5d7182`) | None for the accepted profile |
| `cuckoo-157x105-qp160.obu` | **Complete:** emitted I420 is byte-identical to the AVM reference (SHA-256 `01fa3b4d41665f9297edc5ad116a65618dac3069adfb6a83f83f7e05aa5010a`) | None for the accepted profile |
| `cuckoo-431x287-qp080.obu` | **Complete:** zero-angle D45 16x8 and D203 16x16 prediction use the normative directional path. The emitted 185,905-byte I420 is byte-identical to AVM (SHA-256 `7318cb9e85ddca039f6ab09b0a96a69ee35fb2f41cc9797df4cf3c43f0d43227`) | None for the accepted profile |
| `emerald-192x96-qp208.obu` | **Complete:** 20 luma and 11 chroma leaves, tile done at tell 731, filtered I420 byte-identical to the AVM reference (SHA-256 `403c2e7245ec484984e60c1eaf8bd1b47487c0ba6b60afc916b9532025fc51ad`) | None for the accepted profile |
| `emerald-480x240-qp128.obu` | **Complete:** direction-1 MHCCP reuses AVM's shared top-left padded-grid sample, and cropped derived-CfL edge references repeat at whole-block boundaries. The emitted I420 is byte-identical to AVM (SHA-256 `0fa9483b9e5472aeff5423fd7340da5ccf3606a14f991a7ed9836425fbc499e0`) | None for the accepted profile |
| `flycatcher-223x149-qp176.obu` | **Complete:** out-of-frame bottom-left mode neighbors now use AVM's absent/default context instead of a stale directional line context. The emitted 50,027-byte I420 is byte-identical to AVM (SHA-256 `38abac499fc98cb79b4c1e6cf166be2c905fb549771d8b51b3161a926b53c3f2`) | None for the accepted profile |
| `flycatcher-397x265-qp048.obu` | **Complete:** coded MI-grid alignment preserves the required edge leaves, and the emitted 158,139-byte I420 is byte-identical to AVM (SHA-256 `9250e697e5cd416444b0f58fd1563dd5ff987e0bba2e97c031d0cdc1c9c4198c`) | None for the accepted profile |
| `laarim-256x171-qp096.obu` | **Complete:** the emitted I420 is byte-identical to AVM (SHA-256 `ec616970ad13be971eb9a9044ed6d9eeefe94d464830313d458fea65039dd2c7`) | None for the accepted profile |
| `laarim-448x299-qp192.obu` | **Complete:** coded-height MHCCP reconstruction and whole-block derived-CfL edge repetition produce a byte-identical 201,152-byte I420 (SHA-256 `028f7433400429949e73b13828b456663d00b583bba13495db56bbf5b7e011e2`) | None for the accepted profile |
| `louis-301x222-qp144.obu` | **Complete:** MHCCP fitting excludes the duplicated first left reference sample, and MHCCP right-edge chroma padding is extended before dependent blocks and filters consume it. The emitted 100,344-byte I420 is byte-identical to AVM (SHA-256 `ddefd8d1fe4fe69a6c70f05e45d73d65ab513a359cb8f90b4156120add57d00e`) | None for the accepted profile |
| `louis-463x342-qp224.obu` | **Complete:** out-of-frame bottom-left mode neighbors use AVM's absent/default context. The emitted 237,690-byte I420 is byte-identical to AVM (SHA-256 `cbc741b34cc55af162d99e44273a6bb01a2f6152cb10ee5884248ceef33d0427`) | None for the accepted profile |
| `pastell-320x213-qp064.obu` | **Complete:** GDF control rows now use the normative `-8` vertical block origin, so rows 120-127 select the second 128-pixel block flag. The emitted 102,400-byte I420 is byte-identical to AVM (SHA-256 `957eaf1ed0c372123eb1e98ac8d279d35834c6d09d48426acf48380d1a5333fb`) | None for the accepted profile |
| `pastell-512x341-qp160.obu` | **Complete:** MHCCP keeps the full top reference window for small superblock-boundary blocks and caps only at the 62-sample interior limit. The emitted 262,144-byte I420 is byte-identical to AVM (SHA-256 `7277ababbf237deffb0e28802bdf1148a8ab379d4c4bde31cecf3ac371a519a8`) | None for the accepted profile |
| `schwaebisch-353x234-qp112.obu` | **Complete:** coded-height MHCCP fitting and post-readiness exclusion of the final no-left outer margin reproduce AVM's padded reference window. The emitted 124,020-byte I420 is byte-identical to AVM (SHA-256 `3d99d715fc4c6540a8c53c6f5d8f8fd6cc03dc11f7b38d782a09a47e13a03038`) | None for the accepted profile |
| `schwaebisch-417x277-qp208.obu` | **Complete:** stale out-of-frame bottom-left mode context, partial-boundary extension signaling, cropped derived-CfL, and directional/Smooth edge profiles are fixed; local and AVM entropy match through the final logged luma endpoint at tell/range `4360/51200`, and the emitted 173,611-byte I420 is byte-identical to AVM (SHA-256 `6fbc5758a05d727ed05b04542de796cc72ca5dd9c402213c3e2d4ec72ae78428`) | None for the accepted profile |
| `silene-384x307-qp080.obu` | **Complete:** the apparent IntraBC source-bounds failure was downstream of an out-of-frame bottom-left luma mode context. Using AVM's absent/default context restores the normative D113 mode and the emitted 177,024-byte I420 is byte-identical to AVM (SHA-256 `613afd94352e93f2d2a6a343bcb77c2231bb693e38995f3bfef1e7c9d7eb2b1d`) | None for the accepted profile |
| `silene-479x383-qp176.obu` | **Complete:** MHCCP uses AVM's padded top/left luma references, and 16x8 IntraBC morph prediction centers its decimated top template. The emitted 275,617-byte I420 is byte-identical to AVM (SHA-256 `1872688a3cc73f622d752d265724c8118b1d0b310934be39bbff7fcbc4cb66d1`) | None for the accepted profile |

Summary and implementation history:

The first bullet describes the current production snapshot. Every later bullet
preserves the checkpoint at which a feature was implemented; present-tense
wording inside those historical bullets is not a current blocker report.

- the public Windows decoder emits output for all 21 artifacts, all 21 are
  byte-identical to AVM, and no stream fails
  closed and none crashes;
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

At that historical checkpoint, Chimp Q64, both Chinnathurai artifacts, Cuckoo
Q160, Emerald Q208, and Schwaebisch Q208 completed end-to-end and were
byte-identical to their pinned AVM outputs. The ordinary 22-leaf/tell-762
fixture is not part of this artifact directory. Current corpus results are
listed in the table above.

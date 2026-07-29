# Decoder status snapshot

Snapshot verified on 2026-07-29 at base revision `c06486d`, including the
current uncommitted MRL indices 2 and 3, complete CfL prediction, and
multi-line MRL work, plus complete luma FSC, primary IDTX, and primary ADST_DCT
reconstruction. The production Windows decoder was rebuilt and run against
all 21 artifacts, and the pinned artifact profile frontier suite passed.

The public decoder completes 1 of the 21 `.obu` files in this directory.
`emerald-192x96-qp208.obu` decodes to an I420 file that is byte-identical to
the locked AVM reference. The other 20 files fail closed. `Tell` counts bits
from the beginning of the tile entropy stream. Block positions are expressed
in 4x4 units.

| File | Current endpoint | Blocker |
| --- | --- | --- |
| `chalk-320x240-qp128.obu` | `(0,0)` 8x16, tell 32 | Unpinned Q128 8x16 coefficient-level profile after exact CDEF, CCSO, mode, EOB, and secondary-transform syntax |
| `chimp-240x287-qp192.obu` | Superblock 14, 271 luma and 16 chroma leaves; `(56,38)` 4x8 Vertical/-3 MRL-3 multi-line predictor/output match exactly (`8cf672b166c4e9ab`); next `(56,39)` starts at tell 8033/range 39832 | Q192 4x8 transform/coefficient profile |
| `chimp-93x111-qp064.obu` | `(0,0)` 8x8 frontier, tell 30 | Remaining Q64 8x8 luma/coefficient profile |
| `chinnathurai-128x128-qp112.obu` | 32x64 EOB 22, implicit DCT_DCT, and secondary symbol 0 complete, tell 19 | Q112 32x64 coefficient-level profile |
| `chinnathurai-333x333-qp224.obu` | Exact AVM entropy parity through `(32,58)` 16x64 coefficient entry: tell 611, range 34904, EOB 15; `(32,54)` EOB 2 levels/signs end at tell 599, range 33656 | Q224 16x64 EOB-15 coefficient-level profile |
| `cuckoo-157x105-qp160.obu` | First 16x32 DCT_DCT block reconstructed sample-exactly and committed at tell 35; next `(0,4)` 8x16 leaf reached at tell 41 before mode syntax | Q160 8x16 luma-profile admission |
| `cuckoo-431x287-qp080.obu` | 64x64 EOB 87 and implicit DCT_DCT complete, tell 23 | Q80 64x64 coefficient-level profile |
| `emerald-192x96-qp208.obu` | **Complete:** 20 luma and 11 chroma leaves, tile done at tell 731, filtered I420 byte-identical to the AVM reference | None for the accepted profile |
| `emerald-480x240-qp128.obu` | First 64x64 luma predictor/output match AVM (`d4ea93e1129d09a3`/`2bc3d4c50c228831`); first 32x32 U/V blocks match through tell 197, range 53184, with prefilter hashes `57a7c38258f413ff`/`15846509fdb0b4f1` | Next luma/chroma traversal profile after the first exact leaf pair |
| `flycatcher-223x149-qp176.obu` | 32x64 EOB 8, implicit DCT_DCT, and secondary symbol 0 complete, tell 20 | Q176 32x64 coefficient-level profile |
| `flycatcher-397x265-qp048.obu` | EOB 45 and DCT_DCT selected, tell 25 | Q48 coefficient-level profile |
| `laarim-256x171-qp096.obu` | First 32x32 D45/DCT_DCT block reconstructed sample-exactly through 126 levels and 82 coefficients at tell 471; next `(0,8)` 16x16 leaf is reached at tell 476 | Q96 16x16 luma-profile admission |
| `laarim-448x299-qp192.obu` | 64x64 EOB 49 and implicit DCT_DCT complete, tell 18 | Q192 64x64 coefficient-level profile |
| `louis-301x222-qp144.obu` | 64x16 EOB 10, short-side symbol 0, and secondary symbol 0 complete, tell 26 | Q144 64x16 coefficient-level profile |
| `louis-463x342-qp224.obu` | One leaf reconstructed; second 16x16 leaf at `(0,8)`, tell 32 | Q224 16x16 luma profile |
| `pastell-320x213-qp064.obu` | DC mode and 8x8 coefficient-prefix entry reached; prefix starts at tell 25 and fails closed at tell 34 | Remaining Q-context-0 8x8 coefficient profile |
| `pastell-512x341-qp160.obu` | First 16x32 DCT_FLIPADST block and second `(0,4)` 16x32 Smooth/DCT_DCT block reconstruct sample-exactly; the second block has levels/signs at tells 81/83 and predictor/output hashes `7b5a4f04146e5eb7`/`4324e4ef5962723d` | Next Q160 luma-profile admission |
| `schwaebisch-353x234-qp112.obu` | EOB 2, DCT_DCT, and secondary symbol 0 complete, tell 32 | Q112 coefficient-level profile |
| `schwaebisch-417x277-qp208.obu` | Decodes through superblock index 31; frame-edge coefficient contexts match AVM at `(64,16)` through tell 4186/range 48752, eliminating the false `(68,40)` coefficient tuple; `(64,48)` D203/+3 MRL-3 reconstructs and `(66,48)` reaches tell 4236 | Q208 32x8 D203/0 reconstruction admission |
| `silene-384x307-qp080.obu` | 64x64 EOB 46 and implicit DCT_DCT complete, tell 20 | Q80 64x64 coefficient-level profile |
| `silene-479x383-qp176.obu` | First 64x64 luma predictor/output match AVM (`cae2b824a5adc383`/`6f9be39143f7d24f`); explicit-CfL U/V predictors and first 32x32 prefilters match through tell 71, range 53320 (`0e163de1147deb8a`/`a28b4981853199a6`, `3e5af7d08452fb90`/`8e29d20ab3284a74`) | Next luma/chroma traversal profile after the first exact leaf pair |

Summary:

- the public Windows decoder completes 1/21 artifacts; 19 unsupported streams
  report `integer overflow`, Chimp Q192 reports `invalid argument`, and none
  produces accepted output;
- Emerald Q208 is the first complete artifact: all locked luma/chroma handoffs
  reach tile completion at tell 731, frame filtering completes, and CTest
  verifies the emitted I420 byte-for-byte against the AVM reference;
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
- Chinnathurai Q224 matches AVM through `(32,54)` coefficient levels and signs
  at tell 599/range 33656, starts the following leaf at tell 600, and reaches
  its EOB-15 coefficient entry at tell 611/range 34904; the previously
  reported one-bit difference compared unlike syntax boundaries and is not a
  decoder/AVM discrepancy;
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
  leaves, where the production decoder fails closed with `invalid argument`.
  This work adds the required FSC 8x4 path, exact Q3 coefficient CDF rows,
  pinned primary-transform rows, IST combinations, directional and multi-line
  predictors, and inverse transform admissions while retaining fail-closed
  tuple checks;
- the Q64 first-block frontiers advance to tells 30 and 34 for Chimp and
  Pastell respectively;
- Laarim Q96, Cuckoo Q160, Pastell Q160, and Louis Q224 retain their locked
  sample-exact partial reconstructions; and
- the remaining artifacts retain their earlier Q-profile, large-transform
  coefficient, prediction, or inverse-reconstruction blockers.

The ordinary 22-leaf/tell-762 fixture is not part of this artifact directory.
Frame-wide decode and filter integration is now demonstrated for Emerald, but
general artifact coverage remains Milestone 8/9 work.

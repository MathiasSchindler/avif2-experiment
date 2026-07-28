# Decoder status snapshot

Snapshot from 2026-07-28 at base revision `dce27a6`, including the current
uncommitted entropy and reconstruction profile work.

The public decoder completes 1 of the 21 `.obu` files in this directory.
`emerald-192x96-qp208.obu` decodes to an I420 file that is byte-identical to
the locked AVM reference. The other 20 files fail closed. `Tell` counts bits
from the beginning of the tile entropy stream. Block positions are expressed
in 4x4 units.

| File | Current endpoint | Blocker |
| --- | --- | --- |
| `chalk-320x240-qp128.obu` | `(0,0)` 8x16, tell 32 | Unpinned Q128 8x16 coefficient-level profile after exact CDEF, CCSO, mode, EOB, and secondary-transform syntax |
| `chimp-240x287-qp192.obu` | `(0,0)` 8x16, tell 15 | Q192 8x16 luma profile |
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
| `schwaebisch-417x277-qp208.obu` | Decodes through superblock index 31; frame-edge coefficient contexts match AVM at `(64,16)` through tell 4186/range 48752, eliminating the false `(68,40)` coefficient tuple; `(64,48)` reaches tell 4232 | Q208 32x8 D203/+3 MRL-3 reconstruction profile |
| `silene-384x307-qp080.obu` | 64x64 EOB 46 and implicit DCT_DCT complete, tell 20 | Q80 64x64 coefficient-level profile |
| `silene-479x383-qp176.obu` | First 64x64 luma predictor/output match AVM (`cae2b824a5adc383`/`6f9be39143f7d24f`); explicit-CfL U/V predictors and first 32x32 prefilters match through tell 71, range 53320 (`0e163de1147deb8a`/`a28b4981853199a6`, `3e5af7d08452fb90`/`8e29d20ab3284a74`) | Next luma/chroma traversal profile after the first exact leaf pair |

Summary:

- the public Windows decoder completes 1/21 artifacts; all 20 unsupported
  streams currently report `integer overflow` and produce no accepted output;
- Emerald Q208 is the first complete artifact: all locked luma/chroma handoffs
  reach tile completion at tell 731, frame filtering completes, and CTest
  verifies the emitted I420 byte-for-byte against the AVM reference;
- Chinnathurai Q224 matches AVM through `(32,54)` coefficient levels and signs
  at tell 599/range 33656, starts the following leaf at tell 600, and reaches
  its EOB-15 coefficient entry at tell 611/range 34904; the previously
  reported one-bit difference compared unlike syntax boundaries and is not a
  decoder/AVM discrepancy;
- Emerald Q128 and Silene Q176 now match AVM through their first 64x64 luma
  and 32x32 U/V reconstruction surfaces. Emerald reaches tell 197/range 53184;
  Silene reaches tell 71/range 53320 with exact plane-specific explicit-CfL
  predictors. Both still fail closed before completing their next profile;
- Schwaebisch Q208 advances through superblock index 31 and tell 4232 after
  correcting TX_8X16 entropy context, admitting three exact predictor
  geometries, and clipping coefficient contexts at frame edges; its next gap
  is the `(64,48)` 32x8 D203/+3 MRL-3 reconstruction profile;
- the Q64 first-block frontiers advance to tells 30 and 34 for Chimp and
  Pastell respectively;
- Laarim Q96, Cuckoo Q160, Pastell Q160, and Louis Q224 retain their locked
  sample-exact partial reconstructions; and
- the remaining artifacts retain their earlier Q-profile, large-transform
  coefficient, prediction, or inverse-reconstruction blockers.

The ordinary 22-leaf/tell-762 fixture is not part of this artifact directory.
Frame-wide decode and filter integration is now demonstrated for Emerald, but
general artifact coverage remains Milestone 8/9 work.

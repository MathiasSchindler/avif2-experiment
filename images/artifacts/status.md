# Decoder status snapshot

Snapshot from 2026-07-24 at base revision `e2493ca`, including the current
uncommitted Emerald, Laarim, and Q160 reconstruction fleet changes.

No `.obu` file in this directory is decoded completely. `Tell` counts bits
from the beginning of the tile entropy stream. Block positions are expressed
in 4x4 units.

| File | Current endpoint | Blocker |
| --- | --- | --- |
| `chalk-320x240.obu` | `(0,0)` 8x16, tell 32 | Unpinned Q128 8x16 coefficient-level profile after exact CDEF, CCSO, mode, EOB, and secondary-transform syntax |
| `chimp-240x287-qp192.obu` | `(0,0)` 8x16, tell 15 | Q192 8x16 luma profile |
| `chimp-93x111-qp064.obu` | `(0,0)` 8x8, tell 16 | Q64 8x8 luma profile |
| `chinnathurai-128x128-qp112.obu` | 32x64 EOB 22, implicit DCT_DCT, and secondary symbol 0 complete, tell 19 | Q112 32x64 coefficient-level profile |
| `chinnathurai-333x333-qp224.obu` | 64x64 EOB 1 and implicit DCT_DCT complete, tell 9 | Q224 64x64 coefficient-level profile |
| `cuckoo-157x105-qp160.obu` | First 16x32 DCT_DCT block reconstructed sample-exactly and committed at tell 35; next `(0,4)` 8x16 leaf reached at tell 41 before mode syntax | Q160 8x16 luma-profile admission |
| `cuckoo-431x287-qp080.obu` | 64x64 EOB 87 and implicit DCT_DCT complete, tell 23 | Q80 64x64 coefficient-level profile |
| `emerald-192x96-qp208.obu` | Five leaves reconstructed sample-exactly through `(10,4)` 16x16 Smooth: EOB 4, ADST_ADST, STX 0, and three coefficients end at tell 72; next `(14,0)` 32x8 leaf is reached at tell 74 | Next Q208 32x8 luma-profile admission |
| `emerald-480x240-qp128.obu` | 64x64 EOB 27 and implicit DCT_DCT complete, tell 23 | Adjusted-32x32 secondary-transform entropy/range-state parity |
| `flycatcher-223x149-qp176.obu` | 32x64 EOB 8, implicit DCT_DCT, and secondary symbol 0 complete, tell 20 | Q176 32x64 coefficient-level profile |
| `flycatcher-397x265-qp048.obu` | EOB 45 and DCT_DCT selected, tell 25 | Q48 coefficient-level profile |
| `laarim-256x171-qp096.obu` | First 32x32 D45/DCT_DCT block reconstructed sample-exactly through 126 levels and 82 coefficients at tell 471; next `(0,8)` 16x16 leaf is reached at tell 476 | Q96 16x16 luma-profile admission |
| `laarim-448x299-qp192.obu` | 64x64 EOB 49 and implicit DCT_DCT complete, tell 18 | Q192 64x64 coefficient-level profile |
| `louis-301x222-qp144.obu` | 64x16 EOB 10, short-side symbol 0, and secondary symbol 0 complete, tell 26 | Q144 64x16 coefficient-level profile |
| `louis-463x342-qp224.obu` | One leaf reconstructed; second 16x16 leaf at `(0,8)`, tell 32 | Q224 16x16 luma profile |
| `pastell-320x213-qp064.obu` | DC mode complete, tell 25 | Q-context-0 8x8 coefficient-prefix geometry |
| `pastell-512x341-qp160.obu` | First 16x32 DCT_FLIPADST block and second `(0,4)` 16x32 Smooth/DCT_DCT block reconstruct sample-exactly; the second block has levels/signs at tells 81/83 and predictor/output hashes `7b5a4f04146e5eb7`/`4324e4ef5962723d` | Next Q160 luma-profile admission |
| `schwaebisch-353x234-qp112.obu` | EOB 2, DCT_DCT, and secondary symbol 0 complete, tell 32 | Q112 coefficient-level profile |
| `schwaebisch-417x277-qp208.obu` | First 64x16 all-zero DC block and second `(4,0)` 32x16 DC/DCT_DCT block reconstruct sample-exactly; the second block has levels/signs at tells 42/43 and predictor/output hashes `a5db4e9a973f1b83`/`3304535c9bf698c3` | Next Q208 luma-profile admission |
| `silene-384x307-qp080.obu` | 64x64 EOB 46 and implicit DCT_DCT complete, tell 20 | Q80 64x64 coefficient-level profile |
| `silene-479x383-qp176.obu` | 64x64 EOB 3 and implicit DCT_DCT complete, tell 13 | Adjusted-32x32 secondary-transform entropy/range-state parity |

Summary:

- no stream stops directly at first-unit CCSO, FSC sentinel context 6, or the
  reached Palette=0 syntax anymore;
- 32x64, 64x64, and 64x16 now derive their normative TX IDs, adjusted
  coefficient geometries, EOB capacities, and entropy contexts; 32x64/64x64
  use implicit DCT_DCT while 64x16 reads its short-side transform symbol;
- seven reach large-transform coefficient-level profiles after exact EOB and
  transform syntax;
- Laarim Q96 decodes its complete 32x32 D45/DCT_DCT block: 126 levels end at
  tell 314, 82 signed/high-range coefficients end at tell 471, predictor and
  prefilter hashes are `75290844cee19be7` and `51b573f06c63948a`, and the
  next 16x16 leaf is reached at tell 476;
- cuckoo Q160 and pastell Q160 reconstruct and commit their first 16x32
  blocks sample-exactly before failing closed at `(0,4)` tells 41 and 78;
- emerald Q208 now reconstructs five leaves. The `(10,4)` Smooth block uses
  primary symbol 1 -> ADST_ADST, STX symbol 0, coefficients
  `+2@0,+1@16,-1@32`, and a complete sample hash `7959e335` through tell 72;
  the next 32x8 leaf is reached at tell 74;
- schwaebisch Q208 reconstructs its adjusted-geometry 64x16 all-zero block
  sample-exactly and retains the `(4,0)` tell-19 frontier; and
- the other streams retain their earlier Q-profile, coefficient, prediction,
  or inverse-reconstruction blockers.

None reaches the ordinary fixture's now-complete 22-leaf luma path through
tell 762. The current first-block artifact frontiers belong primarily to
Milestone 8 transform/coefficient/reconstruction work. Full frame-wide filter
integration remains Milestone 9 work, and ordinary chroma remains a separate
Milestone 7 frontier.

# Decoder status snapshot

Snapshot from 2026-07-24 at repository revision `8f0a680`, including the
current uncommitted large-transform work.

No `.obu` file in this directory is decoded completely. `Tell` counts bits
from the beginning of the tile entropy stream. Block positions are expressed
in 4x4 units.

| File | Current endpoint | Blocker |
| --- | --- | --- |
| `chalk-320x240.obu` | `(0,0)` 8x16, tell 32 | Unpinned Q128 8x16 coefficient-level profile after exact CDEF, CCSO, mode, EOB, and secondary-transform syntax |
| `chimp-240x287-qp192.obu` | `(0,0)` 8x16, tell 15 | Q192 8x16 luma profile |
| `chimp-93x111-qp064.obu` | `(0,0)` 8x8, tell 16 | Q64 8x8 luma profile |
| `chinnathurai-128x128-qp112.obu` | `(0,0)` 32x64 derived, tell 9 | Large-transform coefficient-prefix range-state parity; rejects before all-zero syntax |
| `chinnathurai-333x333-qp224.obu` | 64x64 EOB 1 and implicit DCT_DCT complete, tell 9 | Q224 64x64 coefficient-level profile |
| `cuckoo-157x105-qp160.obu` | First 16x32 coefficient block complete, tell 35 | 16x32 DCT_DCT inverse reconstruction for DC prediction |
| `cuckoo-431x287-qp080.obu` | `(0,0)` 64x64 derived, tell 9 | Large-transform coefficient-prefix range-state parity; rejects before all-zero syntax |
| `emerald-192x96-qp208.obu` | One leaf reconstructed; second 32x32 leaf at `(0,8)`, tell 32 | 32x32 DC prediction with references |
| `emerald-480x240-qp128.obu` | Vertical +2 mode and 64x64 derivation complete, tell 17 | Large-transform coefficient-prefix range-state parity; rejects before all-zero syntax |
| `flycatcher-223x149-qp176.obu` | 32x64 EOB 8, implicit DCT_DCT, and secondary symbol 0 complete, tell 20 | Q176 32x64 coefficient-level profile |
| `flycatcher-397x265-qp048.obu` | EOB 45 and DCT_DCT selected, tell 25 | Q48 coefficient-level profile |
| `laarim-256x171-qp096.obu` | D45 mode and all-zero flag complete, tell 20 | Extended 32x32 EOB-point symbol 7 |
| `laarim-448x299-qp192.obu` | DC mode and 64x64 derivation complete, tell 5 | Large-transform coefficient-prefix range-state parity; rejects before all-zero syntax |
| `louis-301x222-qp144.obu` | 64x16 EOB 10, short-side symbol 0, and secondary symbol 0 complete, tell 26 | Q144 64x16 coefficient-level profile |
| `louis-463x342-qp224.obu` | One leaf reconstructed; second 16x16 leaf at `(0,8)`, tell 32 | Q224 16x16 luma profile |
| `pastell-320x213-qp064.obu` | DC mode complete, tell 25 | Q-context-0 8x8 coefficient-prefix geometry |
| `pastell-512x341-qp160.obu` | First 16x32 coefficient block complete, tell 65 | DCT_FLIPADST inverse reconstruction |
| `schwaebisch-353x234-qp112.obu` | EOB 2, DCT_DCT, and secondary symbol 0 complete, tell 32 | Q112 coefficient-level profile |
| `schwaebisch-417x277-qp208.obu` | First 64x16 all-zero block complete, tell 17; entropy traversal reaches `(4,0)` 32x16 at tell 19 | 64x16 all-zero DC reconstruction; then Q208 32x16 luma-profile admission |
| `silene-384x307-qp080.obu` | Palette=0 and 64x64 derivation complete, tell 6 | Large-transform coefficient-prefix range-state parity; rejects before all-zero syntax |
| `silene-479x383-qp176.obu` | 64x64 EOB 3 and implicit DCT_DCT complete, tell 13 | Adjusted-32x32 secondary-transform entropy/range-state parity |

Summary:

- no stream stops directly at first-unit CCSO, FSC sentinel context 6, or the
  reached Palette=0 syntax anymore;
- 32x64, 64x64, and 64x16 now derive their normative TX IDs, adjusted
  coefficient geometries, EOB capacities, and entropy contexts; 32x64/64x64
  use implicit DCT_DCT while 64x16 reads its short-side transform symbol;
- five large-transform streams reject before coefficient-prefix entropy
  because their incoming range state is not yet AVM-exact;
- three reach large-transform coefficient-level profiles after exact EOB and
  transform syntax;
- one reaches an exact EOB but stops before unconfirmed adjusted-size
  secondary-transform entropy, and one completes a 64x16 all-zero block
  before reconstruction and the following leaf; and
- the other eleven retain their earlier Q-profile, coefficient, prediction,
  or inverse-reconstruction blockers.

None reaches the ordinary fixture's now-complete 22-leaf luma path through
tell 762. The current first-block artifact frontiers belong primarily to
Milestone 8 transform/coefficient/reconstruction work. Full frame-wide filter
integration remains Milestone 9 work, and ordinary chroma remains a separate
Milestone 7 frontier.

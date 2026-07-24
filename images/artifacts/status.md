# Decoder status snapshot

Snapshot from 2026-07-23 at repository revision `07ae33f`, including the
current uncommitted artifact-frontier work.

No `.obu` file in this directory is decoded completely. `Tell` counts bits
from the beginning of the tile entropy stream. Block positions are expressed
in 4x4 units.

| File | Current endpoint | Blocker |
| --- | --- | --- |
| `chalk-320x240.obu` | `(0,0)` 8x16, tell 32 | Unpinned Q128 8x16 coefficient-level profile after exact CDEF, CCSO, mode, EOB, and secondary-transform syntax |
| `chimp-240x287-qp192.obu` | `(0,0)` 8x16, tell 15 | Q192 8x16 luma profile |
| `chimp-93x111-qp064.obu` | `(0,0)` 8x8, tell 16 | Q64 8x8 luma profile |
| `chinnathurai-128x128-qp112.obu` | `(0,0)` 32x64, tell 9 | 32x64 transform-size derivation |
| `chinnathurai-333x333-qp224.obu` | After Palette=0, tell 7 | 64x64 transform-size derivation |
| `cuckoo-157x105-qp160.obu` | First 16x32 coefficient block complete, tell 35 | 16x32 DCT_DCT inverse reconstruction for DC prediction |
| `cuckoo-431x287-qp080.obu` | `(0,0)` 64x64, tell 9 | 64x64 transform-size derivation |
| `emerald-192x96-qp208.obu` | One leaf reconstructed; second 32x32 leaf at `(0,8)`, tell 32 | 32x32 DC prediction with references |
| `emerald-480x240-qp128.obu` | Vertical +2 mode complete, tell 17 | 64x64 transform-size derivation |
| `flycatcher-223x149-qp176.obu` | DC mode complete, tell 11 | 32x64 transform-size derivation |
| `flycatcher-397x265-qp048.obu` | EOB 45 and DCT_DCT selected, tell 25 | Q48 coefficient-level profile |
| `laarim-256x171-qp096.obu` | D45 mode and all-zero flag complete, tell 20 | Extended 32x32 EOB-point symbol 7 |
| `laarim-448x299-qp192.obu` | DC mode complete, tell 5 | 64x64 transform-size derivation |
| `louis-301x222-qp144.obu` | DC mode complete, tell 16 | 64x16 transform-size derivation |
| `louis-463x342-qp224.obu` | One leaf reconstructed; second 16x16 leaf at `(0,8)`, tell 32 | Q224 16x16 luma profile |
| `pastell-320x213-qp064.obu` | DC mode complete, tell 25 | Q-context-0 8x8 coefficient-prefix geometry |
| `pastell-512x341-qp160.obu` | First 16x32 coefficient block complete, tell 65 | DCT_FLIPADST inverse reconstruction |
| `schwaebisch-353x234-qp112.obu` | EOB 2, DCT_DCT, and secondary symbol 0 complete, tell 32 | Q112 coefficient-level profile |
| `schwaebisch-417x277-qp208.obu` | DC mode complete, tell 9 | 64x16 transform-size derivation |
| `silene-384x307-qp080.obu` | After Palette=0, tell 6 | 64x64 transform-size derivation |
| `silene-479x383-qp176.obu` | After Palette=0, tell 9 | 64x64 transform-size derivation |

Summary:

- no stream stops directly at first-unit CCSO, FSC sentinel context 6, or the
  reached Palette=0 syntax anymore;
- ten streams now reach mode completion and stop at missing large or
  rectangular transform-size derivation;
- five stop at exact Q-dependent EOB or coefficient-level frontiers; and
- the remaining streams stop at earlier Q-profile admission, prediction, or
  inverse-reconstruction paths.

None reaches the ordinary fixture's now-complete 22-leaf luma path through
tell 762. The current artifact frontiers belong to Milestone 8
transform/coefficient/reconstruction work or Milestone 9 frame-filter
integration; ordinary chroma remains a separate Milestone 7 frontier.

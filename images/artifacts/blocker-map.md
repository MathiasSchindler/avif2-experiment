# Decoder blocker map

Generated from the pinned AVM accounting census at revision `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d` and a live production decoder sweep. It describes the complete syntax surfaces each artifact will exercise, not a speculative sequence of future failures.

## Reading the map

- **Current stop** is executable evidence from the production decoder.
- Numeric cells are AVM entropy-event counts over the complete frame. They prove that the artifact exercises that surface beyond the current stop; they do not by themselves prove a missing implementation.
- `-` means the accounting oracle observed no event in that surface.
- Luma/chroma inverse prediction and transform reconstruction are not entropy events. A reconstruction gap is confirmed only when production or a pinned hosted walk reaches it.

## Per-file surface exposure

| File | Current stop | Shared partition | Luma partition | Luma mode/parse | Luma prefix | Luma levels | Luma tx/IST syntax | Chroma partition | Chroma mode/parse | Chroma prefix | Chroma levels | Chroma CCTX | Filter controls |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `chalk-320x240-qp128.obu` | luma reconstruction / inverse transform | 16 | 3498 | 6233 | 2878 | 8340 | 758 | 1702 | 1624 | 4151 | 11791 | 331 | 39 |
| `chimp-240x287-qp192.obu` | luma reconstruction / inverse transform | 14 | 2338 | 3323 | 1918 | 8937 | 537 | 294 | 77 | 83 | 75 | 5 | 6 |
| `chimp-93x111-qp064.obu` | frame filter | 19 | 908 | 2539 | 2039 | 38060 | 321 | 198 | 223 | 698 | 10583 | 91 | 8 |
| `chinnathurai-128x128-qp112.obu` | frame filter | 4 | 748 | 1569 | 1134 | 9544 | 230 | 184 | 179 | 332 | 1285 | 34 | 8 |
| `chinnathurai-333x333-qp224.obu` | complete | 1 | 185 | 376 | 258 | 749 | 82 | 152 | 100 | 92 | 24 | - | 8 |
| `cuckoo-157x105-qp160.obu` | luma reconstruction / inverse transform | 10 | 256 | 493 | 281 | 944 | 107 | 60 | 21 | 34 | 63 | 1 | - |
| `cuckoo-431x287-qp080.obu` | luma reconstruction / inverse transform | 1 | 3047 | 5844 | 3819 | 46238 | 1021 | 1217 | 819 | 2078 | 6789 | 210 | 55 |
| `emerald-192x96-qp208.obu` | complete | 7 | 39 | 61 | 65 | 459 | 24 | 26 | 23 | 32 | 34 | 1 | - |
| `emerald-480x240-qp128.obu` | luma reconstruction / inverse transform | 1 | 4023 | 7555 | 5795 | 111260 | 1458 | 572 | 451 | 780 | 2528 | 89 | 69 |
| `flycatcher-223x149-qp176.obu` | luma reconstruction / inverse transform | 4 | 440 | 584 | 320 | 810 | 85 | 144 | 105 | 135 | 119 | 7 | 3 |
| `flycatcher-397x265-qp048.obu` | luma parse / coefficient prefix | 7 | 6248 | 13081 | 6160 | 75361 | 1226 | 1809 | 1372 | 4301 | 31721 | 480 | 78 |
| `laarim-256x171-qp096.obu` | luma reconstruction / inverse transform | 7 | 1831 | 3365 | 2556 | 30831 | 709 | 749 | 519 | 1358 | 4930 | 126 | 19 |
| `laarim-448x299-qp192.obu` | luma reconstruction / inverse transform | 1 | 739 | 1062 | 732 | 2368 | 238 | 300 | 122 | 163 | 116 | 5 | 4 |
| `louis-301x222-qp144.obu` | luma reconstruction / inverse transform | 7 | 2029 | 3897 | 1938 | 9338 | 433 | 427 | 324 | 412 | 547 | 21 | 26 |
| `louis-463x342-qp224.obu` | chroma parse / partition or mode | 7 | 385 | 633 | 344 | 631 | 85 | 217 | 140 | 124 | 44 | - | 8 |
| `pastell-320x213-qp064.obu` | luma reconstruction / inverse transform | 19 | 5361 | 11369 | 7454 | 69771 | 1846 | 1604 | 1701 | 6552 | 63756 | 769 | 45 |
| `pastell-512x341-qp160.obu` | luma reconstruction / inverse transform | 10 | 3362 | 5339 | 2063 | 4547 | 609 | 2996 | 2339 | 3932 | 4796 | 161 | 12 |
| `schwaebisch-353x234-qp112.obu` | chroma parse / U coefficient prefix | 13 | 4996 | 9678 | 7002 | 81226 | 1736 | 941 | 780 | 1471 | 4832 | 171 | 58 |
| `schwaebisch-417x277-qp208.obu` | luma reconstruction / inverse transform | 7 | 542 | 1153 | 575 | 1650 | 215 | 242 | 91 | 110 | 69 | 5 | 4 |
| `silene-384x307-qp080.obu` | luma parse / unknown | 1 | 3513 | 7616 | 4767 | 68324 | 965 | 1358 | 1098 | 3221 | 20607 | 366 | 49 |
| `silene-479x383-qp176.obu` | luma reconstruction / inverse transform | 1 | 1703 | 2880 | 1156 | 3489 | 331 | 677 | 494 | 597 | 316 | 9 | 24 |

## Confirmed current frontiers

| File | Stage | Leaf | Progress |
|---|---|---|---|
| `chalk-320x240-qp128.obu` | luma reconstruction / inverse transform | (16,16) block=32x64 plane=Y | 875; completed leaves: Y=20 UV=6 |
| `chimp-240x287-qp192.obu` | luma reconstruction / inverse transform | (12,34) block=8x16 plane=Y | 1958; completed leaves: Y=78 UV=2 |
| `chimp-93x111-qp064.obu` | frame filter | (0,0) block=4x4 plane=Y | 950; completed leaves: Y=26 UV=9 |
| `chinnathurai-128x128-qp112.obu` | frame filter | (0,0) block=4x4 plane=Y | 4183; completed leaves: Y=87 UV=47 |
| `chinnathurai-333x333-qp224.obu` | complete | - | - |
| `cuckoo-157x105-qp160.obu` | luma reconstruction / inverse transform | (8,17) block=8x32 plane=Y | 629; completed leaves: Y=26 UV=2 |
| `cuckoo-431x287-qp080.obu` | luma reconstruction / inverse transform | (0,76) block=16x64 plane=Y | 996; completed leaves: Y=19 UV=4 |
| `emerald-192x96-qp208.obu` | complete | - | - |
| `emerald-480x240-qp128.obu` | luma reconstruction / inverse transform | (8,64) block=64x32 plane=Y | 641; completed leaves: Y=5 UV=4 |
| `flycatcher-223x149-qp176.obu` | luma reconstruction / inverse transform | (8,36) block=16x32 plane=Y | 437; completed leaves: Y=9 UV=2 |
| `flycatcher-397x265-qp048.obu` | luma parse / coefficient prefix | (12,48) block=64x16 plane=Y | 1587; completed leaves: Y=48 UV=6 |
| `laarim-256x171-qp096.obu` | luma reconstruction / inverse transform | (8,40) block=16x32 plane=Y | 2434; completed leaves: Y=13 UV=14 |
| `laarim-448x299-qp192.obu` | luma reconstruction / inverse transform | (8,72) block=32x8 plane=Y | 800; completed leaves: Y=22 UV=4 |
| `louis-301x222-qp144.obu` | luma reconstruction / inverse transform | (8,64) block=64x32 plane=Y | 955; completed leaves: Y=16 UV=5 |
| `louis-463x342-qp224.obu` | chroma parse / partition or mode | (32,56) block=32x32 plane=U | 1327; completed leaves: Y=78 UV=40 |
| `pastell-320x213-qp064.obu` | luma reconstruction / inverse transform | (48,44) block=16x16 plane=Y | 3066; completed leaves: Y=68 UV=55 |
| `pastell-512x341-qp160.obu` | luma reconstruction / inverse transform | (20,0) block=32x16 plane=Y | 928; completed leaves: Y=27 UV=10 |
| `schwaebisch-353x234-qp112.obu` | chroma parse / U coefficient prefix | (8,72) block=4x32 plane=U | 2520; completed leaves: Y=72 UV=20 |
| `schwaebisch-417x277-qp208.obu` | luma reconstruction / inverse transform | (52,80) block=32x16 plane=Y | 3953; completed leaves: Y=214 UV=32 |
| `silene-384x307-qp080.obu` | luma parse / unknown | (16,72) block=16x16 plane=Y | 553; completed leaves: Y=14 UV=10 |
| `silene-479x383-qp176.obu` | luma reconstruction / inverse transform | (0,72) block=32x32 plane=Y | 335; completed leaves: Y=9 UV=4 |

## What is actually still uncertain

- **Secondary inverse transform:** syntax is visible here, but this AVM revision's accounting value does not expose the packed secondary set and type. Exact missing tuples remain reach-and-pin work.
- **Luma prediction:** all current corpus prediction frontiers clear, but tuple admission is intentionally narrow; later oracle-observed geometries are exposure, not proof of support.
- **Coefficient and parse stages:** complete-frame event counts show where each file will return after earlier fixes. Adaptive CDF state means a future failure cannot be ordered safely until preceding blocks decode sample- and range-exactly.
- **Frame filtering:** controls show which tools are signaled. Only the two current frame-filter stops are confirmed missing production paths; a nonzero control count alone is not a blocker.

Regenerate after building the accounting census and production decoder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  tools/census/generate_blocker_map.ps1
```

Pass `-RefreshCurrent` to rerun the production decoder instead of consuming the verified `build/*-continued-final.stderr` sweep.

# Artifact feature exposure map

Generated from the pinned AVM accounting census at revision `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`. It describes entropy surfaces observed over each complete AVM-decoded frame. It is useful for corpus selection and coverage planning; it is not a decoder status or missing-feature report. See [`status.md`](status.md) for current production results.

## Reading the map

- Numeric cells are AVM entropy-event counts over the complete frame. They prove that the artifact exercises that surface; they do not by themselves prove production support or a missing implementation.
- `-` means the accounting oracle observed no event in that surface.
- Luma/chroma inverse prediction and transform reconstruction are not entropy events. A reconstruction gap is confirmed only when production or a pinned hosted walk reaches it.

## Per-file surface exposure

| File | Shared partition | Luma partition | Luma mode/parse | Luma prefix | Luma levels | Luma tx/IST syntax | Chroma partition | Chroma mode/parse | Chroma prefix | Chroma levels | Chroma CCTX | Filter controls |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `chalk-320x240-qp128.obu` | 16 | 3498 | 6233 | 2878 | 8340 | 758 | 1702 | 1624 | 4151 | 11791 | 331 | 39 |
| `chimp-240x287-qp192.obu` | 14 | 2338 | 3323 | 1918 | 8937 | 537 | 294 | 77 | 83 | 75 | 5 | 6 |
| `chimp-93x111-qp064.obu` | 19 | 908 | 2539 | 2039 | 38060 | 321 | 198 | 223 | 698 | 10583 | 91 | 8 |
| `chinnathurai-128x128-qp112.obu` | 4 | 748 | 1569 | 1134 | 9544 | 230 | 184 | 179 | 332 | 1285 | 34 | 8 |
| `chinnathurai-333x333-qp224.obu` | 1 | 185 | 376 | 258 | 749 | 82 | 152 | 100 | 92 | 24 | - | 8 |
| `cuckoo-157x105-qp160.obu` | 10 | 256 | 493 | 281 | 944 | 107 | 60 | 21 | 34 | 63 | 1 | - |
| `cuckoo-431x287-qp080.obu` | 1 | 3047 | 5844 | 3819 | 46238 | 1021 | 1217 | 819 | 2078 | 6789 | 210 | 55 |
| `emerald-192x96-qp208.obu` | 7 | 39 | 61 | 65 | 459 | 24 | 26 | 23 | 32 | 34 | 1 | - |
| `emerald-480x240-qp128.obu` | 1 | 4023 | 7555 | 5795 | 111260 | 1458 | 572 | 451 | 780 | 2528 | 89 | 69 |
| `flycatcher-223x149-qp176.obu` | 4 | 440 | 584 | 320 | 810 | 85 | 144 | 105 | 135 | 119 | 7 | 3 |
| `flycatcher-397x265-qp048.obu` | 7 | 6248 | 13081 | 6160 | 75361 | 1226 | 1809 | 1372 | 4301 | 31721 | 480 | 78 |
| `laarim-256x171-qp096.obu` | 7 | 1831 | 3365 | 2556 | 30831 | 709 | 749 | 519 | 1358 | 4930 | 126 | 19 |
| `laarim-448x299-qp192.obu` | 1 | 739 | 1062 | 732 | 2368 | 238 | 300 | 122 | 163 | 116 | 5 | 4 |
| `louis-301x222-qp144.obu` | 7 | 2029 | 3897 | 1938 | 9338 | 433 | 427 | 324 | 412 | 547 | 21 | 26 |
| `louis-463x342-qp224.obu` | 7 | 385 | 633 | 344 | 631 | 85 | 217 | 140 | 124 | 44 | - | 8 |
| `pastell-320x213-qp064.obu` | 19 | 5361 | 11369 | 7454 | 69771 | 1846 | 1604 | 1701 | 6552 | 63756 | 769 | 45 |
| `pastell-512x341-qp160.obu` | 10 | 3362 | 5339 | 2063 | 4547 | 609 | 2996 | 2339 | 3932 | 4796 | 161 | 12 |
| `schwaebisch-353x234-qp112.obu` | 13 | 4996 | 9678 | 7002 | 81226 | 1736 | 941 | 780 | 1471 | 4832 | 171 | 58 |
| `schwaebisch-417x277-qp208.obu` | 7 | 542 | 1153 | 575 | 1650 | 215 | 242 | 91 | 110 | 69 | 5 | 4 |
| `silene-384x307-qp080.obu` | 1 | 3513 | 7616 | 4767 | 68324 | 965 | 1358 | 1098 | 3221 | 20607 | 366 | 49 |
| `silene-479x383-qp176.obu` | 1 | 1703 | 2880 | 1156 | 3489 | 331 | 677 | 494 | 597 | 316 | 9 | 24 |

## Interpretation limits

- **Secondary inverse transform:** syntax is visible here, but this AVM revision's accounting value does not expose the packed secondary set and type.
- **Prediction and reconstruction:** inverse prediction and transform reconstruction are not entropy events, so their exact mode/geometry coverage needs separate sample-locked evidence.
- **Adaptive syntax:** event counts measure demand, not independent reachability. CDF and reconstructed-reference dependencies still require ordered differential validation.
- **Frame filtering:** control counts show signaled entropy syntax, not which pixels a filter changes or whether output is sample-exact.

Regenerate after building the accounting census:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  tools/census/generate_blocker_map.ps1
```

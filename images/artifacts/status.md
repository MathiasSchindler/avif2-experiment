# Decoder status

Current non-byte-identical results as of 2026-08-03. Files that decode
byte-identically to AVM are intentionally omitted.

| File | Current result |
| --- | --- |
| `chimp-166x199-qp104.obu` | Fails closed: luma prediction (`invalid argument`) |
| `chimp-240x287-qp192.obu` | Fails closed: luma prediction (`unsupported feature`) |
| `chimp-93x111-qp064.obu` | Fails closed: luma prediction (`invalid argument`) |
| `chinnathurai-128x128-qp112.obu` | Fails closed: luma coefficient levels (`unsupported feature`) |
| `chinnathurai-257x257-qp152.obu` | Fails closed: luma prediction (`invalid argument`) |
| `cuckoo-289x193-qp216.obu` | Fails closed: luma prediction (`unsupported feature`) |
| `cuckoo-431x287-qp080.obu` | Fails closed: luma coefficient levels (`unsupported feature`) |
| `emerald-352x176-qp072.obu` | Fails closed: luma prediction (`invalid argument`) |
| `emerald-480x240-qp128.obu` | Fails closed: luma prediction (`unsupported feature`) |
| `flycatcher-311x207-qp136.obu` | Fails closed: luma prediction (`unsupported feature`) |
| `flycatcher-397x265-qp048.obu` | Decodes, but output differs from AVM |
| `laarim-256x171-qp096.obu` | Fails closed: luma prediction (`invalid argument`) |
| `laarim-389x259-qp024.obu` | Fails closed: luma coefficient levels (`unsupported feature`) |
| `laarim-448x299-qp192.obu` | Fails closed: luma prediction (`unsupported feature`) |
| `louis-301x222-qp144.obu` | Fails closed: luma prediction (`invalid argument`) |
| `louis-377x279-qp184.obu` | Decodes, but output differs from AVM |
| `louis-463x342-qp224.obu` | Fails closed: luma prediction (`unsupported feature`) |
| `pastell-431x287-qp232.obu` | Fails closed: luma IntraBC reference (`unsupported feature`) |
| `pastell-512x341-qp160.obu` | Fails closed: luma IntraBC source bounds (`invalid data`) |
| `schwaebisch-353x234-qp112.obu` | Fails closed: luma prediction (`invalid argument`) |
| `silene-384x307-qp080.obu` | Fails closed: luma prediction (`invalid argument`) |
| `silene-425x340-qp120.obu` | Fails closed: luma prediction (`invalid argument`) |
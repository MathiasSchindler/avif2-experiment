# Decoder status

Current non-byte-identical results. Files that decode
byte-identically to AVM are intentionally omitted.

| File | Current result |
| --- | --- |
| `pastell-512x341-qp160.obu` | Fails closed: luma IntraBC source bounds (`invalid data`) |
| `schwaebisch-353x234-qp112.obu` | Decodes, but output differs from AVM |
| `silene-384x307-qp080.obu` | Fails closed after an earlier chroma entropy divergence (`invalid data`) |
| `silene-425x340-qp120.obu` | Decodes, but output differs from AVM |
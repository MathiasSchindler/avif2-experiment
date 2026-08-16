# I420/I444 luma 8x8 AC complete-stream syntax model

Status: promoted to production; private probes remain the syntax oracle.

The test-only driver replaces only luma's scalar DC coefficient writer with
the completed general 8x8 `DCT_DCT` writer. U and V keep the existing
DC-only 4x4/4x8/8x4/8x8/8x16/16x8/16x16 transform syntax.

## Exact tile and state recipe

1. Initialize one tile range writer and one `EncSyntaxCdfs`.
2. Visit superblocks in raster order. For each superblock, emit the complete
   luma partition subtree first. Leaves are depth-first and their 8x8
   transforms are row-major. Then emit that superblock's chroma leaves;
   each leaf is mode syntax, U coefficient syntax, then V coefficient syntax.
3. Every adaptive CDF row persists for the entire tile across transforms,
   leaves, planes, and superblocks. There is no leaf or superblock CDF reset.
4. For every non-DC luma transform, emit primary `DCT_DCT`, then the required
   color-sequence secondary-transform symbol `stx_flag=0`, using the adaptive
   intra 8x8 `default_sec_tx_type_cdf` row. Omitting this symbol corrupts the
   following coefficient payload even though the I400 stream is valid.
5. Luma `cul_level` is the saturated sum of all 64 magnitudes plus DC sign.
   It updates the covered 4x4 above/left entries after each transform. Above
   state persists for the tile. Left state persists across superblock columns
   and resets only when traversal enters the next 64-luma-row superblock band.
6. Chroma has separate U and V neighbor arrays, so luma AC state does not
   alter chroma skip or DC-sign contexts. U is coded and committed first.
   V skip is its own neighbor base context plus 6 exactly when the matching U
   transform has nonzero EOB.
7. Cropped edges still code the full transform selected by the partition
   geometry. Entropy neighbor updates cover only in-grid 4x4 units.
8. All three planes use the frame base-q context:
   `1..90 -> 0`, `91..140 -> 1`, `141..190 -> 2`, `191..255 -> 3`.

Frozen evidence covers 10 streams, five per format, with 206 nonzero-AC luma
transforms, 66 DC-only chroma transforms, and 338 context transcript events.
The matrix includes 8x8, 17x19 odd edges, multileaf, 72x64 multisuperblock,
and prepared texture cases across all four q contexts. All 10 local
reconstructions equal both SHA-256-pinned decoders.

Run:

```sh
make -C experimental/encoder build/ac8x8-color-syntax-driver
python3 experimental/encoder/tools/ac8x8_color_syntax_model.py --check
python3 -m unittest \
  experimental/encoder/tests/test_ac8x8_color_syntax_model.py
```

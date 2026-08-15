# M3 multi-leaf transform and coefficient entropy state

Status: exact executable transcript derived, frozen, and integrated. The later
boundary investigation corrected the 8x8 partition split context; public
production support now includes 17..64.

## Production integration result

An earlier integration attempt on 2026-08-15 kept the public 1..16 bound after pinned
AVM exposed a remaining boundary-class discrepancy. The production prototype
matched the frozen 32x32 transcript and all three reconstructions for seven
qindex probes (1, 90, 91, 100, 141, 191, 255), but pinned `avmdec` rejected
17x17 and the 24- and 56-pixel rounded partition classes while the project
decoder accepted them. The 32, 40, 48, and 64 classes passed the same non-flat
probe. This is not attributed to the known project-decoder overflow: these
were project-generated DC-only streams, and the disagreement is specifically
an AVM tile-data rejection.

The follow-on investigation proved that rectangle and extension signaling
were correct. The mismatch was the terminal 8x8 `do_split` context: private
syntax used row 4 instead of row 0. See
`m3-boundary-partition-transcript.md`. The correction is retained because
both decoders now accept all flat and non-flat rounded classes, but the large
path is still not publicly exposed.

## Scope and evidence

`tools/m3_multileaf_cdf_model.py` combines the verified 17..64 rectangle
partition traversal with the existing DC quantization/reconstruction model,
the encoder's generated AV2 v1.0 CDF subset, and the exact inverse-CDF update
used by both the encoder writer and project decoder. The normative subset file
hash is
`0939be6a2ba6c099070138a14f008e85f635a91d194879d8ab971822cae85b9f`.

The independent decoder control flow establishes the boundaries:

* `src/decoder/tile.c` initializes transform-partition, coefficient-prefix,
  transform-type, and coefficient-level CDFs once when a tile starts.
* `src/decoder/tile_luma.c` reads transform partition syntax once per
  prediction leaf, then visits each selected transform in order.
* `src/decoder/coeff.c` derives the prefix skip and DC-sign contexts from the
  shared coefficient context arrays.
* `src/decoder/coeff_level.c` updates those arrays immediately after each
  transform with `cul_level`.
* `src/decoder/range.c` adapts the selected CDF immediately after each symbol.

## Exact symbol and update order

For each 8x8, 8x16, 16x8, or 16x16 prediction leaf:

1. `tx_do_partition[fsc=0,inter=0,size_group]` is coded. Groups are 1 for
   8x8, 2 for rectangles, and 3 for 16x16.
2. If split, `tx_partition_type[group]` is coded: symbol 1 for 8x16, 2 for
   16x8, and 0 for the four-way 16x16 split.
3. The resulting 8x8 transforms are visited in leaf-local raster order.
4. `txb_skip[qctx,plane=Y,txs=1,skip_context]` is coded (`0` nonzero,
   `1` all zero) and adapted immediately.
5. A nonzero DC-only transform codes `eob_pt_64=0`. Primary DCT_DCT is
   implied for `eob=1`; no transform-type symbol is present.
6. `coeff_base_lf_eob[txs=1,ctx=0]`, optional `coeff_br_lf[ctx=0]`,
   `dc_sign[sign_context]`, and any non-adaptive remainder bits follow.
7. The transform's context value is committed over its two-by-two MI area:
   `min(abs(dc),7)`, plus bit 3 for negative DC or bit 4 for positive DC.

Every adaptive event in the locked JSON contains the exact materialized
inverse CDF before and after the symbol, including count and three rate
parameters. Tests require each later use of a row to begin at the previous
use's exact final state.

## Persistence boundaries

Adaptive CDF state is tile state. It does **not** reset at a transform block,
prediction block/leaf, or superblock position. A row is updated after its
symbol and the updated row is visible to every later transform in the tile.

Coefficient spatial context is separate. It is updated after every transform.
Above context persists across superblock rows; left context is reset or
overwritten by superblock-row traversal. Neither context is reset between
transforms or prediction leaves.

The coefficient CDF bank is selected once from the **frame base qindex**:
`<=90`, `91..140`, `141..190`, or `>=191`. Block delta-q changes
quantization but does not select another coefficient CDF bank. Locked 32x32
vectors at qindex 90, 91, 141, and 191 exercise all four banks.

## Second-transform divergence reproducer

The first 16x16 leaf of the locked 32x32 qindex-100 case has:

```text
tx  x,y   level  txb-skip-ctx  dc-sign-ctx
0   0,0   -46    1             0
1   8,0   +69    3             1
2   0,8   +22    3             1
3   8,8   +58    5             2
```

Transform 1 therefore diverges from any implementation that resets either
state family: its skip/sign contexts include transform 0, and its
`eob_pt_64`, base, BR, and sign rows begin from the adaptive state left by
earlier symbols. This is the minimal locked reproducer for the previously
observed second-transform desynchronization.

## Frozen vectors and validator evidence

`tests/m3_multileaf_cdf_vectors.json` contains complete transcripts for
17x17, 17x32, 32x17, 32x32, 48x64, and 64x64. They contain respectively
9, 12, 12, 16, 48, and 64 transforms. It also contains a flat-128 32x32
all-zero control and the four q-context vectors.

Pinned AVM revision `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`
successfully encoded and decoded all six non-flat sources. Exact OBU hashes
are locked in `tests/m3_multileaf_cdf_avm_probes.json`. Binary hashes are:

* `avmenc`: `830c5f7a82e071560501183fb85ea9e2632ce7665c9033232ac64b546efb7e64`
* `avmdec`: `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`
* project decoder:
  `c0c6329cc85de13c92d04a00deff8124c9b45dbe74da8a86ddc60d9d7b4de9df`

The project decoder currently rejects these AVM-generated non-flat streams at
the first coefficient with `integer overflow (cause 7, stage 2, tell 25)`.
That validator limitation does not affect the transcript derivation: the
project source supplies the independent state/context boundaries, while every
CDF transition is deterministically compared to the encoder's normative
tables and writer update algorithm.

Run:

```sh
python3 -m unittest \
  experimental/encoder/tests/test_m3_multileaf_cdf_model.py
python3 experimental/encoder/tools/m3_multileaf_cdf_model.py \
  --probe experimental/encoder/build/m3-multileaf-probe
```

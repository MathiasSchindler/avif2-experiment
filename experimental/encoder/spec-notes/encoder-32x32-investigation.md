# 32x32 encoder syntax investigation

Status: root cause identified. A production implementation of the
four-16x16-prediction-block recipe is present in the shared tree, but its
current flat-128 stream is rejected by pinned `avmdec` at tile data. It remains
unsupported until focused tests and exact local/AVM/project reconstruction
parity pass.

Run the reproducible case generator and optional last-byte black-box sweep:

```sh
python3 experimental/encoder/tools/encoder_32x32_investigation.py
python3 experimental/encoder/tools/encoder_32x32_investigation.py \
  --probe experimental/encoder/build/encoder-32x32-investigation/one32/flat128.obu
python3 -m unittest \
  experimental/encoder/tests/test_encoder_32x32_investigation.py
```

## Controlled evidence

The disposable generalized prototype produced 32x32 I400 flat-128, flat-96,
and four-quadrant inputs. The one-32x32-prediction-block streams had SHA-256
`49925c7f...e81b63`, `2bedc9d1...2a4ca`, and `9eb2328e...0c82c`.
Pinned `avmdec` rejected all three at tile data. The project decoder diagnosed
the nonzero flat stream at `leaf=(0,0) block=9 ... tx=2 ... eob=25`, i.e. it
had already selected the 32x32 prediction block and diverged while consuming
transform syntax.

The flat-128 stream has a one-byte tile, `3c`. Exhaustively replacing that
byte found several decoder-accepted terminations; notably the one-bit mutation
`3c -> 3e` is accepted by both decoders and reconstructs exactly 1024 samples
of value 128. This localizes the first failure to the tile entropy transcript,
not OBU carriage, dimensions, sequence header, or frame header.

## First syntax divergence

For `BLOCK_32X32` the normative tables select:

* block enum 9;
* `TileTxDoPartitionCdf[..., Size_To_Tx_Part_Group_Lookup[9]]`, group 5;
* when partitioning is selected, `TileTxPartitionTypeCdf[..., 
  Size_To_Tx_Type_Group_Vert_And_Horz[9]]`, group 6.

The removed/generalized prototype signaled `tx_do_partition=1` but emitted no
`tx_partition_type`; it immediately wrote sixteen 8x8 coefficient transcripts.
The decoder therefore consumes the first coefficient decision as transform
partition type. Even after that accidental symbol, a 32x32 prediction block
split with `TX_PARTITION_SPLIT` contains four **16x16 transform blocks**, not
sixteen 8x8 transform blocks. Nonzero streams consequently select the wrong
EOB family, transform-size context, scan, dequant/inverse transform, and
coefficient contexts. This is the root cause.

## Legal block trees

Two useful legal choices are:

1. `BLOCK_32X32`, `PARTITION_NONE`: one 32x32 prediction block; signal
   transform partition and use four 16x16 transform blocks.
2. `BLOCK_32X32`, `PARTITION_SPLIT`: four 16x16 prediction blocks; each uses
   the existing `TX_PARTITION_SPLIT` path to four 8x8 transform blocks.

Choice 2 is the simpler next implementation. It needs one new partition
decision but reuses the validated 8x8 coefficient and inverse-transform path.
Choice 1 requires a complete 16x16 DC coefficient/reconstruction model.

For choice 2, `do_split` uses context
`Partition_Size_Adjust[BLOCK_32X32] * 4 + ctx1*2 + ctx2`; with reset
`BLOCK_256X256` neighbors this is 8. A true split then also requires
`do_square_split=1`; at the top-left frame corner its context is 0 and the
official row is `{18000, 7, 0}`. Omitting this second symbol makes the decoder
continue into rectangular-partition syntax.

## Prediction, contexts, and delta-Q

Prediction is per prediction block, not per transform block. For the selected
four-16x16 design, derive one 16x16 DC predictor from that block's external
top/left reconstructed edges, then code four 8x8 residual transforms against
that same predictor. Do not recompute prediction from internal 8x8 transform
edges.

`clear_above_context()` runs once at tile start and
`clear_left_context()` once per superblock row. They initialize size neighbors
to `BLOCK_256X256` and coefficient/DC contexts to zero. Context arrays are not
reset at prediction-block boundaries; update them after each decoded block or
transform so later 16x16 blocks and 8x8 transforms see their real neighbors.

The current M3 frame has `delta_q_present=0`, so it emits no delta-Q symbol.
When enabled, `decode_tile()` sets `ReadDeltas = delta_q_present` once per
superblock. The first eligible block reads `delta_qindex`, then clears
`ReadDeltas`; delta-Q is therefore per superblock, not per 16x16 prediction
block. `CurrentQIndex` begins at `base_q_idx` at tile start and carries forward.

## Concrete implementation recipe

1. Extend bounded storage/capacities to 32x32 and 16 transform records.
2. At the in-frame 32x32 node, code `do_split=1` with context 8 and
   `do_square_split=1` with context 0; materialize the official square-split
   CDF.
3. Visit four 16x16 prediction blocks in partition order. Code each existing
   16x16 mode and split-transform syntax.
4. Compute prediction once per 16x16 block. Select four 8x8 DC levels against
   that fixed predictor, reconstruct transforms in raster order, and update
   coefficient contexts globally.
5. Keep delta-Q absent for the present frame header. If enabled later, emit it
   only for the first eligible block of each superblock.
6. Gate on flat-128, flat-96, and quadrants matching both decoders exactly;
   retain a one-bit/tile-byte mutation sweep when a failure recurs.

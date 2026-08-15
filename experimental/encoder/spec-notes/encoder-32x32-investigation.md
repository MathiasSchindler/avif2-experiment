# 32x32 encoder syntax investigation

Status: resolved as an invalid partition recipe. The rejected shared-tree
attempt must not be revived: AV2 v1.0 does not permit a coded
`PARTITION_SPLIT` at `BLOCK_32X32`. The verified replacement is the
rectangle-only traversal documented in `m3-large-partitions.md`.

Run the retained controlled-case and mutation tools with:

```sh
python3 experimental/encoder/tools/encoder_32x32_investigation.py
python3 experimental/encoder/tools/encoder_32x32_investigation.py \
  --probe experimental/encoder/build/encoder-32x32-investigation/one32/flat128.obu
python3 -m unittest \
  experimental/encoder/tests/test_encoder_32x32_investigation.py
```

## Two distinct historical failures

The first one-block experiment selected `BLOCK_32X32`,
`PARTITION_NONE`, then signaled `tx_do_partition=1` without emitting
`tx_partition_type`. It immediately wrote sixteen 8x8 coefficient
transcripts. A 32x32 prediction block with `TX_PARTITION_SPLIT` instead has
four 16x16 transform blocks. This shifted the entropy transcript and selected
the wrong EOB, scan, dequantization, inverse-transform, and coefficient
contexts.

The later four-16x16-prediction-block attempt failed for a different reason:
it coded a square split at 32x32. AV2 v1.0's square-split eligibility set is
only 128x128 and 256x256. After `do_split=1`, a 32x32 node proceeds directly
to rectangular syntax.

The legal four-leaf replacement is:

```text
32x32 HORZ
  32x16 VERT -> 16x16, 16x16
  32x16 VERT -> 16x16, 16x16
```

At reset top-left state, the 32x32 split context is 8 and the rectangular-type
context is 12. The next symbol after `do_split=1` is `rect_type=HORZ`; there
is no square-split symbol.

## Retained black-box evidence

The original flat-128 stream had a one-byte tile, `3c`. Replacing it with
`3e` produced a stream accepted by both decoders and reconstructing 1024
samples of value 128. This localized that failure to the entropy transcript,
not dimensions, OBU carriage, sequence header, or frame header. It did not
validate the attempted partition recipe.

Prediction is derived per transform. For each 16x16 leaf, visit its four 8x8
transforms in local raster order; each transform reads the reconstructed
neighbors available at its own coordinates, reconstructs, and updates
coefficient contexts before the next transform. Delta-Q remains once per
superblock: when enabled, only the first eligible block consumes it.

No production syntax helper was added from this investigation. The next
implementation must follow the verified rectangular transcript, preserve
depth-first partition order, and pass both decoders before raising the current
1..16 dimension bound.

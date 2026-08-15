# M3 17x17 boundary coefficient context parity

Status: complete. All 14 frozen probes are exact across the local model,
pinned AVM, and the project decoder after correcting the earlier terminal
8x8 partition context. The corrected path is public for 17..64 encoding.

## Boundary leaves and spatial state

The 17x17 coded canvas is 24x24. Depth-first traversal ends with three edge
prediction leaves after the leading 16x16 leaf:

```text
leaf 1: 8x16 at (16,0), transforms (16,0), (16,8)
leaf 2: 16x8 at (0,16), transforms (0,16), (8,16)
leaf 3: 8x8  at (16,16), transform  (16,16)
```

Each transform derives its context from the two-by-two MI span clipped to the
coded frame. Above/left values are `cul_level = min(abs(dc),7)` with bit 3
for negative DC or bit 4 for positive DC. The update is committed immediately
after each transform. CDF rows remain adaptive tile state and are not reset at
the prediction-leaf boundary.

For a transform split from an 8x16 or 16x8 coding block, skip context is the
normative 5x5 lookup indexed by the OR of neighboring low three bits. For the
unsplit 8x8 coding block it is always zero. DC-sign context is 1 for a
negative neighbor sum, 2 for a positive sum, and 0 for a tie.

The non-flat edge transforms have these `(level, skip, sign, prediction,
reconstruction)` tuples:

| qindex | 8x16 top | 8x16 bottom | 16x8 left | 16x8 right | 8x8 corner |
|---:|---|---|---|---|---|
| 1 | -1223,3,2,160,64 | -506,5,0,136,96 | -404,3,2,96,64 | 301,5,0,136,160 | -814,0,0,128,64 |
| 90 | -93,3,2,160,64 | -39,5,0,136,96 | -31,3,2,96,64 | 23,5,0,136,160 | -62,0,0,128,64 |
| 91 | -90,3,2,160,64 | -38,5,0,136,96 | -30,3,2,96,64 | 23,5,0,136,160 | -60,0,0,128,64 |
| 100 | -70,3,2,160,64 | -29,5,0,136,96 | -22,3,2,95,65 | 17,5,0,137,160 | -46,0,0,128,65 |
| 141 | -21,3,2,160,66 | -9,5,0,136,96 | -7,3,2,97,66 | 5,5,0,136,159 | -14,0,0,128,65 |
| 191 | -5,3,2,166,71 | -2,5,0,138,100 | -1,2,2,90,71 | 1,4,0,138,157 | -3,0,0,129,72 |
| 255 | -1,2,2,128,8 | 0,2,0,128,128 | -1,2,2,128,8 | 0,2,0,128,128 | -1,0,0,128,8 |

Flat-128 produces five zero levels at every qindex. Rectangle transforms use
skip context 1 and the corner 8x8 uses context 0; all sign contexts are zero.

## Coefficient transcript

Every nonzero transform emits and adapts:

```text
txb_skip -> eob_pt_64(0) -> implied DCT_DCT ->
coeff_base_lf_eob -> optional coeff_br_lf -> dc_sign -> remainder literals
```

An all-zero transform emits only `txb_skip=1`; transform type and all
coefficient suffix syntax are implied. The locked JSON records the exact
normative inverse CDF before and after every adaptive event, plus the
prediction, level, reconstruction, context value, and cropped reconstruction
hash.

## Private validator matrix and decision

The private boundary driver exercises the 17..64 syntax without changing
`ENC_M3_MAX_DIMENSION`. Fourteen 17x17 streams cover flat and alternating
non-flat sources at qindex 1, 90, 91, 100, 141, 191, and 255.

All 14/14 are exact: local reconstruction, pinned AVM output, and project
decoder output have the same cropped 17x17 bytes. The pinned `avmdec` SHA-256
is `c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0`.

The previous 11 mismatches were partition-induced range divergence. The final
8x8 `PARTITION_NONE` selected split context 4 instead of normative context 0,
so the coefficient syntax was decoded from an already divergent range state.
After that correction there are zero remaining partition divergences and zero
genuine edge coefficient-context errors. Production stays at 1..16 because
this focused proof does not promote the separate full-image path.

Evidence:

* `tests/m3_edge_coeff_context_vectors.json`
* `tests/m3_edge_coeff_avm_probes.json`
* `tools/m3_edge_coeff_model.py`

Run:

```sh
python3 -m unittest experimental/encoder/tests/test_m3_edge_coeff_model.py
python3 experimental/encoder/tools/m3_edge_coeff_model.py --vectors
python3 experimental/encoder/tools/m3_edge_coeff_model.py \
  --probe experimental/encoder/build/m3-edge-coeff-probes
```

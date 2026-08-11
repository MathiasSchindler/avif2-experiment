# 10-bit implementation WP2 sample storage

## Result

10B-WP2 is complete. Prepared two-byte workspace accounting now binds typed,
bounded U16 reconstruction planes, predictor/reference scratch, filter stages,
and final planar output. Public 10-bit decode reaches the reconstruction owner
and remains deliberately unsupported and nonmutating there.

This package adds storage plumbing only. It does not define the normative
10-bit quantizer, transform, prediction, reconstruction, or filter arithmetic
owned by later packages.

## Typed storage contract

The existing U8 reconstruction and filter structs remain intact. Parallel U16
owners carry `uint16_t *` sample pointers and sample-count strides for:

- coded reconstruction planes;
- predictor and reference scratch;
- deblocked and CDEF stage planes; and
- tightly packed final Y, U, and V output planes.

Coefficient arrays remain signed 32-bit values. Reconstruction readiness maps,
deblock cells, CDEF controls, CCSO controls, GDF controls, syntax state, and CDF
state retain their existing element widths.

The public layout continues to express plane offsets, row strides, workspace
extent, and output extent in bytes. A typed binding converts byte offsets and
byte strides once at its boundary; all U16 kernels then use sample pointers and
sample strides.

## Binding and alignment

`av2h_frame_decode_workspace_init_bytes` mirrors the byte-aware workspace
query. It accepts one or two bytes per sample, verifies that the requested width
matches sequence bit depth, recomputes and compares the complete layout, and
binds only the matching U8 or U16 views. The original initializer remains a
one-byte compatibility wrapper.

Every U16 sample allocation is aligned to `_Alignof(uint16_t)`. The final U16
output binder independently checks output alignment, exact required extent,
and checked plane offsets before creating typed pointers. Workspace and output
are separately caller-owned buffers, so each has its own alignment contract.

## Sample operations

Typed U16 operations now exist beside the U8 operations for plane fill/copy,
visible crop, clamped reference load, reconstructed load, predictor commit,
direct load/store, and domain-aware clip/store. Focused vectors cover row
padding and crop guards as well as values 0, 512, 700, and 1023. Production
selection of the normative 10-bit sample domain remains 10B-WP3 work.

## Deliberate decode boundary

For the locked 16x16 profile-0 10-bit 4:2:0 stream, `av2h_decode` now completes
container preparation, typed workspace initialization, and U16 output binding.
It then returns `AV2H_UNSUPPORTED` with `AV2H_CONTEXT_DECODE` at the first
unavailable owner: frame reconstruction. All 768 output bytes remain unchanged.
Overlapping workspace/output ranges are rejected before binding.

The raw WP0 oracle check continues to require exit code 3 and byte-for-byte
preservation of a pre-existing output file.

## Validation

Run the focused package gate with:

```sh
make 10bit-wp2
```

It runs exact private layout checks, U16 sample-operation tests, one-byte and
two-byte workspace accounting tests, public binding/nonmutation tests, and the
WP0 real-decoder nonmutation guard. Workspace coverage includes canaries,
exact-size success, one-byte-short rejection, alignment rejection, layout
mismatch rejection, output extent/alignment rejection, overlap rejection, and
checked arithmetic overflow. The ordinary `make test` suite remains the full
regression gate.
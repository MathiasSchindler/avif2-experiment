# 10-bit implementation WP9 GDF and final private output

## Result

10B-WP9 is complete. The private U16 path now runs deblock, CDEF, CCSO, and
GDF in normative order, then crops the final coded planes to tightly packed
little-endian `yuv420p10le`. All 22 supported tiny 4:2:0 streams match pinned
AVM final Y, U, and V output exactly.

Public HEIF admission is intentionally unchanged. The public decode API still
returns `AV2H_UNSUPPORTED` with `AV2H_CONTEXT_FILTER` before caller-output
mutation; canonical metadata admission and public format exposure belong to
WP10.

## GDF arithmetic

The typed U16 kernel keeps guided input in the 10-bit domain. Reconstruction
differences are not scaled, gradient features reduce by four bits, error
compensation uses symmetric two-bit rounding, and final samples clip to
`[0, 1023]`. Training-QP selection, scale selection, class construction, all
22 inputs, three network indices, and the intra error lookup retain the pinned
AVM table layout.

A direct scalar oracle linked against AVM revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d` locks exact classes, gradients,
errors, and output samples. Focused tests cover disabled and mode-2 controls,
invalid controls, clipping, overlap rejection, and deblocked reference-line
selection at stripe boundaries.

## Pipeline ownership

The U16 stage requires disjoint coded, deblocked, and CDEF/CCSO row ranges.
It copies reconstruction into caller-owned deblock storage, runs CDEF and CCSO
in their existing workspace planes, writes final GDF luma back to the coded Y
plane, and writes final CCSO chroma back to coded U and V. Visible output is
not copied until every filter succeeds.

Restoration is rejected before filtering and before caller-output mutation.
Undersized output and invalid workspace layouts likewise preserve caller
bytes. The hosted raw OBU utility selects the byte-explicit workspace from the
Sequence Header and emits exact 10-bit output through its historical `.i420`
path. Canonical suffix and public-tool format work remains in WP10.

## Retained evidence

`tests/fixtures/10bit/wp9-gdf-final.sha256` locks all 22 tiny final-output
files plus 72x64 pre/post-GDF coded-luma captures from the active-GDF
65x63 boundary stream. The post-GDF capture crops exactly to AVM's final
visible luma and differs from the pre-GDF capture. Dynamic AVM guided-input
halos are not present in those plane-origin captures, so executable arithmetic
parity is provided by the direct scalar oracle and boundary unit vectors.

## Validation

Run the focused package gate with:

```sh
make 10bit-wp9
```

It runs filter, reconstruction, frame, workspace-accounting, public-boundary,
and output-format tests under the selected host preset; verifies the WP6-WP9
manifests; and requires exact successful WP0 raw output. The complete
regression gate remains `make test`.

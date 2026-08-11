# 10-bit implementation WP8 CDEF and CCSO

## Result

10B-WP8 is complete. The private U16 filter stage now runs deblock, CDEF, and
CCSO in normative order using caller-owned workspace. Seven retained CDEF-only
streams, one CCSO-only stream, and two combined CDEF+CCSO streams match pinned
AVM exactly on complete 10-bit Y, U, and V planes.

This package does not implement U16 GDF or final filtered output. Public 10-bit
decode therefore still returns `AV2H_UNSUPPORTED` with `AV2H_CONTEXT_FILTER`
before mutating caller output. Decoded CDEF and CCSO controls are retained in
the private workspace so the bounded WP8 stage can be validated without
changing that boundary.

## CDEF arithmetic

The U16 direction search normalizes each sample with `sample >> 2` before
subtracting the 8-bit midpoint. This preserves AVM direction and variance
selection while keeping partial sums in their normative range. Primary and
secondary strengths are shifted left by two for filtering. Tap parity uses the
unshifted primary-strength parity, and damping is increased by two for luma or
one for chroma before the constrain operation.

Signed differences, variance-based luma strength adjustment, primary and
secondary taps, floor rounding, neighborhood clipping, partial blocks, and
zero-strength behavior match the scalar AVM path. Final U16 stores clip to
`[0, 1023]`. A 17x15 visible 4:2:0 vector retains 24x16 coded luma geometry and
the final partial-width chroma blocks.

## CCSO arithmetic

CCSO samples the immutable deblocked luma plane while updating the CDEF planes
in place. Edge classification uses full-range signed U16 differences and the
existing syntax-derived quantization threshold. Band lookup uses
`10 - max_band_log2`; unit controls, support offsets, and expanded LUT indices
remain narrow. Signed offsets are added in an `int` and clipped to
`[0, 1023]` before U16 storage.

Focused vectors distinguish 10-bit band selection from the 8-bit shift, cover
both edge-class directions, chroma luma-coordinate mapping, disabled controls,
and lower/upper clipping.

## Retained evidence

QP 94, 96, 121, 122, 125, 128, and 131 have CDEF enabled with CCSO and GDF
disabled. QP 124 has CCSO enabled with CDEF and GDF disabled. QP 93 and 104
have both CDEF and CCSO enabled with GDF disabled. Their normal AVM
`*-decoded.yuv` files are therefore unambiguous complete outputs for the WP8
stage after deblock.

All files are tightly packed little-endian 10-bit Y/U/V planes. SHA-256 hashes
are recorded in `tests/fixtures/10bit/wp8-cdef-ccso.sha256`. AVM is pinned at
revision `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.

## Validation

Run the focused package gate with:

```sh
make 10bit-wp8
```

It runs filter, reconstruction, frame, workspace-accounting, and public
boundary tests under the selected host preset; verifies the WP6, WP7, and WP8
oracle manifests; and reruns the WP0 decoder nonmutation oracle. The complete
regression gate remains `make test`.

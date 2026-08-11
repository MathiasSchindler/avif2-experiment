# 10-bit implementation WP7 deblock

## Result

10B-WP7 is complete. The mandatory deblock stage now accepts reconstructed U16
Y, U, and V planes, copies them into caller-owned stage storage, and applies the
same transform/block metadata traversal used by the 8-bit decoder. Eleven
retained 16x16 4:2:0 streams match pinned AVM exactly on complete deblocked
planes.

This package does not implement CDEF, CCSO, or GDF for U16 samples. Public
10-bit decode therefore still returns `AV2H_UNSUPPORTED` with
`AV2H_CONTEXT_FILTER` before mutating caller output. The stage-local entry point
exists so WP8 and WP9 can consume exact deblocked planes without changing
syntax or deblock control storage.

## Arithmetic

The existing quantizer threshold owner was already depth-aware. For 10-bit
input it derives the Q threshold from the 10-bit dequant step and derives the
side threshold from the normative Q-index domain shifted by 48. Thresholds from
both sides of an edge are rounded and averaged exactly as in AVM.

The U16 kernel preserves AVM's signed second derivatives, direction tests,
strong/weak filter selection, Q-threshold delta clamp, asymmetric weights, and
signed rounding. Only sample access and final clipping differ from the U8
kernel: U16 stores clip to `[0, 1023]`. Transform and block ownership, skip,
lossless, superblock asymmetry, and luma/chroma edge selection continue to use
`Av2hDeblockCell` without widening or duplication.

## Retained evidence

The retained oracle set contains seven active deblock outputs at QP 90, 91, 92,
99, 134, 148, and 160; one enabled no-op at QP 112; and disabled no-op controls
at QP 105, 139, and 144. Their census records show CDEF, CCSO, and GDF disabled,
so each normal AVM `*-decoded.yuv` output is also an unambiguous deblock-stage
capture. Each stream has a paired WP6 `*-unfiltered.yuv` reconstruction oracle.

The active set covers vertical and horizontal luma edges and reached chroma
edges. Isolated U16 vectors additionally retain both orientations, weak and
Q-clamped filtering, one-sided and two-sided lossless behavior, frame edges,
17x15 visible geometry with 24x16 coded storage, minimum and maximum samples,
and explicit no-op thresholds.

All stage files are tightly packed little-endian 10-bit Y/U/V planes. Their
SHA-256 hashes are recorded in
`tests/fixtures/10bit/wp7-deblocked.sha256`. AVM is pinned at revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.

## Validation

Run the focused package gate with:

```sh
make 10bit-wp7
```

It runs the filter, reconstruction, frame, workspace-accounting, and public
boundary tests under the selected host preset; verifies the WP6 reconstructed
and WP7 deblocked oracle manifests; and reruns the WP0 decoder nonmutation
oracle. The complete regression gate remains `make test`.

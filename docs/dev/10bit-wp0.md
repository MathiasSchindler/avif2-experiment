# 10-bit implementation WP0 oracle

## Result

10B-WP0 is complete. The first normative implementation stream is
`tests/fixtures/10bit/avm-reference/tiny-constrained-16x16-yuv420p10le-qp208.obu`.
Its exact contract is machine-readable in
`tests/fixtures/10bit/wp0-oracle.json`.

The stream is generated from the deterministic tiny 10-bit 4:2:0 source with
pinned AVM revision `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`. The generator
explicitly disables palette, IBP, IntraBC, CfL, MHCCP, CCTX, MRL, FSC, IST,
IDTX, Paeth, Smooth, restoration, deblock, CDEF, CCSO, and GDF, and requests
intra DCT-only transforms. These switches are part of the executable contract,
not assumptions inferred from absent census categories.

## Locked stream

- OBU size: 45 bytes.
- OBU SHA-256:
  `0a6489e29e1e72845924c27d81e1d9ea83c582336f9744fcfec4770e03733f30`.
- Output: tightly packed `yuv420p10le`, 16x16 Y plus 8x8 U and V.
- Output size: 768 bytes.
- Output SHA-256:
  `ca9ec6d9f6a901431de7364968b9b84ebf8ed34a832b167b66661add370f302b`.
- Plane SHA-256 values:
  - Y: `98d9e42de8a9aef3a649194227d876f1775c97a82272260c7e4cf051247ec219`.
  - U: `410fe42ed07908d04009787be600b1fc30013afc32812225d4a95c26d4491840`.
  - V: `8145a7b1d1b1b248005d5228c068a4b62c532e1bc0f7e920258d752e9a6290c8`.

The AVM inspection/accounting trace records:

- base Q-index 256 and reached per-block Q-index 252;
- a 25-bit uncompressed frame header;
- final tile entropy tell 192;
- two coding records, in the exact order pinned by the oracle;
- DC luma and chroma prediction only;
- 8x16 DCT_DCT transforms only;
- 166 accounting events; and
- deblock, CDEF, CCSO, and GDF disabled at frame level.

The base Q-index intentionally exceeds 255. This makes the 9-bit frame-syntax
requirement observable in the first implementation stream instead of relying
only on an isolated unit vector.

## Candidate correction

The initially proposed retained QP 144 fixture looked constrained in the WP10
aggregate report, but full block tracing showed nonzero luma palette sizes and
sequence-level IBP enablement. The retained tiny sweep contained no cleaner
DC/DCT-only stream. WP0 therefore generated a dedicated fixture with explicit
tool disables. This prevents palette or another unrelated coding owner from
being pulled into the first 10-bit reconstruction package.

## Commands

The self-contained gate uses the already built production decoder:

```sh
make 10bit-wp0
```

It verifies all artifact, OBU-ledger, source, plane, syntax, block-order, and
filter-state locks. It also runs the current sanitized decoder and requires
exit code 3 with `unsupported feature`, while preserving a pre-existing output
sentinel byte-for-byte.

The reference-backed reproducibility gate is:

```sh
make 10bit-wp0-reference
```

It rebuilds or verifies pinned AVM, re-encodes and decodes the stream, reruns
the accounting census, and requires byte-identical OBU and planar output plus
identical syntax/filter evidence.

For future partial decodes, the private comparator is:

```sh
python3 tools/check_10bit_wp0_oracle.py compare ACTUAL.yuv
```

It reports size mismatches first. Sample mismatches report the plane, linear
sample index, x/y coordinate, expected value, and actual value.

## Boundary

WP0 changed no production decoder admission or arithmetic. At WP0 completion,
ten-bit input failed at the explicit frame support gate before output mutation.
10B-WP1 subsequently admitted the constrained syntax and moved the deliberate
unsupported boundary to two-byte workspace binding. 10B-WP2 subsequently
bound typed two-byte storage and moved that boundary to reconstruction; later
packages own the remaining normative implementation.
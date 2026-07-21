# Milestone 0 evidence

Milestone 0 tests assumption A2: moving the Sequence Header OBU from AVM's raw
elementary stream into a separate `configOBUs` span preserves decode semantics
when configuration and item payload are presented as one reconstructed logical
AV2 stream.

## Reproduction

Run:

```sh
make milestone-0
```

This verifies or builds pinned AVM v1.0.0, runs the project-owned unit tests,
generates four deterministic 8-bit 4:2:0 sources, encodes one frame per source,
normalizes each OBU stream, decodes both forms, and requires byte-identical I420
output. The complete machine-readable record is generated at
`build/m0/matrix/manifest.json`.

The authoritative encoder and decoder argument arrays are recorded by
`tools/run_m0_matrix.py`, including explicit AV2, raw OBU, single-frame,
still-picture, 8-bit 4:2:0, quality, speed, and thread settings. The runner
rejects any AVM checkout other than commit
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d` or a dirty checkout.

## Result

The exit gate passed on macOS arm64 with these reference binary hashes:

- `avmenc`: `426a41472a45cbeda3376ba162731d39bc0e582c729098970a8167ccf8b022dd`
- `avmdec`: `5a9a7cfcfe5f6012acb664ec5206968a56dae6ca4641b428a9c50f34abcd46da`

| Fixture | Size | Source SHA-256 | Original OBU SHA-256 | Decoded I420 SHA-256 |
| --- | ---: | --- | --- | --- |
| tiny | 16x16 | `ab42238158a28dd09c0b8770b45b66054f30010a7990d474e2d405fd8bb0b5cf` | `f1671801dd0e83b837562e845c5f5bc8062db225f8d687b45feed3a0b9e80b12` | `5c18646460bb9f13c53baa6e65861a5cc746706a173e9c4f9b1cdeda86fd7ab5` |
| odd | 17x15 | `d2335215ead313209788de6df81a202e57d7f499c18c779a1c22f22736f83252` | `fbd99c5971da00073a3cb9b526df00129b1cff24734e5ef1f1b52fab35581c04` | `74b176943ef8760c8971600c4e6ec25d6e3b6a4b21ffc3a55df845a41bdef9af` |
| ordinary | 160x120 | `6a8516bc3702135f8af853572503be21f8c47013739b3a3b0e5fa9261f624252` | `8594be37f3f06510afccfdd0aed9dd4076c3f7ac1d3c495a365f55cf606d82d9` | `def0ced80c4bc1866d0d865fb392c80020ac0a823f310b2f62999d10eede1af5` |
| high-detail | 96x80 | `b92b6949cafe80bb80660249c18830d49ffaf70e7310fb9711c3b681d3fa42b2` | `248fc661313998b6c52a16f2daf85a4432e07ce649c11e9e165af5bcd5de6864` | `4120930c9c9c49f73d4aa3e1d4c56cbb4b557679b42fe3b9de984bca1431745a` |

Every original stream had this ordered ledger:

1. Temporal Delimiter
2. Sequence Header
3. Closed Loop Key

Normalization removed the Temporal Delimiter, placed the single Sequence
Header in `configOBUs`, and preserved the Closed Loop Key OBU byte-for-byte as
the item payload. Reconstructed streams contained Sequence Header followed by
Closed Loop Key. Decoder-reported dimensions, I420 format, complete file bytes,
and per-plane hashes matched for every pair.

## Scope

The result supports A2 for the selected single-layer, one-frame v0 profile. It
does not establish a standardized AV2 HEIF image-item binding or validate other
profiles and metadata OBUs. Explicit BT.709 encoder signaling produced a
Content Interpretation OBU; because initial v0 does not yet define that OBU's
configuration or container mapping, the canonical Milestone 0 matrix leaves
color interpretation unspecified rather than silently discarding metadata.
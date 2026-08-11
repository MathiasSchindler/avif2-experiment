# Carriage reference evidence

The carriage experiment tests assumption A2: moving the Sequence Header OBU from AVM's raw
elementary stream into a separate `configOBUs` span preserves decode semantics
when configuration and item payload are presented as one reconstructed logical
AV2 stream.

## Reproduction

Run:

```sh
make test-carriage-reference
```

This verifies or builds pinned AVM v1.0.0, runs the project-owned unit tests,
generates four deterministic 8-bit 4:2:0 sources, encodes one frame per source,
normalizes each OBU stream, decodes both forms, and requires byte-identical I420
output. The complete machine-readable record is generated at
`build/carriage-reference/matrix/manifest.json`.

The authoritative encoder and decoder argument arrays are recorded by
`tools/run_carriage_reference.py`, including explicit AV2, raw OBU, single-frame,
still-picture, 8-bit 4:2:0, quality, speed, and thread settings. The runner
rejects any AVM checkout other than commit
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d` or a dirty checkout.

The original experiment passed `--full-still-picture-hdr`, but that option
selects the longer full still-picture Sequence Header rather than the compact
single-picture syntax required by canonical v0. The corrected matrix omits
the option, independently parses the resulting short Sequence Headers, and
still produces sample-exact decoded output before and after normalization.

## Result

The exit gate passed on macOS arm64 with these reference binary hashes:

- `avmenc`: `426a41472a45cbeda3376ba162731d39bc0e582c729098970a8167ccf8b022dd`
- `avmdec`: `5a9a7cfcfe5f6012acb664ec5206968a56dae6ca4641b428a9c50f34abcd46da`

| Fixture | Size | Source SHA-256 | Original OBU SHA-256 | Decoded I420 SHA-256 |
| --- | ---: | --- | --- | --- |
| tiny | 16x16 | `ab42238158a28dd09c0b8770b45b66054f30010a7990d474e2d405fd8bb0b5cf` | `9d0d4888795cc89c9a0982ecc4dd8cd3c228bfe7163410748d4d0da240dc8e2a` | `5c18646460bb9f13c53baa6e65861a5cc746706a173e9c4f9b1cdeda86fd7ab5` |
| odd | 17x15 | `d2335215ead313209788de6df81a202e57d7f499c18c779a1c22f22736f83252` | `458b164bce45e2ead530f5c901f806e244d27d28cb02f604505cb5f95cf881db` | `74b176943ef8760c8971600c4e6ec25d6e3b6a4b21ffc3a55df845a41bdef9af` |
| ordinary | 160x120 | `6a8516bc3702135f8af853572503be21f8c47013739b3a3b0e5fa9261f624252` | `d14ec9fbe436181a9b2becc97eb50679bf59aaad9d015ba55f97d720fb505378` | `def0ced80c4bc1866d0d865fb392c80020ac0a823f310b2f62999d10eede1af5` |
| high-detail | 96x80 | `b92b6949cafe80bb80660249c18830d49ffaf70e7310fb9711c3b681d3fa42b2` | `544cf228b81054e000698970d2ff3478b0cf2acd0e46a561d93c0e59b060c8e3` | `4120930c9c9c49f73d4aa3e1d4c56cbb4b557679b42fe3b9de984bca1431745a` |

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
configuration or container mapping, the canonical carriage experiment matrix leaves
color interpretation unspecified rather than silently discarding metadata.
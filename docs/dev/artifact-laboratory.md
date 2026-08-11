# Artifact laboratory

artifact laboratory implements a hosted, machine-readable laboratory around the
freestanding AV2-HEIF v0 core. One command generates, normalizes, muxes,
inspects, extracts, decodes, and compares the initial four-fixture matrix using
the pinned local AVM v1.0.0 build.

The laboratory does not add AVM, libc, allocation, or file I/O to the
freestanding core. Its C command-line tool is a separately built hosted adapter
that calls the public core API.

## Reproduction

Prepare the pinned reference build once, if it is not already available:

```sh
make reference-avm
```

This setup target may access the network to obtain the exact locked AVM
revision. Normal laboratory runs use only the verified local checkout and
binaries:

```sh
make test-artifact-lab
```

The command first runs the complete vendor-free test gate, then processes all
four fixtures. Results are written below `build/artifact-lab/`; the common
manifest is `build/artifact-lab/manifest.json`. The target deliberately does
not depend on `reference-avm`, so a missing local build fails instead of
silently downloading software.

## Pipeline

For each deterministic 8-bit I420 source, `tools/run_artifact_lab.py` performs:

1. Generate the source and encode one AV2 frame with pinned `avmenc`.
2. Normalize the elementary stream into one Sequence Header configuration OBU
   and one Closed Loop Key item OBU.
3. Mux those spans into canonical AV2-HEIF experimental v0.
4. Inspect the typed container, box graph, associations, OBUs, and metadata.
5. Extract configuration, payload, and reconstructed logical stream.
6. Decode the original and extracted streams with pinned `avmdec`.
7. Classify encoded-byte, normalized-structure, metadata, and plane results.

The hosted `build/host/av2heif-artifact` command has three operations:

```text
av2heif-artifact pack CONFIG PAYLOAD OUTPUT CP TC MC limited|full
av2heif-artifact inspect INPUT OUTPUT_JSON
av2heif-artifact extract INPUT CONFIG PAYLOAD RECONSTRUCTED
```

`pack` and `extract` use `av2h_heif_write` and `av2h_heif_parse` directly.
`inspect` first requires the strict typed parser to accept the file, then emits
the checked box tree and the item/property graph for the canonical profile.

## Artifact schema

The top-level `av2heif-artifact-laboratory-v1` manifest records:

- source and generated data files' relative paths, byte counts, and SHA-256;
- nested box type, offset, size, header, payload, and child structure;
- primary item, extent, property table, essential flags, and associations;
- ordered original, normalized, and extracted OBU ledgers with length, header,
  body, and payload spans plus per-OBU SHA-256;
- effective Sequence Header, dimensions, pixel format, bit depth, range, and
  CICP values obtained from strict container inspection;
- normalized whole-file and Y, U, and V plane hashes;
- exact command argument arrays and shell renderings;
- AVM revision, tag, CMake options, compiler, and binary hashes; and
- project tool revision, dirty-worktree record, compiler identity, exact build
  flags, binary hash, and source hashes.

Every fixture also has a standalone `result.json`, and every canonical
container has an `inspection.json` using schema
`av2heif-artifact-inspection-v2`. Version 2 adds the query surface query result,
including input binding, structural counts, plane layouts, output sizing, and
pre-decode workspace sizing.

The parent laboratory manifest records each fixture `result.json` hash. A
manifest does not recursively record its own hash; doing so would be
self-referential. The source manifest, normalization ledger, inspection,
container, OBU spans, and decoded outputs are all hash-tracked.

## Comparison semantics

The result intentionally distinguishes different forms of equivalence:

- `encoded_bytes` compares the original AVM stream and extracted normalized
  stream hashes. They differ because normalization removes the Temporal
  Delimiter and relocates the Sequence Header into configuration.
- `normalized_obu_structure` compares typed OBU structure and OBU hashes after
  normalization. It is identical for mux input and extracted output.
- `interpreted_metadata` compares dimensions, format, bit depth, range, and the
  complete CICP triplet against the inspected container values.
- `decoded_planes` requires byte-identical I420 files and records per-plane
  hashes; all fixtures are sample-exact.
- `lossy_quality` is explicitly `not-measured` because this laboratory compares
  carriage, not encoder quality.

AVM's raw I420 decoder output reports dimensions and samples but does not report
range or CICP. The manifest marks those decoder fields as
`not-reported-by-avmdec`; range and CICP comparisons are attributed to the
explicit source metadata and inspected `nclx` property, not inferred from raw
planes.

## Result

The complete matrix passes with pinned AVM commit
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.

| Fixture | Canonical container SHA-256 | Decoded I420 SHA-256 |
| --- | --- | --- |
| tiny | `9d8e6e53b11256c31bd4350d56798e802987d3b0e83853222850c7061e170128` | `5c18646460bb9f13c53baa6e65861a5cc746706a173e9c4f9b1cdeda86fd7ab5` |
| odd | `682f1abc6c865a4ecc30951e51e1e807c4c56a3d7a6bec8e407209d1fb2e0ffb` | `74b176943ef8760c8971600c4e6ec25d6e3b6a4b21ffc3a55df845a41bdef9af` |
| ordinary | `db7c3820be2696ce188ff58c38d1f33fd704ce11374901c39d276ec7c4dfd6d7` | `def0ced80c4bc1866d0d865fb392c80020ac0a823f310b2f62999d10eede1af5` |
| high-detail | `1ea2afce8a84aef16811440f389625fdbb41588d766a6bda16f7a59a4ba4c2b2` | `4120930c9c9c49f73d4aa3e1d4c56cbb4b557679b42fe3b9de984bca1431745a` |

## Limitations

The current adapter describes the strict one-item v0 graph and its one OBU per
configuration and payload span. It is not a general-purpose HEIF dumper. AVM is
the only applicable AV2 reference adapter currently pinned; AV1-only tools are
not treated as AV2 oracles. The laboratory does not measure lossy quality,
compression efficiency, timing, or memory use. Broader schema validation,
fuzzing, independent AV2 implementations, and additional profiles remain later
work.
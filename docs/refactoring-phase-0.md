# Refactoring Phase 0: trustworthy gates

**Status:** Complete as of 2026-08-04.

Phase 0 prepares the decoder for move-only module extraction. It does not
change decoding algorithms or remove diagnostics. The permanent plan and exit
criteria remain in `decoder-refactoring-plan.md`.

## Completed gates

### Artifact parity

`windows-decoder-artifact-parity` decodes every `.obu` listed in
`tests/artifact-parity.sha256` and compares the complete I420 SHA-256 digest
with the output produced by pinned AVM revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.

The test has these properties:

- 31 explicit fixtures; a count mismatch fails before decoding;
- no AVM, vendor checkout, Python, or PowerShell dependency;
- one CTest invocation for the complete corpus;
- output under `Testing/Temporary/artifact-parity`;
- cleanup before success or failure is reported.

Validated result: 31/31 complete I420 hashes exact.

### Freestanding symbols

The `av2heif-freestanding-check` CMake target compiles the canonical
`AV2HEIF_CORE_SOURCES` list with the freestanding release flags. A portable
object-level symbol check verifies that every undefined reference has a
definition in another core object. This avoids ELF/Mach-O/COFF relocatable-link
differences and rejects compiler runtime or libc references.

The historical `make test-freestanding` entry point now delegates to this
CMake target and no longer owns a second strict source list.

Validated result: no unresolved core symbols with Clang 22.1.8 on Windows.

### State layout

`layout-unit` records the current 64-bit C ABI. These values are behavioral
guards for move-only work, not desired long-term sizes.

| Type | Size | Alignment |
| --- | ---: | ---: |
| `Av2hSequenceState` | 66 | 2 |
| `Av2hFrameState` | 3,192 | 8 |
| `Av2hFirstCoeffLevelCdfs` | 20,464 | 2 |
| `Av2hLeafCdfs` | 2,498 | 2 |
| `Av2hTileDecoder` | 33,424 | 8 |
| `Av2hReconstructionScratch` | 16,896 | 4 |
| `Av2hFilterWorkspace` | 368 | 8 |
| `Av2hFilterWorkspaceLayout` | 152 | 8 |

Filter workspace query sizes, alignment rejection, undersized-memory rejection,
and storage binding remain covered by `filter-unit`.

## Restored hosted harnesses

The five previously excluded harnesses now run in default CTest. Obsolete
whole-stream entropy frontiers and exhaustive duplicate pixel sweeps were
retired in favor of focused owner-level checks and the stronger complete-corpus
output gate.

| Harness | Restored contract |
| --- | --- |
| `tile-unit` | scheduler, root handoff, argument, and failure behavior |
| `leaf-unit` | mode/context, palette, GDF, and IntraBC behavior |
| `transform-unit` | current CDF layout, syntax maps, and signaling behavior |
| `reconstruct-unit` | focused prediction, transform, clipping, and malformed-input behavior |
| `artifact-profile-unit` | OBU, sequence, frame-header, dimensions/QP, and tile-group parsing for all 31 artifacts |

The `AV2HEIF_ENABLE_LEGACY_HOST_TESTS` option and all CTest exclusions were
removed. All 15 hosted harnesses are registered by default.

## Diagnostic inventory

There are 95 `av2h_debug_*` global definitions across eight decoder sources:

| Owner | Globals | Classification |
| --- | ---: | --- |
| `coeff.c` | 10 | error reporting and focused entropy observations |
| `coeff_level.c` | 11 | focused FSC observations and artifact trace debris |
| `filter.c` | 3 | error reporting and focused stage observations |
| `leaf.c` | 21 | mode/context observations and error reporting |
| `partition.c` | 14 | traversal observations and traces |
| `reconstruct.c` | 23 | error reporting, prediction observations, artifact traces |
| `tile.c` | 12 | error reporting, leaf traces, artifact traces |
| `transform.c` | 1 | error reporting |

The disposition is intentionally by family:

- Retain stage, reason, and range data currently used to populate decode failure
  results, but move it into owner-scoped result records in Phase 9.
- Replace generic CDF, context, prediction, and filter observations with focused
  test result records or stable behavioral hashes.
- Remove artifact-specific `chimp`, `target`, raw MHCCP trace, and second
  superblock trace families after their useful assertions have replacements.

No global is removed during structural extraction. Mixing that cleanup with a
move would make parity regressions harder to localize.

## Completion validation

- Hosted CTest: 15/15 passed.
- Windows release CTest: 51/51 passed.
- Complete artifact parity: 31/31 I420 hashes exact.
- Freestanding object closure: no undefined core symbols.
- Schwaebisch Q208 output: 173,611 bytes with SHA-256
  `8906752a123863bdb65d1c2dad8edd9ae5ce0c8c0dfd21fce48f14a9d09faaa1`.
- Pinned AVM checkout: clean.

LLVM-MinGW on Windows does not provide a usable AddressSanitizer runtime for
this host configuration. The `host-sanitized` preset remains the sanitizer
gate on Linux and macOS; Phase 0 does not weaken or bypass it.

## Commands

```sh
cmake --preset host
cmake --build --preset host
ctest --preset host
```

```powershell
cmake --preset windows-x86_64
cmake --build --preset windows-x86_64
ctest --test-dir build/windows-x86_64 `
  -R '^windows-decoder-artifact-parity$' --output-on-failure
```
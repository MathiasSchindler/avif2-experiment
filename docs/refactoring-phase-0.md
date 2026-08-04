# Refactoring Phase 0: trustworthy gates

**Status:** In progress as of 2026-08-04.

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

## Excluded harness baseline

The five legacy harnesses were built and run without changing expectations.
None is suitable for default CTest yet.

| Harness | Current result | Required replacement direction |
| --- | --- | --- |
| `tile-unit` | 10 failed checks | state transitions and handoff behavior |
| `leaf-unit` | 501 failed checks | focused mode/context/palette/IntraBC vectors |
| `transform-unit` | 16 failed checks | syntax maps and exact entropy endpoints |
| `reconstruct-unit` | access violation | isolate the crashing vector, then family pixel hashes |
| `artifact-profile-unit` | 638 failed checks | remove whole-stream incidental snapshots |

`AV2HEIF_ENABLE_LEGACY_HOST_TESTS` remains available for this restoration work.
It must not be removed until all five replacements pass under the default gate.

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

## Remaining Phase 0 work

1. Restore `transform-unit` with focused syntax-map and entropy tests.
2. Replace stale `leaf-unit` and `tile-unit` snapshots with owner-level vectors.
3. Isolate the `reconstruct-unit` access violation and rebuild coverage by
   prediction/transform family.
4. Reduce `artifact-profile-unit` to end-to-end contracts not already covered
   by the 31-artifact parity gate.
5. Run the host sanitizer gate on Linux or macOS, then rerun the complete
   Windows release suite before Phase 1 starts.

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
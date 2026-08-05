# Decoder refactoring plan

## Purpose

Restructure the private decoder so that Milestone 10 and later feature work can
continue without changing decoded output, the freestanding dependency boundary,
or the current memory model.

This is a sequence of independently validated extractions, not a rewrite. Each
step must keep the decoder freestanding C11, allocator-free, libc-free in the
production target, and byte-exact for the tracked artifact corpus.

## Current shape

Measured source size on 2026-08-04:

| File | Lines | Approximate functions | Primary responsibilities |
| --- | ---: | ---: | --- |
| `reconstruct.c` | 9,294 | 85 | IST, prediction, inverse transforms, CfL/MHCCP, plane commit |
| `coeff_level.c` | 3,636 | 45 | CDF initialization, coefficient contexts, level reads, dequantization |
| `leaf.c` | 2,931 | 44 | mode entropy, filter controls, palette, IntraBC, neighbor state |
| `filter.c` | 1,723 | 53 | deblock, CDEF, GDF, CCSO, filter workspace and orchestration |
| `transform.c` | 1,665 | 10 | transform CDFs, syntax maps, primary/secondary transform reads |
| `partition.c` | 1,259 | 27 | partition contexts, decisions, traversal |
| `tile.c` | 1,068 | 22 | tile state machine and luma/chroma orchestration |
| `frame_output.c` | 874 | 13 | filter metadata, output sizing, crop/copy |
| `coeff.c` | 777 | 15 | coefficient prefix syntax and context state |
| Remaining nine `.c` files | 2,069 | 66 | focused geometry, range, frame, and CDF ownership |

The size is not the only problem. The large headers expose many private structs,
and tests depend on dozens of `av2h_debug_*` globals. Five snapshot-heavy unit
binaries (`tile`, `leaf`, `transform`, `reconstruct`, and `artifact-profile`)
compile but are currently excluded from default CTest because their intermediate
expectations are stale. Product parity is stronger: all 31 tracked artifacts
currently match pinned AVM I420 output byte-for-byte.

## Goals

- Give each module one codec responsibility and a small private interface.
- Make dependency direction visible and enforceable.
- Keep public and existing private function signatures stable during move-only
  phases.
- Replace test access to global implementation details with focused behavioral
  probes over time.
- Make Milestone 10 workspace/API ownership straightforward.
- Keep generated normative tables generated and reviewable as data.

## Non-goals

- No algorithm changes while moving code.
- No broad renaming, formatting pass, or C dialect change.
- No heap allocation, libc dependency, dynamic dispatch, or opaque runtime
  framework.
- No attempt to make every file the same size. Cohesion is more important than
  a fixed line limit; roughly 500-2,000 lines is a useful signal, not a rule.
- No public API expansion before the Milestone 10 design is ready.

## Intended dependency direction

Dependencies should flow in one direction:

```text
base / range / cdf / geometry
        |
        v
partition, mode, coefficient and transform syntax
        |
        v
prediction and inverse transforms
        |
        v
reconstruction planes and frame filters
        |
        v
tile / frame-output orchestration
        |
        v
platform CLI and future public decode API
```

Lower layers must not include tile, frame-output, platform, or public decode
orchestration headers. Sibling implementations communicate through narrow
private headers under `src/decoder/internal/`; they must not grow another
monolithic internal header.

## Refactoring rules

1. One responsibility extraction per change.
2. A move-only change may adjust linkage and includes, but not conditions,
   constants, table values, arithmetic, or error precedence.
3. Preserve existing externally visible private entry points as forwarding
   wrappers until all callers and tests have migrated.
4. Add source files explicitly to CMake; do not use globbing.
5. Run the narrowest relevant test immediately after each extraction, then the
   complete release gate before merging.
6. Keep state sizes and alignment stable unless a separately reviewed change
   deliberately updates workspace calculations and assertions.
7. Do not combine debug-instrumentation cleanup with algorithm extraction.

## Phase 0: establish trustworthy gates

This phase blocks the structural refactor.

**Status:** Complete. Implementation and validation details are recorded in
`refactoring-phase-0.md`.

- Add a vendor-free all-artifact parity test that decodes all 31 `.obu` files
  and compares complete I420 hashes with the pinned references. It must use a
  temporary output directory and leave no stage snapshots in `build/`.
- Refresh or replace stale intermediate expectations in the five excluded host
  tests. Prefer focused vectors at the owning API over re-locking thousands of
  incidental entropy tells.
- Re-enable each host test in default CTest as soon as its reliable assertions
  pass; remove `AV2HEIF_ENABLE_LEGACY_HOST_TESTS` when no exclusions remain.
- Restore the strict freestanding undefined-symbol gate under CMake so it is
  available on Linux and macOS without Makefile-owned source lists.
- Record baseline sizes for key state objects and scratch/workspace alignment.
- Classify every `av2h_debug_*` global as a retained diagnostic, a focused test
  observation that needs an accessor, or obsolete trace debris.

Exit gate: all active host tests pass with sanitizers where available, all 31
artifacts are byte-exact, the Windows release tests pass, and the freestanding
symbol check passes.

## Phase 1: establish private module conventions

**Status:** Complete. Conventions and validation details are recorded in
`refactoring-phase-1.md`.

Create `src/decoder/internal/` and use it only for interfaces between decoder
implementation files. Publicly included private headers such as `reconstruct.h`
keep their current declarations during this phase.

Start with shared helpers that are already duplicated, but only after focused
unit vectors pin their behavior. Examples are signed rounding, clipping, small
geometry predicates, and plane bounds checks. Do not create a generic utility
bucket; helpers stay with the lowest coherent owner.

Exit gate: header self-containment builds, no dependency cycle, no state-size
change, and no output change.

## Phase 2: split frame filtering as the pilot

**Status:** Complete. Ownership decisions and validation details are recorded
in `refactoring-phase-2.md`.

`filter.c` has good focused coverage and clear boundaries, making it the safest
place to prove the extraction workflow.

Target layout:

- `filter.c`: pipeline ordering and `av2h_filter_frame()` only.
- `filter_workspace.c`: workspace query/init and overlap validation.
- `deblock.c`: thresholds, edge decisions, lane kernels, frame traversal.
- `cdef.c`: direction search, constrain/tap kernels, block/frame filtering.
- `gdf.c`: generated GDF data and GDF block/frame filtering.
- `ccso.c`: control validation, classification, and frame filtering.

Keep `filter.h` stable initially. Add narrow internal headers only where the
pipeline needs a sibling entry point. Move `gdf_data.inc` with its owner but do
not edit generated values.

Validation after each file extraction: focused `filter-unit`, sanitizer gate,
then all-artifact parity after the phase.

## Phase 3: split coefficient-level ownership

**Status:** Complete. Ownership decisions and validation details are recorded
in `refactoring-phase-3.md`.

Target layout:

- `coeff_level_cdf.c`: CDF bank initialization and generated coefficient/FSC
  data includes.
- `coeff_dequant.c`: Q context, geometry, dequant steps, clamping, and CUL
  finalization.
- `coeff_context.c`: scan positions and LF/base/BR/sign context derivation.
- `coeff_level.c`: luma/chroma/FSC entropy reads and public forwarding entry
  points.

Keep `Av2hFirstCoeffLevelCdfs` layout unchanged. The generated
`coeff_cdf_*.inc` and `fsc_cdf_data.inc` files remain generator-owned data.

Validation: `coeff-level-unit`, `coeff-unit`, relevant malformed vectors,
freestanding symbol check, then all-artifact parity.

## Phase 4: split transform syntax and table ownership

**Status:** Complete. Ownership decisions and validation details are recorded
in `refactoring-phase-4.md`.

Before this phase, restore `transform-unit` to the default gate.

Target layout:

- `transform_cdf.c`: transform CDF initialization.
- `transform_map.c`: primary type maps and secondary set/type maps.
- `transform.c`: syntax signaling and range-decoder reads.

This phase does not move inverse-transform kernels; those belong to
reconstruction phases. Keep `Av2hFirstTransformCdfs` layout and existing public
private entry points stable.

Validation: `transform-unit`, coefficient tests, exact entropy endpoint vectors,
and all-artifact parity.

## Phase 5: split leaf syntax and context state

**Status:** Complete. Ownership decisions and validation details are recorded
in `refactoring-phase-5.md`.

Before this phase, replace stale whole-stream leaf expectations with focused
mode, state-transition, palette, IntraBC, and filter-control vectors.

Target layout:

- `leaf_cdf.c`: `Av2hLeafCdfs` initialization.
- `filter_entropy.c`: GDF, CDEF, and CCSO entropy state/read ownership.
- `palette.c`: color cache, packed map, and palette entropy reads.
- `intrabc.c`: candidate bank, displacement syntax, and morph context.
- `leaf_context.c`: neighbor derivation and context-state lifecycle.
- `leaf.c`: luma/chroma mode orchestration and compatibility wrappers.

Do not split the `Av2hFirstLeafMode` structure during move-only work. Its later
separation into syntax result and diagnostics belongs to Phase 9.

Validation: restored `leaf-unit`, partition and tile tests, sanitizer gate, and
all-artifact parity.

## Phase 6: split reconstruction, highest risk

**Status:** Complete as of 2026-08-04. See `refactoring-phase-6.md`.

Apply this only after Phases 1-5 establish conventions and reliable focused
coverage. Preserve `reconstruct.h` and its entry points throughout the moves.

Suggested extraction order:

1. `secondary_transform.c`: IST scan, generated IST tables, and inverse
   secondary-transform kernels.
2. `inverse_transform.c`: primary 1-D kernels, geometry/kind selection, generic
   row/column inverse transform.
3. `intra_prediction.c`: DC, smooth, Paeth, directional, IDIF, MRL, IBP, and
   edge preparation for luma/basic chroma prediction.
4. `intrabc_prediction.c`: IntraBC copy/morph prediction and source validation.
5. `chroma_prediction.c`: explicit/derived CfL and MHCCP fitting/sampling.
6. `reconstruct.c`: residual composition, cross-component transform,
   reconstructed-plane readiness, and public private orchestration wrappers.

Important constraints:

- Keep all `ist_*.inc` files together under the secondary-transform owner and
  generator workflow.
- Do not deduplicate similar prediction branches during extraction. First move
  them exactly; simplify only in later reviewed behavior-preserving changes.
- Keep `Av2hReconstructionScratch` caller-owned and layout-stable.
- Introduce internal parameter structs only when they reduce argument drift
  without increasing workspace or hiding bounds.

Validation after every extraction: a focused reconstruction subset for the
moved family, restored `reconstruct-unit`, sanitizer gate, Windows release
suite, and all 31 artifact hashes. Any parity change stops the phase.

## Phase 7: partition and tile orchestration

**Status:** Complete as of 2026-08-04. See `refactoring-phase-7.md`.

Once syntax and reconstruction dependencies point downward, separate:

- partition context derivation from traversal/decision reading;
- tile luma/chroma stepping from reconstruction commit;
- frame filter-control handoff from tile traversal.

`tile.c` should end as a small explicit state machine. It may coordinate lower
layers but must not contain codec kernels or table selection.

Validation: restored `tile-unit`, malformed/truncation tests at every state
transition, output nonmutation on failure, and full parity.

## Phase 8: frame output and workspace ownership

**Status:** Complete as of 2026-08-04. See `refactoring-phase-8.md`.

Prepare Milestone 10 by making one owner responsible for:

- coded/reference/visible plane dimensions;
- deblock/filter metadata storage;
- workspace size/alignment queries;
- caller output binding and final crop/copy;
- overlap, truncation, and undersized-workspace rejection.

No public API should expose entropy-module structs. The future public decode
state should compose private owners rather than duplicate their fields.

Validation adds workspace boundary, alignment permutation, stale-input,
truncation, and nonmutation tests before exposing the API.

## Phase 9: diagnostics cleanup

Only after structural moves are complete:

- Remove obsolete artifact-specific globals and unconditional diagnostic file
  writes.
- Group retained observations by owner in private diagnostic records.
- Pass an optional private diagnostic pointer through decoder state or compile
  diagnostics behind an explicit development option.
- Convert tests from `extern av2h_debug_*` access to focused result records or
  stable behavioral hashes.
- Ensure release builds have no diagnostic output or stage-file side effects.

This phase must not change decode decisions. Validate both diagnostics-enabled
and release configurations against the same output hashes.

## Generated data policy

The following remain generated normative data, not hand-refactoring targets:

- `partition_cdf_data.inc`
- `coeff_prefix_cdf_all.inc`
- `coeff_cdf_all.inc`, `coeff_cdf_q0_*.inc`, and `fsc_cdf_data.inc`
- `gdf_data.inc`
- all `ist_*.inc` files

A structural move may change which `.c` file includes them. Any content change
requires its generator/checker and independent normative validation.

## Validation matrix

Every extraction uses the narrowest row that applies, followed by the release
row before merge.

| Scope | Required checks |
| --- | --- |
| CDF/data ownership | owning bank hash test and generator/checker |
| Syntax/context move | owning unit test plus exact tell/range vectors |
| Prediction/transform move | focused pixel/coefficient hashes under ASan/UBSan |
| Filter move | stage-local filter evidence and final output hashes |
| Tile/orchestration move | state-transition, malformed input, nonmutation tests |
| Release gate | strict freestanding symbols, Windows CTest, all 31 AVM-reference I420 hashes, clean vendor tree |

## Completion criteria

The refactor is complete when:

- no production `.c` file mixes entropy syntax, prediction kernels, transform
  kernels, filtering, and orchestration;
- dependency direction matches this plan without cycles;
- all 14 hosted C harnesses are trustworthy and enabled by default;
- release builds contain no unconditional debug traces or stage files;
- workspace ownership is ready for the Milestone 10 public API;
- all 31 tracked artifacts remain byte-identical to pinned AVM output.

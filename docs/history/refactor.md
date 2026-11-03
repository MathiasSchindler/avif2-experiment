# Decoder refactoring milestones

**Status:** M0-M8 complete, 3 November 2026. M8 acceptance was narrowed by
the user to execution on this macOS computer; cross-builds and complete image
corpora are not required.

## Purpose and scope

Improve ownership, admission logic, and private interfaces in
[`src/decoder/`](../../src/decoder/) and the files directly under
[`src/`](../../src/). This is a second incremental refactoring pass, not a rewrite
or an expansion of the supported AV2 profile.

The [previous refactoring plan](../dev/decoder-refactoring-plan.md) and its phase
reports remain historical evidence. This plan builds on those extractions.
[Decoder capabilities](../capabilities.md) remains the authoritative contract.

Supporting changes may touch tests, build source lists, generators, and
documentation where an extraction requires them. Platform behavior, public
formats, encoder recipes, and oracle updates are not refactoring objectives.

## Starting points

Source sizes measured on 2 November 2026:

| Owner | Lines | Main opportunity |
| --- | ---: | --- |
| [`intra_prediction.c`](../../src/decoder/intra_prediction.c) | 3,839 | Separate kernels, edge preparation, and admission/dispatch |
| [`reconstruct.c`](../../src/decoder/reconstruct.c) | 3,080 | Separate residual reconstruction from picture-plane integration |
| [`frame_workspace.c`](../../src/decoder/frame_workspace.c) | 1,482 | Separate storage planning, output copying, and filter metadata |
| [`frame_output.c`](../../src/decoder/frame_output.c) | 964 | Shorten orchestration and repeated failure-context assembly |
| [`carriage.c`](../../src/carriage.c) | 1,121 | Separate OBU framing, codec syntax, and carriage policy |
| [`container.c`](../../src/container.c) | 1,201 | Separate container reading/probing from writing |

Size alone does not justify splitting a file. Existing focused filter,
arithmetic, geometry, and layout owners should remain intact unless a concrete
dependency or responsibility problem is demonstrated.

## Invariants

Every milestone must preserve:

- Exact decoded samples for admitted 8-bit and 10-bit inputs.
- The distinction between legal AV2 syntax and implemented combinations.
  Refactoring must neither admit unsupported tuples nor reject supported ones.
- Entropy-read and adaptation order, tile/chroma scheduling, reference
  availability, filter order, and failure precedence.
- Public signatures, public layouts, caller-owned state, workspace size and
  alignment, checked bounds, and documented output nonmutation on failure.
- Accurate statuses: unsupported syntax must not become overflow, malformed
  data, an argument error, or an allocation failure.
- Freestanding C11, allocation-free and libc-free production decoding, and
  existing platform behavior.
- The pinned AVM v1.0.0 baseline, encoded fixtures, golden pixels, and locks.
  The moving oracle remains a separate opt-in check.

Use one responsibility extraction per change. Move code before simplifying it:
move-only changes may adjust linkage, includes, and forwarding wrappers, but
must not change arithmetic, constants, conditions, or error precedence.

Update all affected explicit source manifests, including the
[Makefile](../../Makefile) and existing platform/WebAssembly build scripts. Do not
introduce source globbing, a generic dispatch framework, or a new utility
bucket. Proposed owner names below are illustrative, not mandatory file counts.

## Milestone overview

M0-M8 are complete under the revised local-macOS acceptance scope. Dependencies are
prerequisites, not merely list order; independent work may proceed separately
if its edits do not overlap.

| Milestone | Outcome | Prerequisites |
| --- | --- | --- |
| M0 (complete) | Freeze behavioral and resource evidence | None |
| M1 (complete) | Extract prediction families without behavior changes | M0 |
| M2 (complete) | Simplify prediction admission and dispatch | M1 |
| M3 (complete) | Separate block reconstruction from plane integration | M1 |
| M4 (complete) | Clarify frame storage, metadata, output, and orchestration | M3 |
| M5 (complete) | Separate codec syntax from root-level carriage/container work | M0 |
| M6 (complete) | Narrow private headers around the resulting owners | M1, M3, M4, M5 |
| M7 (complete) | Standardize error-record initialization and context assembly | M4, M5 |
| M8 (complete) | Verify local macOS execution and document final ownership/integration evidence | M2, M6, M7 |

The recommended main sequence is M0 through M4, then M5 through M8.
Minimal interface changes needed by an extraction belong in that milestone;
M6 is the broader dependency cleanup, not a reason to postpone necessary headers.

## M0: freeze behavior and resource evidence

**Completed:** [behavioral and resource baseline](../dev/refactoring-m0.md).
The baseline adds implementation-characterization tests, freezes measured
resources, maps extraction owners to active tests, and records the existing
missing-media failures. It does not claim that the full corpus gate passes.

Before moving code:

- Record the current native test results, admitted profile boundaries, layout
  assertions, and representative workspace sizes.
- Preserve the retained Baya Weaver regression and its
  [provenance](../../tests/fixtures/baya-weaver-current-i420-qp152.json).
- Inventory prediction coverage by geometry, bit depth, mode, edge
  availability, MRL, and IBP. Distinguish tested, implemented, and unsupported
  combinations rather than assuming every cross-product is supported.
- Add characterization tests where a planned extraction lacks observable
  coverage. Include unsupported reconstruction statuses, truncation,
  insufficient storage, overlap, and unchanged caller output where promised.
- Cover diagnostics-enabled and diagnostics-disabled configurations. Record
  platform/runtime limitations separately from passing results.

The current evidence includes 16 focused 4x64/64x4 DC/IBP cases and exact
decoding of the retained Baya stream. The last local photographic check had
116 available references and 14 absent target-baseline references. Recheck
availability at execution time; do not describe the incomplete corpus as a
fully passing reference gate.

**Exit:** each proposed extraction has a named behavioral test, a resource
baseline, and a recorded validation command. Existing failures or missing
media are explicit; no expected output is weakened to obtain a green baseline.

## M1: extract prediction families

**Completed:** [prediction-family extraction and evidence](../dev/refactoring-m1.md).
Typed numerical owners now sit behind the existing entry points. Dispatch
precedence, legacy branches, and frozen M0 expectations remain unchanged.

Separate the responsibilities currently concentrated in
[`intra_prediction.c`](../../src/decoder/intra_prediction.c):

- Reference-edge preparation and filtering.
- Nondirectional DC, smooth, and Paeth kernels.
- Directional interpolation and its MRL/IBP handling.
- Intra-DIP and its data.
- Admission and dispatch entry points.

Keep the current dispatch precedence and every supported/unsupported decision
unchanged during these moves. Retain existing entry points as wrappers until
callers have migrated. Keep tables with their numerical owner.

Preserve typed 8-bit and 10-bit paths. Share small geometry or arithmetic
helpers only when their semantics are already identical; do not introduce
untyped sample buffers or force both paths through one macro-heavy kernel.

**Exit:** the dispatcher and numerical owners have narrow interfaces, all
characterization cases remain identical, and no new admission decision or
workspace change has occurred.

## M2: simplify prediction admission and dispatch

**Completed:** [admission consolidation and equivalence evidence](../dev/refactoring-m2.md).
Canonical families now have explicit geometry/feature rules and one dispatch
path. Observable noncanonical direct-helper behavior remains isolated rather
than being silently removed or reclassified.

Only after M1 is stable:

- Replace repeated geometry facts with the existing
  [transform geometry descriptors](../../src/decoder/tx_geometry.c) or a small
  prediction-owned predicate where appropriate.
- Keep implemented-feature restrictions explicit and separate from legal
  transform geometry. A valid descriptor is not sufficient proof that a
  prediction combination is supported.
- Characterize overlapping branches before consolidating them. Remove a
  redundant path only after proving which path currently handles its inputs.
- Consolidate repeated edge and IBP/MRL setup without changing rounding,
  clipping, reference extension, or order of operations.
- Exercise the supported matrix and representative rejected tuples, including
  4x64 and 64x4, all edge-availability combinations, and padded output strides.

Do not silently expand the feature set while deleting allowlists. Any desired
new admission requires a separate capability change and AVM parity evidence.

**Exit:** each admitted prediction family has one understandable admission
rule and dispatch path, with unchanged pixels, rejection statuses, and bounds.

## M3: separate block reconstruction from plane integration

**Completed:** [block/plane ownership and equivalence evidence](../dev/refactoring-m3.md).

Split [`reconstruct.c`](../../src/decoder/reconstruct.c) at the boundary between:

1. Reconstructing samples from a supplied predictor and coefficient block:
   lossless reconstruction, inverse-transform selection, residual composition,
   and clipping.
2. Integrating a block into a picture: reference gathering, palette predictor
   preparation, tile/readiness checks, plane writes, and readiness commits.

Keep inverse-transform kernels with their existing owners. Preserve
cross-component transform ordering and the distinction between coefficient
dimensions and output dimensions.

Extract genuinely common plane/reference helpers at their lowest coherent
owner. Keep typed sample access and arithmetic; do not replace them with a
runtime-polymorphic plane abstraction.

**Exit:** block reconstruction can be tested using supplied inputs without
tile orchestration. Plane-integration tests independently cover reference
availability, partial visible blocks, chroma formats, and readiness updates.
Scratch layouts and exact decoded output remain unchanged.

## M4: clarify frame responsibilities

Refine the ownership currently shared by
[`frame_workspace.c`](../../src/decoder/frame_workspace.c) and
[`frame_output.c`](../../src/decoder/frame_output.c):

- Keep arena planning/binding, capacities, overlap validation, and preflight
  together. Query and initialization must retain the same allocation order.
- Extract visible-plane copying/cropping into an output owner, preserving
  byte order, padding policy, and execution dispatch.
- Put deblock metadata commits and restoration skip metadata with a coherent
  filter-metadata owner rather than workspace allocation code.
- Extract repeated luma/chroma failure-context assembly into small helpers.
- Keep tile traversal, pending chroma scheduling, reconstruction commits, and
  filter ordering explicit in frame orchestration.

Do not replace the decode loop with callbacks merely to shorten it. An
internal context structure is justified only if it clarifies ownership without
hiding buffer bounds, increasing workspace, or changing lifetimes.

**Exit:** storage, metadata, output copying, and orchestration have distinct
owners. Layout, overlap, parallel-output, tile-boundary, filter-order, and
failure-path tests preserve existing behavior.

**Completed:** see [M4 ownership and validation evidence](../dev/refactoring-m4.md),
including unchanged allocation/scheduling bodies, frozen frame characterizations,
and the existing missing-media limits on aggregate gates.

## M5: separate codec syntax and container carriage

For [`carriage.c`](../../src/carriage.c):

- Move sequence-state parsing alongside the
  [decoder sequence interface](../../src/decoder/sequence.h).
- Give OBU framing/LEB128 and content-interpretation syntax explicit owners.
- Keep configuration-box handling and container-carriage validation distinct
  from parsing codec tool flags.
- Preserve existing public parsing entry points and supported HEIF/OBU shapes.
  This milestone does not add container support for previously rejected forms.

For [`container.c`](../../src/container.c):

- Separate deterministic writing from reading/probing.
- Retain shared interpretation rules between probing and parsing; do not
  duplicate the canonical container policy.
- Continue using the existing BMFF box primitives rather than introducing a
  second box parser.

Move container-probe declarations out of [`base.h`](../../src/base.h) into a
private container interface. Keep root-level public query/decode functions as
thin coordinators rather than moving codec internals into them.

**Exit:** codec sequence syntax no longer belongs to container-carriage code.
Container round-trip/determinism, dialect detection, metadata consistency, OBU
validation, offsets, error precedence, and public decode tests remain exact.

**Completed:** see [M5 ownership and validation evidence](../dev/refactoring-m5.md),
including all 57 preserved function bodies, 52,630 frozen parsing/container
scenario groups, and the unchanged missing-media limits on aggregate gates.

## M6: narrow private interfaces

Reduce the transitive dependency chain currently pulled in by
[`reconstruct.h`](../../src/decoder/reconstruct.h):
coefficient-level, transform, coefficient, leaf, and frame interfaces.

- Separate the value types needed by kernels from entropy/CDF banks, parser
  contexts, and orchestration structures.
- Define each type at its lowest coherent owner. Do not create a universal
  `types.h` or another monolithic internal header.
- Use direct includes and forward declarations where valid; do not make
  callers rely on incidental transitive includes.
- Ensure prediction and transform kernels do not depend on tile/frame
  orchestration interfaces.
- Keep shared mathematical helpers independent of an unrelated codec feature
  merely because that feature was their first caller.
- Compile affected private headers independently and check dependency
  direction. Preserve layout assertions throughout.

**Exit:** numerical owners expose only their necessary inputs, private headers
are self-contained, no include cycle is introduced, and state sizes and
freestanding linkage remain unchanged.

**Completed:** see [M6 interface and validation evidence](../dev/refactoring-m6.md),
including 52 standalone private headers, 35 value-only kernel dependency roots,
unchanged M0 inventories, and the preserved numerical/state declarations.

## M7: standardize error construction

Introduce a small private helper for repeated error-record initialization in
root-level parsing/query/decode code. Keep context-specific offsets, syntax
details, and stage interpretation local.

Build on M4's frame-context helpers instead of adding a second competing
diagnostic mechanism. Do not introduce global mutable state.

Preserve status and diagnostic behavior independently: initializing a record
must not erase an intentional detail, remap an unsupported status, or reorder
which validation failure is reported first. Any inconsistency requiring an
observable behavior change belongs in a separate fix with its own tests.

**Exit:** representative failures from container parsing, sequence/frame
syntax, luma/chroma reconstruction, filtering, and output validation retain
their status, location, stage/cause, and documented caller-buffer behavior.

**Completed:** see [M7 error-construction evidence](../dev/refactoring-m7.md),
including 43,878 frozen error scenarios and 84,144 old/new diagnostic
comparisons per configuration, preserved partial updates and buffer behavior,
and unchanged M0 layouts.

## M8: integration and completion

- Build and execute the normal native decoder on this macOS computer using
  representative retained 8-bit and 10-bit inputs.
- Record focused native parity and malformed-input results. Complete image
  corpora and cross-builds are not required for this milestone's acceptance.
- Retain WebAssembly, freestanding/no-libc and other platform results as
  additional coverage evidence, not mandatory exit conditions. Compilation is
  not runtime parity.
- Recheck 8-bit and 10-bit layouts, workspace sizes, and dependencies.
- Update source-hash-bound witness records only after the modified tests
  actually pass. Do not change golden pixels to accommodate a refactor.
- Document final ownership and dependency direction, and mark milestones
  complete with their measured evidence.
- Remove forwarding wrappers only after all callers have migrated and the
  removal is separately validated.

Directory regrouping is optional after ownership stabilizes. Do not move
tables or introduce a directory hierarchy solely to make the tree look tidy.

**Exit:** the normal macOS executable runs and matches representative retained
output, ownership/dependencies are documented, and coverage limitations are
stated without claiming unexecuted platform or full-corpus gates.

**Completed under the revised acceptance scope:** see
[M8 integration and final ownership evidence](../dev/refactoring-m8.md).
The normal hosted macOS ARM64 decoder builds and runs, with exact retained
Baya 8-bit output and a passing 10-bit WP0 oracle check. The earlier native,
WebAssembly and layout evidence remains valid. Missing media, the separate
macOS no-libc linker limitation, and unavailable platform runtimes remain
coverage notes, not M8 acceptance blockers.

## Validation cadence

After each extraction, run the smallest affected unit set, followed by the
relevant exact-parity tests. Broader coverage gates remain available:

```sh
make test
make spatial-still-admission
make test-reference-images
```

M8's revised acceptance requires local macOS execution, not a fully populated
image corpus or cross-platform execution. Incomplete broader gates must still
be reported accurately.

`make test` includes the sanitizer-backed C, corpus, reference-lock,
freestanding, and Python tests. The retained Baya regression is in
`make test-c`; it needs neither an AVM checkout nor ignored prepared media.

For prediction/reconstruction milestones, additionally run the opt-in fresh
Baya comparison when the current oracle and prepared inputs are available:

```sh
make check-avm-current \
  ORACLE_ARGS='--suite photographic-8bit --source baya-weaver --preparation le896'
```

Record the resolved oracle revision and binary hashes from each report.
Changed encoder output alone is not a decoder regression. Do not fetch a new
oracle or regenerate the pinned references as part of a move-only change.

The [spatial platform gate](../dev/spatial-still.md) and
[10-bit release evidence](../dev/10bit-wp14.md) describe additional platform
validation. Use applicable commands on supported hosts rather than claiming
every platform was exercised by a native-only run.

If locked media are absent, restore the original inputs from their recorded
provenance or report the gate as incomplete and list what was tested. Never
silently skip missing inputs, replace them with new encodes, or loosen hashes.

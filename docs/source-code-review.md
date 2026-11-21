# Source-code quality and architecture review

**Date:** 3 November 2026  
**Subject:** the current [`src/`](../src/) tree  
**Baseline:** commit `3ed64ff`, including the uncommitted Windows
frame-result copy fix present at the start of this investigation.

## 1. Executive opinion

This is a serious experimental codec implementation, not a superficial wrapper
around another decoder. Its strongest qualities are explicit memory ownership,
bounded parsing, separation of platform services from codec logic, and unusually
substantial bit-exact regression evidence.

Its architecture is coherent, but it optimizes for an unusual combination of
goals: independently maintained CC0 code, no libc in the core, caller-supplied
storage, small standalone platform executables, and incremental admission of
AV2 still-image features. Those goals explain much of what would otherwise look
unnecessarily complicated.

My principal criticism is not the choice of C or the absence of dependencies.
It is the imbalance between a carefully engineered support infrastructure and
codec paths that still contain extensive tuple-by-tuple dispatch and inherited
private-helper exceptions. The project is more disciplined about preserving
behavior than about making all of that behavior easy to explain.

**Overall judgment:** a credible, carefully tested research decoder with useful
embedding properties; not yet a general-purpose, performance-oriented, or
production-qualified image decoder. The best next work is selective
simplification and measurement, not another broad mechanical refactoring.

## 2. Scope and evidence

This was a broad source investigation, not a line-by-line proof of every
algorithm or table. I followed the public query/decode path and inspected
representative container, workspace, tile, reconstruction, prediction, entropy,
filter, output, runtime, and platform implementations. Supporting tests,
build rules, capability documentation, and provenance records were consulted
where necessary to interpret the code.

The tree contains:

| Kind | Files | Physical lines, including comments and blanks |
| --- | ---: | ---: |
| C implementations | 76 | 33,911 |
| Headers | 67 | 4,569 |
| Include fragments | 43 | 23,402 |
| Assembly | 4 | Not included in the line totals above |
| Windows import definition | 1 | Not included in the line totals above |

Include fragments contain both generated numerical data and executable typed
kernels. They must not all be counted as either handwritten logic or passive
tables. The three largest table fragments alone account for approximately
16,000 lines: [predefined matrices](../src/decoder/predefined_qmatrix.inc),
[coefficient CDF data](../src/decoder/coeff_cdf_all.inc), and
[GDF data](../src/decoder/gdf_data.inc).

Evidence checked during this investigation:

- The private-header checker passed: **55 standalone headers, 35 value-only
  kernel roots, both diagnostic configurations, and no include cycles**.
- Current Windows ARM64 stack frames were measured with Clang 22.1.8 at `-O2`,
  using the freestanding include configuration.
- The immediately preceding Windows validation in this session demonstrated
  both ARM64 and x86-64 builds, 117 exact-output corpus checks per target,
  filter and preview checks, and frame/workspace/public-decode unit checks.
  That evidence is recorded in the
  [architecture document](improve-architecture.md#windows-verification-after-the-a1-refactors).
  It was not a new full test-suite run for this report.

The photographic 102-stream reference gallery was not available locally.
Windows x86-64 runtime checks used ARM64 emulation, not a physical x86-64 host.
No new Linux, macOS, WebAssembly, throughput, or peak-memory benchmark was run
for this report. This is not a security certification or an exhaustive
conformance assessment.

## 3. Architectural map

The main responsibilities are recognizable:

| Layer | Representative owners | Assessment |
| --- | --- | --- |
| Checked arithmetic, spans, readers, arenas | [base implementation](../src/base.c), [base types](../src/base.h) | Small reusable substrate; appropriate foundation for a binary parser. |
| Container and bitstream carriage | [BMFF](../src/bmff.c), [container reader](../src/container.c), [container writer](../src/container_write.c), [carriage](../src/carriage.c) | Container validation is distinct from pixel reconstruction. |
| Public orchestration | [query](../src/query.c), [decode](../src/decode.c), [public API](../include/av2heif.h) | Clear caller-buffer contract, but expensive repeated preparation. |
| Syntax and entropy | [sequence](../src/decoder/sequence.c), [frame](../src/decoder/frame.c), [tile](../src/decoder/tile.c), [range decoder](../src/decoder/range.c) | Explicit state rather than a hidden global decoder. |
| Geometry and samples | [transform geometry](../src/decoder/tx_geometry.c), [reconstruction](../src/decoder/reconstruct.c), [inverse transforms](../src/decoder/inverse_transform.c) | Useful ownership boundaries; substantial local complexity remains. |
| Frame coordination and storage | [coordinator](../src/decoder/frame_output.c), [workspace](../src/decoder/frame_workspace.c), [metadata](../src/decoder/frame_filter_metadata.c) | Large state now lives in caller-owned storage; control flow still has temporal coupling. |
| Filtering and output | [filter orchestration](../src/decoder/filter.c), [visible-plane copy](../src/decoder/frame_copy.c), [output layout](../src/output_layout.c) | Filter order and byte/sample distinctions are explicit. |
| Applications and operating system | [shared CLI](../src/shared/cli_common.c), [platform adapters](../src/platform/), [runtime](../src/runtime/), [architecture support](../src/arch/) | Good separation from numerical kernels, with a significant maintenance bill. |

The public path is approximately:

```text
query
  -> identify canonical container
  -> validate container and metadata
  -> prepare sequence, frame, matrices, and tile information
  -> calculate output layout and workspace requirement
  -> bind information to the input

decode
  -> query again and compare the supplied information
  -> validate storage sizes, alignment, and separation
  -> prepare again and bind workspace
  -> decode/reconstruct tiles
  -> apply filters
  -> copy visible pixels into the promised output representation
```

An important qualification: the [CLI](../src/shared/cli_common.c#L76) takes
raw OBU streams through private preparation and decoding interfaces. It is not
simply a thin invocation of the canonical HEIF public API. The paths share
codec machinery, but have different input contracts and orchestration.
Successful CLI corpus decoding therefore does not, by itself, prove the public
container API works.

## 4. Choice: freestanding C and a locally owned support substrate

**Observed.** The core uses checked arithmetic, local memory primitives,
minimal standard-type headers, and no heap-allocation or file-I/O calls in the
decoder implementation files searched. Windows uses a
[minimal Win32 declaration set](../src/platform/windows/win32_min.h);
the no-libc application uses
[direct operating-system interfaces](../src/platform/nolibc/decoder.c).
The [build](../Makefile) also selects specialized linking arrangements.

**Upside.** This makes the core usable where libc, an allocator, or a full
application runtime is unavailable or undesirable. Dependency and licensing
boundaries are unusually explicit. The
[provenance record](../provenance/cc0-sources.md) is a real strength: copied
support code has identified origins and adaptation boundaries.

**Downside.** Dependency-free does not mean maintenance-free. The project owns
type assumptions, startup behavior, ABI declarations, file operations, memory
primitives, and some concurrency machinery. Compiler-generated runtime calls
remain possible: the Windows x86-64 structure-copy link failure found earlier
in this session is a concrete example.

The implementation should be described as freestanding on its supported
toolchains, not as universally portable to any C11 compiler. The
[type shim](../src/shared/stdint.h), compiler builtins, GNU-style atomics and
assembly, and Windows attributes make the distinction important.

**Alternatives.**

- Keep the freestanding core but make conventional libc-backed application
  builds the easiest development path. This reduces application maintenance
  and improves tool support without introducing allocation into codec logic;
  it gives up some standalone-size and import-surface goals.
- Use a small, well-defined compiler-runtime shim for generated memory calls.
  This centralizes compiler accommodation, but becomes another target-specific
  component to test.
- Rely entirely on hosted libraries. This is simpler for a desktop application,
  but abandons a meaningful project requirement rather than merely cleaning up
  implementation style.

**Opinion:** retain the freestanding core. Treat no-libc application plumbing
and custom linking as explicit product features with an ongoing cost, not as
automatic improvements over conventional platform libraries.

## 5. Choice: caller-owned arena storage rather than internal allocation

The [public API](../include/av2heif.h#L287) reports output and workspace sizes.
The [workspace binder](../src/decoder/frame_workspace.c#L109) allocates typed
regions from one caller-provided arena. The same allocation sequence can run in
sizing mode or binding mode.

This is a good design for predictable ownership and cleanup. It avoids many
small allocations, makes allocation failures explicit before the main decode,
and supports applications with their own memory policies.

There are three qualifications:

1. **Allocation-free is not small-memory.** Coded planes, filter stages,
   readiness maps, entropy state, and reconstruction scratch remain substantial.
   The [default limits](../src/query.c#L50) permit a workspace up to 1 GiB.
   That is a ceiling, not a claim that normal images consume 1 GiB, but it is
   not a low-memory embedded default either.
2. **The reported workspace size is conservative.** In
   [arena sizing mode](../src/base.c#L525), every allocation reserves the
   maximum possible alignment padding. Binding uses actual padding. The
   reported amount is a deterministic sufficient requirement, not necessarily
   the mathematical minimum for an already aligned buffer.
3. **State relocation is incomplete.** Allocation-free APIs still use stack
   space, and some entry points use a great deal of it.

Measured per-function frames on Windows ARM64:

| Function | Frame bytes |
| --- | ---: |
| [Public query](../src/query.c#L128) | 16,112 |
| [Public decode](../src/decode.c#L224) | 17,712 |
| [CLI decode](../src/shared/cli_common.c#L182) | 18,384 |
| [Frame coordinator](../src/decoder/frame_output.c#L648) | 2,672 |

These are compiler-reported frames, not measured whole-call-stack peaks.
Because public decode calls query, those first two frames can coexist: their
sum alone is 33,824 bytes, before deeper callees and call overhead.

**Alternatives.** A prepared caller-owned context could retain parsed state
and move more large objects out of stack frames. That improves repeated
decoding and small-stack hosting, but requires a clear lifetime/API contract.
Individual dynamic allocations simplify some local code but complicate cleanup,
fragmentation, and embedding. Region reuse based on explicit stage lifetimes
can reduce peak memory, but is harder to reason about than today's separate
buffers.

**Opinion:** the arena is a sound choice. Finish reducing public-entry stack
frames and publish representative peak-memory figures before describing the
decoder as lightweight.

## 6. Choice: defensive revalidation instead of a prepared decode handle

The [decode entry point](../src/decode.c#L224) reruns query, compares layouts,
sizes, pointers, and an input fingerprint, and then prepares decoding again.
The [query implementation](../src/query.c#L128) also repeats some container work
performed during dialect detection.

For the normal successful `query -> decode` workflow, preparation is reached
three times: the caller's query, decode's fresh query, and decode's explicit
preparation. The input fingerprint is calculated during both queries.

**Upside.** A caller cannot simply edit a public layout structure and have it
trusted. Input changes and stale information are checked. The public contract
does not expose a long-lived internal decoder object.

**Downside.** Query is not a cheap metadata peek. It performs codec preparation,
and decode repeats work whose results were just available. This is structurally
redundant; its actual contribution to end-to-end time was not benchmarked here.

The [fingerprint](../src/query.c#L15) is FNV-style, not cryptographic. It is an
input-binding/correctness aid, not authentication, and does not replace the
requirement to keep input storage valid and unchanged while decoding.

**Alternative:** add a prepared context with explicit ownership of either
immutable input references or a private input copy. Validate untrusted public
parameters at the boundary, then reuse the prepared state internally.
This removes repeated parsing but creates lifetime, invalidation, and ABI
questions. An internal shared preparation path is a smaller first step;
arbitrarily removing checks is not.

**Opinion:** this is one of the clearest worthwhile improvements. Preserve
public-boundary validation while eliminating duplicated preparation.

## 7. Choice: a bounded profile rather than a general HEIF/AV2 implementation

The [container parser](../src/container.c#L108) enforces a restricted shape:
for example, ordinary eight-byte box headers, limited property storage, and
specific item/property relationships. The
[capability contract](capabilities.md) explicitly excludes many otherwise
plausible image/video features.

Fixed capacities are also visible in [tile state](../src/decoder/tile.h):
partition-node arrays, tile spans, coefficient contexts, and a bounded deferred
chroma queue. [Partition traversal](../src/decoder/partition.c#L764) uses an
explicit checked stack rather than uncontrolled C recursion.

**Upside.** Resource usage and rejection behavior are easier to bound. The
project does not need to implement all of HEIF or temporal video to support its
stated still-image experiment.

**Downside.** A valid file in a wider ecosystem can be rejected. Public limits
do not make hard implementation capacities dynamically extensible. Having a
tool present in the code does not establish support for all combinations with
other tools.

**Alternatives.** A generic container library and growable parser structures
offer broader interoperability, at the cost of dependencies, allocation, and
additional behavior to qualify. A retained bounded core with a richer external
container adapter is a reasonable compromise.

**Opinion:** the narrow profile is legitimate. Keep its exact boundaries
visible to callers and tests; do not describe it merely as "an AV2 decoder"
without the still-image and tuple restrictions.

## 8. Choice: incremental tuple dispatch and retained private compatibility

This is the most conspicuous maintainability concern.

The [secondary-transform implementation](../src/decoder/secondary_transform.c#L360)
contains a long dispatch chain over mode, dimensions, transform type, set, and
symbol. Some data fragments retain fixture-oriented names such as
[palace transform data](../src/decoder/ist_palace_data.inc).
The [prediction dispatcher](../src/decoder/intra_prediction.c#L23) explicitly
preserves special private-helper behavior outside canonical bitstream admission.

These are not evidence of image-name detection or hardcoded output images:
the inspected dispatch is based on coding parameters. Nevertheless, the shape
of the implementation shows the maintenance cost of incremental feature
admission.

**Upside.** A newly admitted tuple can be isolated, compared with an oracle,
and enabled without asserting correctness for an untested cross-product.
Preserving private helper behavior also reduces refactor risk.

**Downside.** First-match chains become difficult to audit for overlapping,
shadowed, or missing cases. Readers must distinguish normative rules,
supported-profile rules, and historical compatibility exceptions. Tests can
accidentally turn incidental private behavior into a permanent obligation.
Moving branches into smaller files does not remove this conceptual complexity.

**Alternatives.**

- Use descriptor tables for genuinely data-driven selection, with duplicate-key,
  ordering, and coverage checks. This makes supported tuples inspectable, but
  can merely hide complexity if exceptions still need arbitrary code.
- Separate admission predicates from general numerical execution. This gives
  a clearer unsupported boundary, but generalizing a kernel requires evidence
  beyond the currently retained streams.
- Define and deliberately revise private-helper contracts instead of preserving
  every historical invalid-input success. This simplifies the implementation,
  but should be a separately reviewed behavior change, not disguised as cleanup.

**Opinion:** preserve proven numerical behavior, but replace accumulated
exceptions with an explicit admission model where possible. Do not equate
private compatibility with public format compatibility.

## 9. Choice: typed include kernels for byte and wide samples

Files such as [sample operations](../src/decoder/internal/sample_ops.h)
instantiate the same executable include twice using small
[byte](../src/decoder/internal/sample_kernel_u8.inc) and
[wide](../src/decoder/internal/sample_kernel_u16.inc) macro environments.

**Upside.** This is a legitimate C technique: type-specialized implementations
without runtime sample-width branches in inner loops, and without maintaining
two handwritten copies of identical arithmetic. The reset include makes macro
scope intentional.

**Downside.** Navigation and compiler diagnostics are less straightforward.
Some generated signatures are hard to read, as in the
[frame-copy kernel](../src/decoder/internal/frame_copy_kernel.inc#L29).
The [source-consistency tests](../tests/test_source_consistency.py#L36)
need special handling for this arrangement.

Importantly, shared kernel text does **not** make feature policy identical.
The [wide filter path](../src/decoder/filter.c#L202) rejects restoration, and
the [coordinator](../src/decoder/frame_output.c#L486) retains different
byte/wide commit sequencing.

**Alternatives.** Explicit functions are easier to debug but risk duplication;
generated C improves ordinary tooling but adds generator/reproducibility work;
one runtime-generic implementation is simpler structurally but can introduce
branches and weaken type clarity. C++ templates provide language-level
specialization, but changing the implementation language is not a small or
necessary fix.

**Opinion:** keep the mechanism for small, truly equivalent kernels. Avoid
forcing complicated policy or structurally different signatures through it.
Converge byte/wide behavior separately, using explicit capability tests.

## 10. Numerical implementation and table ownership

The [sample-math helpers](../src/decoder/internal/sample_math.h#L129) make
different signed rounding rules explicit. Checked dimensions and byte/sample
units are visible throughout the layout and workspace code. The
[wide output copy](../src/decoder/internal/frame_copy_kernel.inc#L12) writes
little-endian bytes explicitly rather than assuming host byte order.
These are good decisions for bit-exact codec work.

Large numerical tables are not inherently poor-quality code. The
[GDF table](../src/decoder/gdf_data.inc#L1) identifies hash-locked normative
attachments; the [matrix table](../src/decoder/predefined_qmatrix.inc#L1)
identifies its generator. Keeping reproducible constants separate from control
flow is preferable to a runtime dependency on an oracle decoder.

The [inverse-transform code](../src/decoder/inverse_transform.c#L772) also uses
straightforward scalar matrix products. Their benefit is traceability.
Their arithmetic complexity and table access make them candidates for future
optimization, but this review did not establish them as measured bottlenecks.

**Alternatives.** Factored integer transforms, sparse/DC-only fast paths, and
target-specific SIMD can reduce work. Each introduces new rounding, overflow,
dispatch, and cross-target testing obligations. Retaining a simple scalar
reference implementation while adding selected fast paths is safer than
replacing every kernel simultaneously.

**Opinion:** prioritize exact arithmetic over fashionable abstractions.
Optimize measured hot paths, with intermediate-state checks as well as final
image comparisons. Do not regenerate expected outputs simply to accommodate
an arithmetic rewrite.

## 11. Frame orchestration, filters, and parallelism

The [frame coordinator](../src/decoder/frame_output.c) now has meaningful
helpers for parsing, reconstruction, deferred chroma, completeness, filtering,
and output. The [tile structures](../src/decoder/tile.h#L122) expose phases and
events rather than relying solely on one huge procedural loop.

Nevertheless, parsing, reconstructing, marking readiness, committing entropy
context, and draining deferred chroma form a temporal protocol. Correct call
ordering is an architectural invariant, not merely a local implementation
detail. The shared active scratch is safe because the current orchestration
is serial; it cannot simply be shared among concurrent tiles.

The [filter implementation](../src/decoder/filter.c#L146) favors separate
intermediate planes and explicit stage order. This improves reasoning about
neighborhood dependencies and restoration baselines, but causes full-plane
copies and substantial memory traffic.

**Alternative:** stage-lifetime analysis, buffer swapping, and eventually
stripe/tile execution can save memory or increase throughput. The downside is
more complicated halo, boundary, baseline-preservation, and aliasing rules.
Changes need filter-specific exact-output tests, not just fewer buffers.

The [execution interface](../src/runtime/execution.h) is a good dependency
inversion: codec helpers can receive a dispatcher instead of owning threads.
However:

- The [public decoder](../src/decode.c#L280) passes no execution dispatcher.
- The [copy dispatcher](../src/decoder/frame_copy.c#L62) parallelizes sufficiently
  large final row copies, not the serial entropy/reconstruction pipeline.
- The [task pool](../src/runtime/concurrency.c) has a specialized
  [Linux x86-64 backend](../src/runtime/concurrency_linux_x86_64.c), while other
  platforms can use [serial execution](../src/runtime/concurrency_serial.c).
- Worker startup failure can fall back to serial operation successfully.
  That supports availability, but callers need to inspect actual width when
  diagnosing performance.

**Opinion:** the abstraction is ahead of the parallel work available through
the public API. An external application-provided executor is an attractive
default; native thread backends remain useful for no-libc applications.
Adding threads before resolving state ownership and filter dependencies would
increase risk more than it guarantees speed.

## 12. Errors, previews, and application behavior

Status values distinguish malformed, truncated, unsupported, resource, and
argument failures. [Shared error construction](../src/error.h) and
[decode-stage translation](../src/decode.c#L9) provide considerably more
diagnostic structure than a single failure code.

The trade-off is a large amount of diagnostic state and mapping code, including
small numeric reason codes whose meaning is not always evident at the call
site. The rich [frame result](../src/decoder/frame_output.h#L53) is valuable
during codec investigation, but should not force numerical kernels to own
application presentation policy.

The [shared CLI](../src/shared/cli_common.c) cleanly abstracts allocation and
file operations. It validates output choices before opening a destination,
which is a useful behavior. Its separate raw-stream preparation path still
needs parity and policy tests against the public API where their responsibilities
overlap.

Two deliberately limited output choices deserve clear labeling:

- [Preview conversion](../src/preview.c#L73) accepts limited-range,
  matrix-coefficients-6 conversion, and the CLI supplies those constants.
  This is a constrained preview, not general metadata-aware color management.
  Raw sample parity does not establish correct appearance for every color
  description.
- The [PNG writer](../src/shared/png_writer.c#L85) uses stored DEFLATE blocks
  and filter type zero. That gives deterministic, dependency-free streaming
  output with a row buffer, but files are approximately raw RGB size rather
  than efficiently compressed PNG.

**Alternatives.** A hosted color-conversion/compression adapter would improve
display fidelity and file size without burdening the freestanding decoder.
The price is dependencies and another application configuration to qualify.
More compression should be optional if diagnostic simplicity is the priority.

**Opinion:** these output choices are reasonable for inspection tools.
They should not be mistaken for a full image-export pipeline.

## 13. Testing quality and its limits

The strongest evidence is exact output against independently produced reference
samples, supported by low-level invariants and malformed-input checks.
The [frame characterization tests](../tests/frame_refactor_unit.c#L454)
also exercise mutated input, status/stage fingerprints, and output preservation.
This is much stronger than checking whether a program exits successfully or
whether an image looks plausible.

The [header tests](../tests/test_private_headers.py) enforce real dependency
boundaries. That is more useful than merely naming a directory "internal."
Formatting, naming, and source-shape checks provide additional consistency.

There are limits:

1. Exact outputs on retained files establish those cases, not the entire
   cross-product of coding tools.
2. Golden hashes can preserve an old defect as easily as correct behavior if
   the oracle or reason for the expectation is not understood.
3. Source-shape rules, line budgets, and source-hash-bound witnesses can make
   legitimate refactoring expensive. They are guards, not semantic proofs.
4. Locally absent media makes the full evidence set less convenient to reproduce.
5. The preceding source-consistency run passed 22 of 23 tests; the remaining
   [missing-formatter assertion](../tests/test_source_consistency.py#L214)
   depends on an executable name appearing in an OS error message and fails
   on this Windows environment. This is a test portability problem, not an
   observed pixel-decoding failure.

**Opinion:** keep the exact-output, poisoning, and dependency tests.
Distinguish public compatibility tests from characterization of incidental
private behavior. Package a practical, always-available regression subset and
make larger-corpus absence an explicit skipped or failed gate.

## 14. Recommended priorities

These are engineering priorities, not vulnerability severities.

| Priority | Recommendation | Benefit | Main cost or caution |
| --- | --- | --- | --- |
| 1 | Retain cross-target freestanding link/import/symbol checks after ordinary source changes. | Catches generated runtime dependencies such as the observed x86-64 copy regression. | Cross-compilation alone does not replace runtime parity. |
| 1 | Reduce public query/decode/CLI stack frames and remove repeated preparation through a defined internal or public prepared-state contract. | Addresses measured stack size and demonstrated redundant work. | Input lifetime, invalidation, and ABI design must be explicit. |
| 1 | Make supported tuples and private compatibility exceptions independently inspectable. | Reduces dispatch ambiguity without pretending to support untested combinations. | Requires characterization and admission tests before simplification. |
| 2 | Measure peak workspace and stage timings for small, photographic, tiled, and 10-bit inputs. | Separates important costs from aesthetically unusual code. | Needs representative workloads and reproducible target configurations. |
| 2 | Reduce filter copies using documented stage lifetimes. | Potential memory-bandwidth and peak-storage savings. | Preserve neighborhoods, restoration baselines, and tile boundaries. |
| 2 | Optimize entropy refill and trusted-state validation only after profiling. | The [range decoder](../src/decoder/range.c#L22) reads input bit by bit and validates CDF ordering per symbol; [descriptor lookup](../src/decoder/tx_geometry.c#L126) also rechecks immutable table relationships. | Keep validation at trust boundaries and retain checked/reference paths. |
| 2 | Treat byte/wide feature convergence as behavior work, not deduplication. | Avoids policy drift behind shared typed kernels. | Do not broaden public admission simply because a helper accepts a domain. |
| 3 | Prefer application-supplied execution and optional hosted export adapters where appropriate. | Contains OS/runtime and color/compression maintenance. | Preserve no-libc applications for users who actually need them. |
| 3 | Repair OS-dependent test diagnostics and prune purely syntactic constraints that no longer protect an invariant. | Improves portability and lowers refactoring friction. | Retain genuine layering, macro-scope, and equivalence guards. |

I would **not** prioritize a language rewrite, a generic plugin framework, or
splitting every large source file into smaller files. None directly addresses
the principal costs observed here. Likewise, importing an entire reference
decoder would alter licensing, dependency, and ownership objectives rather than
constitute an uncomplicated implementation improvement.

## 15. Final assessment

Several unusual choices are justified: caller-owned arenas, explicit bounded
state, integer arithmetic, generated normative data, typed C kernels, and a
narrow supported format. They serve identifiable requirements.

Other choices need continued justification through measurements and clearer
contracts: repeated full preparation, large public stack frames, extensive
special-case dispatch, preservation of noncanonical private behavior, and
locally maintained platform runtime machinery.

The code is neither an unstructured prototype nor a finished general decoder.
It is a disciplined research implementation carrying visible historical
complexity. Preserve its strong ownership and equivalence evidence; spend the
next simplification effort on reducing the number of rules a maintainer must
remember, not merely the number of lines in each file.

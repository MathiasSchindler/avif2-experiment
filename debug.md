# Decoder Fixture and Workaround Removal Ledger

This file tracks source paths that are tailored to observed corpus files or
exact syntax tuples instead of implementing one general AV2 rule. The goal is
to remove each entry only after the normative path produces the same result.

The corpus is currently 21/21 byte-exact against the pinned AVM reference.
That is the baseline to preserve, not evidence that these branches are
general. Block coordinates below are in 4x4 units unless stated otherwise.

## Status and priority

- `active/output`: reachable and directly changes reconstructed pixels.
- `active/parse`: reachable and changes entropy parsing, CDF selection, or
  accepted syntax.
- `active/admission`: reachable and changes whether a syntax tuple is
  supported.
- `mixed`: contains an active clause plus ineffective or shadowed clauses.
- `diagnostic`: records or emits data but does not intentionally affect pixels.
- `dead`: unreachable after an unconditional return.
- Priority `P0` is the first correctness target; `P3` is cleanup only.

## Removal queue

| ID | Priority | Status | Fixture or trigger | Location | Current behavior | General replacement and definition of done |
| --- | --- | --- | --- | --- | --- | --- |
| F-01 | P0 | complete (2026-08-03) | `emerald-192x96-qp208.obu`; five exact luma blocks | General path in [`av2h_luma_reconstruct_to_plane`](src/decoder/reconstruct.c#L6940) | Removed the five exact transform/mode/angle gates, the AVM-curated predictor arrays, their include, and the data file. The existing general reference gathering and D135/D203/Smooth-Horizontal prediction path already reproduces the required output. | Focused Emerald Q208 generation and byte comparison pass without the oracle; Emerald Q128 generation and comparison also pass. No source reference to the removed include, arrays, or `pinned_predictor` remains. |
| F-02 | P0 | complete (2026-08-03) | `schwaebisch-417x277-qp208.obu`, luma CCSO unit at leaf `(64,0)`, plane Y | General [`ccso_context`](src/decoder/leaf.c#L1025) call in `read_ccso_entropy` | Removed the exact QP/dimensions/leaf/plane assignment. At unit column zero there is no left control, and row 64 is a superblock boundary for every supported 64/128/256 superblock size, so the general availability rule excludes the above control and derives context `0`. | Schwaebisch Q208 emits the locked 173,611-byte I420 with SHA-256 `6fbc5758a05d727ed05b04542de796cc72ca5dd9c402213c3e2d4ec72ae78428`; all 40 CTest cases pass and no Q208/417x277 condition remains in `leaf.c`. |
| F-03 | P0 | complete (2026-08-03) | `emerald-480x240-qp128.obu` and `silene-479x383-qp176.obu`; chroma 64x64 leaf, skip context 6 | General descriptor branch in [`av2h_chroma_coeff_prefix_read`](src/decoder/coeff.c#L1102) | Removed the exact QP/dimension branch, both adjusted skip banks, both adjusted EOB banks, their initialization, and their CDF storage. The branch was unreachable: a 64x64 leaf always yields a valid chroma 32x32 descriptor with transform context 3, and skip context 6 is accepted by the preceding complete-bank path for both planes. | Emerald Q128 and Silene Q176 generation and byte comparisons pass through the complete Q-context/plane/transform/context banks. `Av2hFirstCoeffCdfs` shrank from 2768 to 2672 bytes and no adjusted-bank symbol remains in production. |
| F-04 | P1 | complete (2026-08-03) | Emerald Q208 192x96, Schwaebisch Q208 417x277, Chimp Q192 240x287, Chinnathurai Q224 333x333, and Louis Q224 463x342 | General descriptor branch in [`av2h_chroma_coeff_prefix_read`](src/decoder/coeff.c#L1012) | Removed the exact QP/dimension whitelist, transform-size sub-whitelists, private Q3 U/V skip banks, six private Q3 EOB banks, their initialization, and their struct storage. Every legal tuple was already consumed by the preceding descriptor-driven complete-bank path; the fixture branch was unreachable. | All nine affected generation/comparison checks pass, as does the strict coefficient unit. `Av2hFirstCoeffCdfs` shrank from 2672 to 2282 bytes, and no private `q3_chroma_*` symbol remains. |
| F-05 | P1 | complete (2026-08-03) | Luma prefix with `prefix->tx_size == AV2H_TX_INVALID`; exact profiles for Emerald Q208, Schwaebisch Q208, Laarim Q96, Chimp Q192, Chinnathurai Q224, and Louis Q224, with many exact modes and coordinates | General [`derive_tx_size`](src/decoder/coeff.c#L311) path in `av2h_coeff_prefix_read` | Removed the invalid-transform fixture disjunction, its following transform-size whitelist, and the duplicate geometry helper. `derive_tx_size()` already returns failure unless `av2h_luma_tx_descriptor_get()` provides a supported descriptor, so `AV2H_TX_INVALID` is unreachable after success; this includes the previously suspected Laarim clause. | The strict coefficient unit passes, Laarim Q96 generation remains byte-exact, all 40 CTest cases pass, and no F-05 fixture dimensions or invalid-transform gate remain in `coeff.c`. |
| F-06 | P1 | complete (2026-08-03) | Any QP `<=90` 64x64 chroma leaf | General descriptor branch in [`av2h_chroma_coeff_prefix_read`](src/decoder/coeff.c#L750) | Removed the low-Q 64x64 shortcut and its private U/V skip and EOB banks. A 64x64 chroma leaf always yields a valid TX_32X32/context-3 descriptor, and every admitted U/V skip context was already accepted by the preceding complete Q-context banks, so the shortcut was unreachable. | The strict coefficient unit and all nine configured Q48/Q64/Q80 tests pass, including every available exact comparison/hash. All 40 CTest cases pass, `Av2hFirstCoeffCdfs` shrank from 2282 to 2078 bytes, and no private `q0_chroma_*` bank remains. |
| F-07 | P1 | complete (2026-08-03) | Any QP 128 chroma 8x8/8x16 leaf | General descriptor branch in [`av2h_chroma_coeff_prefix_read`](src/decoder/coeff.c#L700) | Removed the Q128 geometry shortcut and its private skip/EOB CDF members. `av2h_chroma_tx_descriptor_get()` already maps 8x8 to TX_4X4/context 0 and 8x16 to TX_4X8/context 1, and every admitted U/V skip context was accepted by the preceding complete Q-context banks, so the shortcut was unreachable. | Focused descriptor assertions pin both mappings. The strict coefficient unit, all three configured Q128 tests, and all 40 CTest cases pass; `Av2hFirstCoeffCdfs` shrank from 2078 to 1964 bytes and no private F-07 CDF symbol or Q128 branch remains. |
| F-08 | P1 | complete (2026-08-03) | Any QP 128 luma leaf; Emerald Q208 192x96 block-size list | Descriptor-based [`luma_geometry_supported`](src/decoder/tile.c#L232) | Replaced the unconditional Q128 admission, Emerald Q208 private eight-size whitelist, duplicated fallback switch, and GDF-mode validation bypass with one unconditional `av2h_luma_tx_descriptor_get()` success predicate. Transform geometry now has one authority and oversized partition blocks fail closed before mode parsing. | The coefficient unit exhaustively checks all 29 block enums: 23 legal leaf sizes map to their expected transforms and six 128/256 classes fail, with `AV2H_BLOCK_INVALID` also rejected. Emerald Q208/Q128 exact comparisons, Chalk Q128, the strict coefficient unit, and all 40 CTest cases pass; no F-08 QP, dimension, or GDF condition remains in `tile.c`. |
| F-09 | P2 | complete (2026-08-03) | First luma leaf only: exact 64x16, DC, IBP, all-zero DCT_DCT tuple | General zero-reference DC branch in [`av2h_luma_reconstruct_to_plane`](src/decoder/reconstruct.c) | Removed the exact first-leaf early return and its duplicate predict, reconstruct, commit, and reconstructed-map logic. Normal reference gathering derives empty top and left edges at `(0,0)`, fills the DC predictor with 128, and reaches the shared reconstruction and commit path; this API is downstream of QP. | Focused synthetic tests cover 8x8 and 64x16 first-leaf DC/IBP blocks, including predictor samples, committed pixels, and reconstructed-map coverage. Schwaebisch Q208 still emits the locked 173,611-byte I420 with SHA-256 `6fbc5758a05d727ed05b04542de796cc72ca5dd9c402213c3e2d4ec72ae78428`; all 40 CTest cases pass and no first-leaf/coordinate gate or duplicate plane path remains in production. |

## Dead fixture code

| ID | Status | Fixture-shaped code | Location | Cleanup criterion |
| --- | --- | --- | --- | --- |
| D-01 | complete (2026-08-03) | Hundreds of QP/dimension/EOB/transform gates for Laarim, Chimp, Pastell, Schwaebisch and other corpus profiles | Generic [`q_level_profile_supported`](src/decoder/coeff_level.c) predicate | Deleted the unreachable profile tail after the unconditional generic return. Coefficient-level admission now depends only on the matching Q context and a supported transform geometry; transform type, EOB, frame dimensions, and fixture identity do not select this gate. The strict ASan/UBSan coefficient-level unit and all 40 CTest cases pass with no disabled fallback left in production. |
| D-02 | complete (2026-08-03) | Q192/Q208/Q224 clauses inside the invalid luma-prefix whitelist | Removed with F-05 | The shadowed clauses and the entire unreachable invalid-transform gate were deleted. |

## Diagnostic residue

These entries are lower risk because they should not select decoded values.
They still couple production code to individual debugging sessions and can
hide accidental dependencies, so remove them after the behavioral queue or
put diagnostics behind one explicit compile-time tracing interface.

| ID | Status | Fixture or probe | Location | Current behavior | Cleanup criterion |
| --- | --- | --- | --- | --- | --- |
| G-01 | diagnostic | `chimp-93x111-qp064.obu` by exact dimensions | [`decode_frame`](src/platform/windows/decoder.c#L716) | Every successful 93x111 decode writes four fixed files under `build/`: reconstruction, deblocked, CDEF, and CCSO I420 stages. | Remove automatic writes from the production CLI or require an explicit trace option with caller-selected paths. |
| G-02 | diagnostic | Chimp-named luma/chroma target arrays and exact target positions | [`reconstruct.c`](src/decoder/reconstruct.c#L7463) | Captures predictor, residual, output, coefficients, and entropy metadata in global arrays; the Windows CLI prints them. | Replace with an optional structured trace sink or delete after the corresponding tests assert the values directly. |
| G-03 | diagnostic | Chimp second-superblock probe | [`tile.c`](src/decoder/tile.c#L654) | Captures the first leaf at/after MI column 16 and later mode/range fields in `av2h_debug_chimp_second_sb`. | Remove the global and CLI printing, or migrate the event to the trace sink. |
| G-04 | diagnostic | CCSO leaf `(64,0)`, plane V | [`leaf.c`](src/decoder/leaf.c#L1150) | Snapshots context, range, CDF, and decoded value. This branch does not override the context. | The production override is gone; move any still-useful context/range/value assertions into a focused test, then delete this probe. |
| G-05 | diagnostic | Coefficient-prefix and coefficient-level targets, including `av2h_debug_chimp_*` globals | [`coeff.c`](src/decoder/coeff.c#L6) and [`coeff_level.c`](src/decoder/coeff_level.c#L3) | Stores mutable CDF, range, EOB, sign, base-level, and FSC snapshots in process-global buffers. | Move required assertions into unit tests; then delete globals, extern declarations, writes, and CLI dumps together. |
| G-06 | diagnostic | Exact-coordinate mode and partition probes | [`leaf.c`](src/decoder/leaf.c#L1896), [`partition.c`](src/decoder/partition.c#L3), and [`tile.c`](src/decoder/tile.c#L10) | Records range/mode/partition state at selected leaves and stages. | Consolidate useful events behind a disabled-by-default trace API; otherwise delete them after parity debugging. |
| G-07 | diagnostic | Filter, transform, directional-prediction, and MHCCP stage globals | [`filter.c`](src/decoder/filter.c#L3), [`transform.c`](src/decoder/transform.c#L3), and [`reconstruct.c`](src/decoder/reconstruct.c#L2018) | Records hashes, plane pointers, stages, edge samples, and MHCCP traces. Most are generic rather than fixture-keyed, but belong to the same production debug surface. | Keep only data required by hosted tests behind a test/trace interface; production decoding must not expose writable debug globals. |

## Removal protocol

For each `F-*` item:

1. Capture the AVM and local state immediately before the workaround: entropy
   tell/range/CDF for parsing issues, or references/predictor for reconstruction.
2. Add a focused test for the normative rule. Include at least one synthetic
   or unrelated-size case so the replacement cannot recognize the fixture.
3. Implement the general rule and remove the complete fixture condition in
   the same change. Do not leave a disabled fallback in production code.
4. Run the focused unit test, the affected artifact comparison, all hosted
   artifact comparisons, and the complete CTest suite.
5. Search `src/` for the removed dimensions, QP tuple, debug symbol, and
   fixture name. Mark the row complete only when no production dependency
   remains.

Suggested remaining order: G-01 through G-07.

## Baseline validation

The minimum regression bar after every removal is:

```powershell
cmake --preset windows-x86_64
cmake --build --preset windows-x86_64
ctest --test-dir build\windows-x86_64 --output-on-failure
```

For output-affecting changes, regenerate the affected local I420 and compare
it byte-for-byte with its locked `images/artifacts/*-decoded.i420` reference.
The final cleanup target is zero fixture names, exact frame-dimension gates,
or corpus-coordinate overrides in the production decoder path.
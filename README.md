# AV2-HEIF experiment

This repository explores a project-defined AV2 still-image carriage in HEIF.
It does not define or claim a standardized AVIF successor.

The implemented core is CC0, freestanding C11, allocation-free, and does not
depend on libc or vendored codecs. Hosted AVM tooling is used only for the
separate reference experiment and is fetched into ignored `vendor/` and
`build/` paths.

## Current status

- Milestone 0 proves sample-exact Sequence Header normalization with four AVM
  v1.0.0 fixtures.
- Milestone 1 provides checked readers, writers, arenas, AV2 OBU framing,
  Sequence Header and Content Interpretation parsing, nine-byte `av2C`, strict
  v0 carriage validation, and checked BMFF box headers.
- Milestone 2 provides a strict zero-copy typed parser and deterministic writer
  for one primary `av02` item, with byte-exact output and mutation coverage.
- Milestone 3 provides an offline hosted artifact laboratory that generates,
  muxes, inspects, extracts, decodes, and compares the four-fixture matrix.
- Milestone 4 provides bounded freestanding validation, structural dialect
  detection, metadata query, exact I420 output sizing, and input binding.
- Milestone 5 is implemented: the private freestanding decoder reconstructs all
  eleven tiny luma and four tiny chroma leaves sample-exactly through tell 725
  with caller-owned traversal, context, scratch, and live-plane state. Focused
  deblock, CDEF, CCSO, and GDF kernels pass; restoration remains unsupported
  and fail-closed.
- Milestone 6 is implemented: caller-owned coded Y/U/V planes distinguish
  visible reference bounds from padded storage, retain per-4x4 reconstruction
  readiness, and reconstruct all 12 odd luma and five odd chroma leaves
  sample-exactly through tell 696 before filtering.
- Milestone 7 completes the ordinary 160x120 fixture through all luma and
  chroma leaves, final tile tell 6498, and sample-exact prefilter and cropped
  output parity.
- Milestone 8 completes the high-detail, CFL, IntraBC, coefficient, prediction,
  and advanced-transform coverage reached by its locked fixture. Milestone 9
  completes mandatory-filter integration and filtered parity. Milestone 10
  remains planned for workspace finalization and the stable public decode API.
- The private Windows decoder emits I420 byte-identical to pinned AVM output
  for all 31 research artifacts.

The public API is declared in `include/av2heif.h`. Milestone details and
limitations are recorded in `docs/milestone-0.md` through
`docs/milestone-10.md`; the longer roadmap is in `plan.md`.

## Build and test

CMake 3.20 or newer, Ninja, and Clang are the required build tools. On Linux
and macOS, run the hosted C gate with AddressSanitizer and
UndefinedBehaviorSanitizer:

```sh
cmake --preset host-sanitized
cmake --build --preset host-sanitized
ctest --preset host-sanitized
```

The short release workflow builds the current freestanding Windows decoder:

```sh
make clean
make
```

On Windows with LLVM-MinGW, use `mingw32-make clean` followed by
`mingw32-make`; it is the same Makefile workflow.

`make clean` removes only the ignored `build/` tree. It preserves the pinned
AVM checkout in `vendor/`, all source files, fixtures, and tracked evidence.
The rebuilt decoder is `build/windows-x86_64/av2heif-decoder.exe`.

Use the `host` preset when sanitizer runtimes are unavailable, including with
LLVM-MinGW on Windows. Both host presets compile all 16 C harnesses and the
artifact tool. All 16 unit suites run by default. The layout harness pins key
64-bit decoder state sizes and alignments, while the artifact-profile harness
checks the OBU, sequence, frame-header, and tile-group contracts for all 31
tracked artifacts. The sample-math harness pins the first self-contained
decoder-internal helper interface introduced by refactoring Phase 1.

Python is not required to build the decoder or run the hosted C gate. It is an
optional development dependency for table regeneration, historical milestone
evidence, and the Milestone 9 parity runner. The Windows product preset enables
that parity test; other configurations can opt in with
`AV2HEIF_ENABLE_PYTHON_TESTS=ON`.

The strict freestanding undefined-symbol check is owned by CMake and runs as
part of normal builds through `av2heif-freestanding-check`. The Makefile keeps a
compatibility entry point for historical milestone workflows. New build and
test targets should be added to CMake only.

On Windows x86-64, LLVM, CMake, and Ninja can build a native freestanding PE
smoke executable without the Microsoft C runtime or Windows SDK headers:

```powershell
cmake --preset windows-x86_64
cmake --build --preset windows-x86_64
ctest --preset windows-x86_64
```

The Windows suite includes a vendor-free corpus gate that decodes all 31
tracked OBUs, compares complete I420 hashes with the pinned AVM references, and
removes its temporary outputs.

The preset passes the x86-64 target to both Clang compilation and executable
linking, including when `clang.exe` comes from an AArch64-host LLVM-MinGW
package.

The portable decoder remains in `src/`; only PE startup and Windows imports
live under `src/platform/windows/`. Architecture-specific code belongs under
`src/arch/` when an optimized or ABI-specific implementation is introduced.

### Experimental Windows OBU decoder

The Windows build produces `build/windows-x86_64/av2heif-decoder.exe`. It is a
project-owned, statically linked PE with no standard C library, compiler
runtime, vendored codec, or AVM dependency. Its only imported DLL is
`KERNEL32.dll`, used for file I/O, virtual memory, and process startup.

Decode a raw AV2 OBU stream to I420 or 24-bit BMP by selecting the output
extension:

```powershell
.\build\windows-x86_64\av2heif-decoder.exe `
  .\images\artifacts\emerald-192x96-qp208.obu `
  -o .\build\emerald.i420

.\build\windows-x86_64\av2heif-decoder.exe `
  .\images\artifacts\emerald-192x96-qp208.obu `
  -o .\build\emerald.bmp
```

BMP output currently applies deterministic limited-range BT.601 I420-to-BGR
conversion. All 31 research artifacts match their locked AVM I420 references
byte-for-byte. The CLI accepts raw OBU input only. HEIF item extraction and
the stable public decode API remain Milestone 10 work.

Failed frame decodes report the original cause separately from the retained
coarse status, followed by the pipeline/syntax stage, plane, leaf position and
geometry, entropy tell, and completed leaf counts. For example:

```text
error: decode failed: invalid argument
  cause: unsupported feature
  stage: chroma parse / U coefficient prefix
  leaf: (0,16) block=64x32 plane=U
  entropy tell: 1567; completed leaves: Y=26 UV=1
```

Run the hosted Milestone 5 normative and pinned-AVM correspondence checks with:

```sh
make milestone-5-evidence
```

Run the hosted AVM differential experiment with:

```sh
make milestone-0
```

That target downloads or verifies the pinned AVM checkout and is intentionally
outside the production dependency boundary.

On Windows, build the native x86-64 AVM tools with LLVM-MinGW UCRT:

```powershell
winget install --id MartinStorsjo.LLVM-MinGW.UCRT --exact --scope user
.\scripts\setup-avm.ps1
```

This produces `build/reference/avm/avmenc.exe` and `avmdec.exe`. The Windows
configuration uses AVM's generic C path and disables the two encoder heuristics
that otherwise require TensorFlow Lite.

After the pinned AVM build is available, run the complete artifact laboratory
without network access with:

```sh
make milestone-3
```

The machine-readable result is `build/m3/laboratory/manifest.json`.

Run the Milestone 4 query and fixture-parity gate with:

```sh
make milestone-4
```
# AV2-HEIF experiment

This repository explores storing and decoding AV2 still images in HEIF files.
The file format is project-defined and experimental; it is not a standardized
successor to AVIF.

The decoder core is CC0, freestanding C11, and allocation-free. It uses memory
provided by its caller and does not depend on the C standard library or a
vendored codec. Pinned AVM tools are used separately to generate reference
files and verify decoded pixels; they are not part of the decoder.

> [!WARNING]
> This is research software and has not received a production security review.
> Do not use it in production to process untrusted files. Read the
> [security policy](SECURITY.md) for risks and reporting instructions.

## Current status

The public decoder accepts this project's canonical HEIF layout and produces
tightly packed 8-bit planar output in three formats:

- **I400:** monochrome, with one full-resolution Y plane;
- **I420:** color, with one full-resolution Y plane and half-resolution U and
  V planes in both directions; and
- **I444:** color, with full-resolution Y, U, and V planes.

The decoder reports the exact output and working-memory sizes before decoding.
The caller owns every buffer. Unsupported profiles, 4:2:2 chroma, sample depths
other than 8-bit, restoration, malformed input, and inconsistent HEIF metadata
are rejected instead of being decoded partially.

Milestones 0 through 12 are complete. Current validation includes:

- exact I420 output for all 31 tracked research artifacts;
- exact I400 and I444 output for the original 12-image corpus at dimensions up
  to 800 pixels, including the 800 x 800 Palace image; and
- the same 12-image I444 results through the public HEIF API, native Windows
  executable, and browser WebAssembly module.

"Exact" means every decoded Y, U, and V sample matches pinned AVM v1.0.0
output. It is evidence for the tested files, not a claim that every valid AV2
file is supported or that the decoder is secure.

The public API is declared in `include/av2heif.h`. Detailed scope and evidence
are recorded in `docs/dev/milestone-0.md` through
`docs/dev/milestone-12.md`; the longer roadmap is in `plan.md`.

## Security

This project parses complex binary input but has not received a production
security review. Its fixed limits, caller-owned memory, strict compiler
settings, and checks that reject unsupported input reduce some risks; they do
not prove memory safety or make untrusted files safe to open.

See [SECURITY.md](SECURITY.md) before embedding the decoder or testing files
from outside this repository. It describes known risk areas, safer testing
practices, the project's support expectations, and how to report a
vulnerability.

## Build and test

CMake 3.20 or newer, Ninja, and Clang are the required build tools. On Linux
and macOS, run the hosted C gate with AddressSanitizer and
UndefinedBehaviorSanitizer:

```sh
cmake --preset host-sanitized
cmake --build --preset host-sanitized
ctest --preset host-sanitized
```

The short release workflow builds the decoder for the current host and native
architecture:

```sh
make clean
make
```

On macOS this produces `build/native/av2heif-decoder`, a native Mach-O
executable. On Windows with LLVM-MinGW, use `mingw32-make clean` followed by
`mingw32-make`; it produces an x86-64 or ARM64 PE executable under
`build/windows-x86_64/` or `build/windows-arm64/` according to the host
architecture.

`make clean` removes only the selected decoder build directory. It preserves
other builds, the pinned AVM checkout in `vendor/`, all source files, fixtures,
and tracked evidence. Override `DECODER_PRESET` and `DECODER_BUILD_DIR`
together to select a non-native product explicitly.

### WebAssembly demo

Build the browser decoder and validate the complete GitHub Pages tree with:

```sh
make wasm
```

Serve `docs/` over HTTP for local testing:

```sh
python3 -m http.server 8080 --directory docs
```

The deployed page is
<https://mathiasschindler.github.io/avif2-experiment/>. It provides the
manifest-backed AV2 quality comparison, bitrate-matched JPEG, WebP, and AVIF
reference series, and local OBU decoding through the same WebAssembly module.
A logarithmic bits-per-pixel control selects the nearest available AV2 and
reference encodes. The reference formats use their own non-equivalent quality
scales and are decoded by the browser.

Use the `host` preset when sanitizer runtimes are unavailable, including with
LLVM-MinGW on Windows. Both host presets compile all 17 C test programs and the
artifact tool. All 17 unit suites run by default. The layout harness pins key
64-bit decoder state sizes and alignments, while the artifact-profile harness
checks the OBU, sequence, frame-header, and tile-group contracts for all 31
tracked artifacts. The sample-math harness pins the first self-contained
decoder-internal helper interface introduced by refactoring Phase 1.

Python is not required to build the decoder or run the hosted C gate. It is an
optional development dependency for table regeneration, historical milestone
evidence, and corpus comparison tests. The Windows product preset enables
those comparison tests; other configurations can opt in with
`AV2HEIF_ENABLE_PYTHON_TESTS=ON`.

The strict freestanding undefined-symbol check is owned by CMake and runs as
part of normal builds through `av2heif-freestanding-check`. The Makefile keeps a
compatibility entry point for historical milestone workflows. New build and
test targets should be added to CMake only.

On Windows x86-64, or when cross-compiling to it, LLVM, CMake, and Ninja can
build a freestanding PE executable without the Microsoft C runtime or Windows
SDK headers:

```powershell
cmake --preset windows-x86_64
cmake --build --preset windows-x86_64
ctest --preset windows-x86_64
```

The Windows suite includes vendor-free corpus gates for all 31 tracked I420
files and the maximum-size I400 and I444 corpora. They compare complete output
hashes with pinned AVM references and remove their temporary outputs. The
hosted Python gate also checks I444 through the public canonical-HEIF API.

The preset passes the x86-64 target to both Clang compilation and executable
linking, including when `clang.exe` comes from an AArch64-host LLVM-MinGW
package.

The equivalent ARM64 workflow is:

```powershell
cmake --preset windows-arm64
cmake --build --preset windows-arm64
ctest --preset windows-arm64
```

The portable decoder remains in `src/`; hosted POSIX file I/O lives under
`src/platform/posix/`, while only PE startup and Windows imports live under
`src/platform/windows/`. Architecture-specific codec code belongs under
`src/arch/` when an optimized or ABI-specific implementation is introduced.

### Experimental Windows OBU decoder

The Windows build produces `build/windows-x86_64/av2heif-decoder.exe`. It is a
project-owned, statically linked PE with no standard C library, compiler
runtime, vendored codec, or AVM dependency. Its only imported DLL is
`KERNEL32.dll`, used for file I/O, virtual memory, and process startup.

Decode a raw AV2 OBU stream by choosing the extension that matches its decoded
format: `.i400`, `.i420`, or `.i444`. Use `.bmp` for a viewable 24-bit image:

```powershell
.\build\windows-x86_64\av2heif-decoder.exe `
  .\images\artifacts\emerald-192x96-qp208.obu `
  -o .\build\emerald.i420

.\build\windows-x86_64\av2heif-decoder.exe `
  .\images\artifacts\emerald-192x96-qp208.obu `
  -o .\build\emerald.bmp
```

BMP output applies deterministic limited-range BT.601 conversion from I400,
I420, or I444 to BGR pixels. The CLI accepts raw OBU input only; applications
that read the project-defined HEIF files use the public API in
`include/av2heif.h`.

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

On Windows ARM64, build native reference tools side by side without changing
the known x86-64 build:

```powershell
.\scripts\setup-avm.ps1 -Architecture arm64
```

This produces `build/reference/avm-arm64/avmenc.exe` and `avmdec.exe`. The
experiment harness uses these native tools by default. The known x86-64 build
remains available at `build/reference/avm`; select it explicitly when needed:

```powershell
python.exe .\experimental\run_experiment.py `
  --avmenc .\build\reference\avm\avmenc.exe `
  --avmdec .\build\reference\avm\avmdec.exe
```

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
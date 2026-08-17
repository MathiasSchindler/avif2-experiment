# AV2-HEIF experiment

This repository explores storing and decoding AV2 still images in HEIF files.
The file format is project-defined and experimental; it is not a standardized
successor to AVIF.

The decoder core is CC0, freestanding C11, allocation-free, and libc-free. It
uses caller-provided workspace and output buffers. Pinned AVM tools generate
reference files and verify decoded pixels, but are not production dependencies.

The project uses the CC0-licensed
[newos](https://github.com/MathiasSchindler/newos) project as a source
substrate for suitable freestanding support code. Needed code is copied and
adapted into this repository rather than adding newos as a build or runtime
dependency. Imported source revisions, paths, and adaptations are recorded in
[provenance/cc0-sources.md](provenance/cc0-sources.md).

> [!WARNING]
> This is research software and has not received a production security review.
> Do not use it in production to process untrusted files. See
> [SECURITY.md](SECURITY.md).

## Decoder contract

The public decoder accepts this project's canonical single-image HEIF layout
and produces tightly packed planar output in five formats:

| Output | Contract |
| --- | --- |
| I400 | 8-bit full-resolution Y |
| I420 | 8-bit Y with half-resolution U and V in both directions |
| I444 | 8-bit full-resolution Y, U, and V |
| `yuv420p10le` | Profile-0 10-bit 4:2:0 in little-endian 16-bit storage |
| `yuv444p10le` | Profile-4 10-bit 4:4:4 in little-endian 16-bit storage |

The caller can query exact output and workspace sizes before decoding and owns
every buffer. `Av2hHeifImage.pixel_format` describes sample depth, storage,
byte order, and plane order; plane offsets, row strides, capacities, and public
buffer sizes are byte counts.

The decoder admits tested 8-bit subsets of IntraBC, always-on single-picture
TCQ, predefined quantization matrices, ALT_Q segmentation and delta-Q,
Intra-DIP, transform partitioning, directional IBP and MRL, secondary
transforms, PC-Wiener restoration, and Wiener-nonsep restoration. These are
bounded profile statements, not claims of complete AV2 feature-family support.

The authoritative supported and fail-closed boundaries are in
[docs/capabilities.md](docs/capabilities.md). Current unfinished work is in
[docs/roadmap.md](docs/roadmap.md), and [docs/README.md](docs/README.md)
classifies current documentation, release evidence, and historical records.

## Validation

"Exact" means every decoded Y, U, and V sample matches output from pinned AVM
v1.0.0 at revision `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.

Current evidence includes:

- 88 legacy photographic reference streams;
- 14 high-effort target-baseline photographic streams exercising the recently
  admitted coding tools;
- exact native and published WebAssembly output for all 102 streams;
- dedicated 8-bit I400, I420, and I444 corpora;
- 10-bit 4:2:0 and 4:4:4 synthetic and photographic release gates; and
- hosted sanitizer, freestanding symbol, Linux/macOS no-libc, Windows, fuzz, and
  malformed-input checks.

This evidence covers the tested files and tuples. It does not establish general
AV2 conformance or security.

## Build and test

GNU Make, Clang, `ar`, Python 3, and standard Unix command-line tools are
required. Build the native decoder with:

```sh
make
```

The Makefile contains the authoritative source lists and invokes the compiler,
archiver, linker, and tests directly. It does not use CMake, Ninja, generated
dependency files, or a separate test registry.

With the generated corpus assets present, run the hosted, corpus,
reference-lock, freestanding, and Python gates:

```sh
make test
```

The generated artifact corpora are not all tracked by Git. Without them, the
hosted, lock-only, and freestanding portions remain independently callable as
`make test-c test-reference-locks test-freestanding`.

Run the extended gate, including exact decoding of every locally present
`images/reference` stream:

```sh
make test-extended
```

The reference files are intentionally not all tracked by Git. When they are
present, the exact reference gate can also be run directly:

```sh
make test-reference-images
```

Run the same reference locks through the published browser module with:

```sh
make test-reference-images-wasm
```

Build the no-libc decoder on Linux x86-64/AArch64 or macOS ARM64 with:

```sh
make nolibc
```

The Makefile selects a static ELF executable on Linux and a no-import Mach-O
executable on macOS. Linux x86-64 and macOS ARM64 use the CC0 project linker
copied from newos; Clang only compiles their freestanding objects. Linux
AArch64 retains LLD because the project linker does not yet provide an AArch64
ELF backend. The macOS build requires the Apple Command Line Tools. The
resulting binaries have no libc or other dynamic library dependency. Both
platform paths run their artifact inspection and exact decode parity tests as
part of `make nolibc`.

Build the browser decoder and viewer assets with:

```sh
make wasm
```

Optional pinned-AVM reference and accounting setup uses AVM's own upstream
build system; that external build is separate from this project's Makefile.

The public API is declared in [include/av2heif.h](include/av2heif.h). Detailed
platform, corpus, and release procedures are indexed from
[docs/README.md](docs/README.md).

## Security

The fixed limits, caller-owned memory, strict compiler settings, and fail-closed
syntax checks reduce some risks; they do not prove memory safety. Read
[SECURITY.md](SECURITY.md) before embedding the decoder or opening files from
outside this repository.

## Licensing

The decoder source and other material covered by [LICENSE](LICENSE) are
released under CC0 1.0. The photographs under `images/`, their prepared forms,
encoded derivatives, previews, and decoded references are separately licensed.
See [images/README.md](images/README.md) and
[images/ATTRIBUTION.md](images/ATTRIBUTION.md).
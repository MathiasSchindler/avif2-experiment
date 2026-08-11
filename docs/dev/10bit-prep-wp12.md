# 10-bit preparation WP12 release gate

WP12 was implemented and validated on 2026-08-11. It adds one repeatable
release command and proves that WP1 through WP11 prepared storage, arithmetic,
fixtures, previews, and container metadata without changing existing 8-bit
decoded bytes.

## Release command

Run the complete gate from the repository root:

```sh
make 10bit-wp12
```

The target builds and runs the hosted ASan/UBSan suite, all Python development
tests, the strict freestanding undefined-symbol check, full native I400/I420/
I444 parity, public canonical-HEIF parity, Linux no-libc tests, WebAssembly
build and parity, Windows x86-64 cross-build and import checks, and pinned AVM
corpus regeneration.

`tools/check_windows_imports.py` requires exactly the established PE surface:
the decoder imports ten functions from `KERNEL32.dll`, and the smoke executable
imports only `ExitProcess`. `tools/validate_8bit_444_wasm.py` continues to use
the Python `wasmtime` module when available and now falls back to Node's built-in
WebAssembly runtime, avoiding an optional package dependency in the release
gate.

`tools/check_10bit_reproducibility.py` snapshots every file below
`tests/fixtures/10bit`, runs the complete `10bit-wp10` generation workflow,
and compares file names and SHA-256 values. It rejects any added, removed, or
changed corpus file.

## ABI comparison

The final LP64 public ABI equals the WP0 baseline:

| Type | WP0 size/alignment | WP12 size/alignment | Difference |
| --- | ---: | ---: | --- |
| `Av2hPlaneLayout` | 32 / 8 | 32 / 8 | none |
| `Av2hHeifImage` | 80 / 8 | 80 / 8 | none |
| `Av2hInfo` | 248 / 8 | 248 / 8 | none |

WP1's six-byte `Av2hPixelFormat` occupies existing LP64 padding.
`Av2hHeifImage.pixel_format` starts at byte 66 and `property_count` remains at
byte 72. On wasm32, `Av2hHeifImage` intentionally grew from 56 to 60 bytes and
moved `property_count` from byte 52 to 56; `Av2hInfo` remained 160 bytes. This
is the only approved public aggregate-size difference from WP0. The browser
decoder's separate nine-word result record remained unchanged.

Every private type recorded by WP0 retains its size and alignment, including
the 66-byte sequence state, 3,192-byte frame state, 35,464-byte tile decoder,
16,896-byte reconstruction scratch, 368-byte filter workspace, and 152-byte
filter layout. Additive locks now also cover the six-byte pixel descriptor,
16-bit `Av2hQIndex`, 56-byte coefficient prefix, renamed byte-stride plane
records, and 16-byte frame workspace layout. Q-index tests pass the value 400
through coefficient and deblock handoffs without narrowing.

## Workspace comparison

All public 8-bit output and workspace sizes remain exactly the WP0 values, with
four-byte workspace alignment:

| Format and fixture | Output bytes | Workspace bytes |
| --- | ---: | ---: |
| I400 tiny 16x16 | 256 | 19,419 |
| I400 odd 17x15 | 255 | 20,179 |
| I400 ordinary 160x120 | 19,200 | 133,391 |
| I400 high-detail 96x80 | 7,680 | 64,493 |
| I420 tiny 16x16 | 384 | 20,350 |
| I420 odd 17x15 | 399 | 21,543 |
| I420 ordinary 160x120 | 28,800 | 198,355 |
| I420 high-detail 96x80 | 11,520 | 90,545 |
| I444 tiny 16x16 | 768 | 21,502 |
| I444 odd 17x15 | 765 | 23,271 |
| I444 ordinary 160x120 | 57,600 | 284,757 |
| I444 high-detail 96x80 | 23,040 | 125,105 |

WP3 added one- and two-byte workspace accounting without changing these
one-byte totals. The two-byte tests cover I400, I420, and I444 at odd and
maximum dimensions, prove that only sample-backed regions scale, retain
four-byte aggregate alignment, exercise overflow and limits, and reject
two-byte binding before mutating decoder state. Normative 10-bit workspace
binding remains deferred.

## Byte parity

The completed gate produced these exact results:

- hosted ASan/UBSan CTest suite: 24/24 passed;
- Python development suite: 27/27 passed;
- production I420 ledger: 31/31 exact, plus three output-policy fixtures for
  the established 34-case I420 coverage;
- production I400 ledger: 12/12 exact;
- production I444 native ledger: 12/12 exact;
- canonical HEIF public I444: 12/12 exact;
- BMP and PNG output-policy hashes for I400, I420, and I444 unchanged;
- PNG signature, chunks, CRC-32, stored DEFLATE, Adler-32, filters, and RGB8/
  RGB16 samples independently reconstructed by `png-writer-unit`;
- Linux static no-libc suite: 2/2 passed;
- WebAssembly I444 parity through Node: 12/12 exact;
- Windows x86-64 decoder and smoke executables built cleanly, with exact import
  surfaces; and
- WP10 focused tests: 10/10 passed, with all 108 source, reference, decoded,
  report, manifest, and negative-fixture files byte-identical after complete
  regeneration.

The AVM dependency builds emitted upstream CMake policy and deprecation
warnings but completed successfully. Regeneration changed no tracked or
untracked project artifact outside the WP12 implementation files.

Windows runtime parity remains unavailable on this Linux host because no Wine,
Wibo, or PE binfmt handler is installed. The cross-build and import checks are
complete; runtime parity must still be executed on Windows or with a configured
PE emulator.

## Preparation boundary

The release retains one deliberate support boundary. Canonical HEIF validates
sequence depth against `pixi` before its explicit 8-bit pixel-format admission
gate; consistent 10-bit metadata returns `AV2H_UNSUPPORTED`, while mismatches
return `AV2H_INVALID_DATA`. Raw OBU decode independently rejects non-8-bit
sequences at the frame admission gate before reconstruction. No 16-bit sample
storage, 9-bit Q-index syntax, high-bit-depth quantization, prediction,
transform, reconstruction, filter arithmetic, or public 10-bit output was
enabled during preparation.

# 10-bit preparation WP0 baseline

WP0 was implemented and validated on 2026-08-10. It records the current 8-bit
decoder contract and normalizes the generated 10-bit AVM acceptance corpus
without enabling 10-bit decode in the production decoder.

## Exact 8-bit output ledgers

The baseline has three tracked owners:

- `tests/artifact-parity.sha256` contains all 31 production I420 SHA-256
  values.
- `tests/8bit-production-parity.sha256` contains the 12 production I400 and 12
  production I444 SHA-256 values, dimensions, and format labels.
- `tests/public_decode_unit.c` contains the complete FNV-1a64 output values for
  the tiny, odd, ordinary, and high-detail fixtures in I400, I420, and I444.

The production I400 and I444 validators compare generated manifest dimensions
and hashes with the tracked ledger before running a decoder. A generator can no
longer move its ignored manifest and expected output together without failing
the tracked baseline.

## Query and layout baseline

On the 64-bit baseline ABI, public layouts are:

| Type | Size | Alignment |
| --- | ---: | ---: |
| `Av2hPlaneLayout` | 32 | 8 |
| `Av2hHeifImage` | 80 | 8 |
| `Av2hInfo` | 248 | 8 |

Private decoder layouts are:

| Type | Size | Alignment |
| --- | ---: | ---: |
| `Av2hSequenceState` | 66 | 2 |
| `Av2hFrameState` | 3,192 | 8 |
| `Av2hCoeffLevelCdfs` | 20,464 | 2 |
| `Av2hLeafCdfs` | 2,522 | 2 |
| `Av2hTileDecoder` | 35,464 | 8 |
| `Av2hReconstructionScratch` | 16,896 | 4 |
| `Av2hFilterWorkspace` | 368 | 8 |
| `Av2hFilterWorkspaceLayout` | 152 | 8 |

`layout-unit` owns these values. `public-decode-unit` owns the following
queried output and workspace values; each workspace alignment is 4 bytes.

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

## Normalized AVM profile

`tools/generate_10bit_avm_references.py` emits manifest schema 2 and records a
machine-readable `coding_tool_profile`. Both 4:2:0 and 4:4:4 explicitly disable
these sequence-level tools:

- film grain;
- LCR;
- operating-point sets;
- atlas; and
- restoration.

The 4:4:4 profile additionally disables IntraBC. Tools not listed in the
profile remain at the pinned AVM defaults. The generator records every actual
encode and decode command, AVM revision and binary hash, source hash, OBU hash,
decoded hash, plane statistics, and OBU ledger.

The pinned AVM revision is
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`. Regeneration produced 36 pairs:
three photographs, two resolutions, two chroma formats, and QP 96, 128, and
160. A second complete regeneration reproduced all 36 OBUs, all 36 decoded
planes, and the manifest byte-for-byte.

The accounting-enabled `av2-census` accepted all 36 streams and reported:

- one frame and 10-bit precision for every stream;
- exact manifest dimensions;
- temporal delimiter, sequence header, and closed-loop-key OBU structure;
- positive feature and coding-block counts for every stream;
- 225,597 coding blocks in aggregate; and
- base Q-index 144, 176, and 208 for QP 96, 128, and 160 respectively.

The generated corpus and full census JSON remain ignored build evidence. The
generator, profile test, tracked 8-bit ledgers, and this baseline record are the
durable source-controlled contract.

## Validation gates

The completion run produced these results:

- hosted ASan/UBSan C suite: 17/17 passed;
- Python development suite: 21 passed, one environment-dependent test skipped;
- native decoder and strict freestanding core: built with no undefined core
  symbols;
- production I420 parity: 31/31 exact;
- production I400 parity: 12/12 exact;
- production I444 native parity: 12/12 exact;
- canonical HEIF public I444 parity: 12/12 exact;
- Linux static no-libc suite: 2/2 passed;
- WebAssembly module: built and produced 12/12 exact I444 outputs through
  Node's WebAssembly runtime; and
- Windows x86-64: both freestanding PE32+ binaries built with no diagnostics,
  and the decoder imports only its intended `KERNEL32.dll` surface.

Windows runtime parity could not execute on this Linux validation host because
no Wine, Wibo, or PE binfmt handler is installed. CTest therefore reports
uniform `Exec format error` failures before decoder startup. Run the existing
`windows-x86_64` CTest preset on Windows or with a configured PE emulator to
complete that platform's runtime confirmation; this is an environment gap, not
a waived test or a successful runtime result.
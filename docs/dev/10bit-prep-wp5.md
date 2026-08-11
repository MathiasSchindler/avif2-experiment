# 10-bit preparation WP5 Q-index representation

WP5 was implemented and validated on 2026-08-11. It gives normative Q-index
state one named 16-bit representation without changing current 8-bit frame
syntax, quantizer tables, clamps, decoded arithmetic, or output bytes.

## Representation

`Av2hQIndex` is a private alias for `uint16_t`, owned by `frame.h`. The named
type now carries Q-index values across these boundaries:

| Boundary | Carrier |
| --- | --- |
| Frame header state | `Av2hFrameState.base_q_index` |
| Delta-Q state | `Av2hTileDecoder.current_q_index` and leaf delta-Q interfaces |
| Transform coefficient state | `Av2hFirstCoeffPrefix.q_index` |
| Coefficient CDF and 8-bit dequant interfaces | `Av2hQIndex` parameters |
| Filter handoff | `Av2hDeblockCell.q_index` |

The tile luma and chroma paths now assign the current Q-index directly to each
coefficient prefix. Their former `uint8_t` casts were removed. Deblock commit
uses `prefix->q_index` as its single source rather than accepting a redundant
Q-index argument, so prefix-to-filter handoff cannot disagree or narrow.

CCSO `quant_index` remains `uint8_t`: it is a separate narrow syntax field and
does not carry the frame or transform Q-index domain.

## Preserved 8-bit behavior

This package changes representation only:

- the frame header still reads exactly eight Q-index bits;
- delta-Q still clamps the current 8-bit implementation to `[1, 255]`;
- the 8-bit coefficient dequantizer still clamps lookup input to 255;
- deblock threshold arithmetic still clamps Q-index input to 255; and
- all current quantizer, coefficient CDF, and deblock tables are unchanged.

The named parse-boundary cast records that the eight-bit syntax value enters a
16-bit state carrier. Enabling the high-bit-depth 9-bit syntax and normative
tables remains later work.

## Storage and layout evidence

Focused tests inject Q-index 400, which cannot fit in `uint8_t`:

- a tile decode probe verifies `current_q_index` reaches
  `Av2hFirstCoeffPrefix.q_index` intact; and
- the filter test verifies the prefix value reaches
  `Av2hDeblockCell.q_index` intact through deblock-map commit.

LP64 private layout locks record the reviewed carriers:

| Type | Size | Alignment |
| --- | ---: | ---: |
| `Av2hQIndex` | 2 | 2 |
| `Av2hFirstCoeffPrefix` | 56 | 8 |
| `Av2hDeblockCell` | 14 | 2 |

Existing frame and tile aggregate sizes remain unchanged because their prior
16-bit state was already wide and the widened embedded fields fit existing
padding.

## Validation gates

The completion run produced these results:

- hosted ASan/UBSan C suite: 20/20 passed;
- focused frame, tile, coefficient, filter, and layout vectors passed;
- Python development suite: 21/21 passed;
- clean native decoder and strict freestanding core built with no undefined
  core symbols;
- production I420 parity: 31/31 exact;
- production I400 parity: 12/12 exact;
- production I444 native parity: 12/12 exact;
- canonical HEIF public I444 parity: 12/12 exact;
- Linux static no-libc suite: 2/2 passed;
- WebAssembly module built and produced 12/12 exact I444 outputs through
  Node's WebAssembly runtime; and
- Windows x86-64 decoder and smoke PE32+ binaries built with no diagnostics;
  their established `KERNEL32.dll` import surfaces are unchanged.

A complete hosted build with `-Wconversion -Wsign-conversion` also completed
all 185 targets with diagnostics made nonfatal. A targeted diagnostic gate
found no conversion warning in the migrated Q-index storage or handoff paths.
The wider repository still has unrelated pre-existing strict-conversion
warnings, so repository-wide promotion of those warnings to errors remains a
separate cleanup task.

Windows runtime parity could not execute on this Linux validation host because
no Wine, Wibo, or PE binfmt handler is installed. The native Windows CTest gate
remains required on Windows or a host with a configured PE emulator.
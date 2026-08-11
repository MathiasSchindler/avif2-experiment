# 10-bit preparation WP1 pixel-format descriptor

WP1 was implemented and validated on 2026-08-10. It makes decoded sample
storage explicit without admitting a 10-bit stream or changing any existing
8-bit decoded bytes.

## Descriptor contract

`Av2hPixelFormat` is the authoritative description of decoded memory. It
reports these independent properties:

- chroma format;
- significant bit depth;
- bytes per stored sample;
- unsigned integer storage width;
- storage byte order; and
- Y/U/V plane order.

The public `Av2hHeifImage.pixel_format` field exposes the descriptor returned by
query. `bits_per_channel` remains available for compatibility, but callers must
not use it alone to infer sample storage.

`src/pixel_format.c` owns descriptor construction, validation, equality, plane
count, and plane-dimension derivation. Container parsing constructs a descriptor
only after canonical metadata has been validated. Query uses the descriptor to
derive plane count and chroma geometry, while decode includes the complete
descriptor in its stale-query binding check.

WP1 intentionally constructs and accepts only these descriptors:

| Decoded format | Chroma idc | Precision | Storage | Byte order | Planes |
| --- | ---: | ---: | --- | --- | --- |
| I400 | 1 | 8 bits | one-byte unsigned | not applicable | Y |
| I420 | 0 | 8 bits | one-byte unsigned | not applicable | Y, U, V |
| I444 | 2 | 8 bits | one-byte unsigned | not applicable | Y, U, V |

Unknown enum values, mismatched precision and storage, unsupported chroma, and
all premature 16-bit combinations fail closed. Two-byte layout arithmetic is
deliberately left to WP2.

The hosted artifact inspector exposes `bytes_per_sample`, `sample_storage`,
`byte_order`, and `plane_order` in its effective codec JSON. These are additive
members, so the inspection schema remains v2: no existing member was removed or
changed, and consumers that tolerate unknown JSON members remain compatible.

## Public ABI review

The descriptor is six bytes with one-byte alignment. On the LP64 baseline it
occupies existing padding in `Av2hHeifImage`; aggregate sizes and the offset of
the following `property_count` field remain unchanged.

| ABI | Type | Before WP1 | After WP1 |
| --- | --- | ---: | ---: |
| LP64 | `Av2hHeifImage` | 80 | 80 |
| LP64 | `Av2hInfo` | 248 | 248 |
| wasm32 | `Av2hHeifImage` | 56 | 60 |
| wasm32 | `Av2hInfo` | 160 | 160 |

On LP64, `pixel_format` starts at byte 66 and `property_count` remains at byte
72. On wasm32, `pixel_format` starts at byte 50 and moves `property_count` from
byte 52 to byte 56. The wasm32 `Av2hHeifImage` growth is therefore an explicit
public ABI change for callers that exchange or embed that aggregate directly;
the exported browser decoder result record is separate and remains unchanged.
Target-specific compile assertions confirmed the wasm32 sizes and offsets.

## Validation gates

The completion run produced these results:

- hosted ASan/UBSan C suite: 18/18 passed, including the new descriptor unit;
- Python development suite: 21/21 passed, including artifact pack, exact
  inspection JSON, extraction, and OBU reconstruction;
- native decoder and strict freestanding core: built with no undefined core
  symbols;
- production I420 parity: 31/31 exact;
- production I400 parity: 12/12 exact;
- production I444 native parity: 12/12 exact;
- canonical HEIF public I444 parity: 12/12 exact;
- Linux static no-libc suite: 2/2 passed;
- WebAssembly module: built and produced 12/12 exact I444 outputs through
  Node's WebAssembly runtime; and
- Windows x86-64: decoder and smoke PE32+ binaries built with no diagnostics;
  the decoder imports only its established ten `KERNEL32.dll` functions and
  the smoke binary imports only `ExitProcess`.

Windows runtime parity could not execute on this Linux validation host because
no Wine, Wibo, or PE binfmt handler is installed. The native Windows CTest gate
remains required on Windows or a host with a configured PE emulator.
# 10-bit preparation WP11 HEIF metadata ownership

WP11 was implemented and validated on 2026-08-11. It removes the canonical
HEIF container's hard-coded 8-bit metadata assumptions without enabling
10-bit decoded storage or reconstruction.

## Precision owner

`src/container.c` now owns a private sequence-to-container pixel metadata
mapping. It derives both the canonical `pixi` channel count and every channel's
precision from validated sequence signaling:

| `chroma_format_idc` | Canonical channels |
| ---: | ---: |
| 0 (4:2:0) | 3 |
| 1 (monochrome) | 1 |
| 2 (4:4:4) | 3 |

| `bit_depth_idc` | `pixi.bits_per_channel` |
| ---: | ---: |
| 0 | 10 |
| 1 | 8 |
| 2 | 12 |

The non-monotonic depth-ID mapping follows the pinned AVM sequence syntax.
Sequence parsing retains all three defined IDs so container metadata can be
validated independently of decoded sample support. Undefined IDs remain
unsupported.

The parser first compares `pixi` channel count and precision with the derived
sequence metadata. A mismatch returns `AV2H_INVALID_DATA`. Only after those
cross-checks pass does the same helper attempt to construct the decoded pixel
format. The existing `av2h_pixel_format_init_8bit` call remains the explicit
public admission gate, so consistent 10-bit or 12-bit metadata returns
`AV2H_UNSUPPORTED` and no high-bit-depth descriptor is exposed.

The frame parser independently retains its `bit_depth_idc == 1` gate. Raw OBU
decoding therefore also rejects high-bit-depth input before frame syntax or
reconstruction can use 8-bit sample storage.

## Writer contract

The canonical writer uses the same derived metadata for `pixi` sizing, channel
count, and emitted precision bytes. Its deterministic property order remains
`av2C`, `ispe`, `pixi`, `colr`, and box sizes remain a function of channel
count rather than precision value.

An 8-bit source still rewrites to the exact existing canonical byte array. A
validated 10-bit source can now be wrapped with three 10-bit `pixi` channels;
the writer's internal verification expects the public parser to return
`AV2H_UNSUPPORTED` for that otherwise consistent output. This prepares
container emission without enabling public 10-bit decode.

## Focused evidence

`container-unit` embeds the sequence and Closed Loop Key OBUs from WP10's
smallest selected positive fixture,
`tiny-precision-16x16-yuv420p10le-qp160.obu`. It does not invoke pixel
reconstruction. The vector proves that:

- writer sizing succeeds and the writer emits three precision bytes of 10;
- parsing the consistent 10-bit container returns `AV2H_UNSUPPORTED`;
- changing its `pixi` precision to 8 returns `AV2H_INVALID_DATA`;
- changing the canonical 8-bit container's `pixi` precision to 10 returns
  `AV2H_INVALID_DATA`; and
- canonical 8-bit writing and remuxing remain byte-identical.

Existing monochrome and color channel-count mismatch vectors continue to fail
closed, and mixed per-channel `pixi` precision remains invalid during property
parsing.

## Validation gates

The completion run produced these results:

- focused sanitizer-backed `container-unit`: 1/1 passed;
- complete hosted ASan/UBSan CTest suite: 24/24 passed;
- Python development suite: 27/27 passed, including WP10 unsupported 10-bit
  4:2:2 and 12-bit status and sanitizer checks;
- Linux static no-libc suite: 2/2 passed;
- WebAssembly module rebuilt and validated 2 scenes, 76 OBUs, and 166
  baselines; and
- Windows x86-64 decoder and smoke executables cross-built cleanly. The decoder
  imports its existing ten functions from `KERNEL32.dll`, while the smoke
  executable imports only `ExitProcess`.

Windows runtime validation was unavailable on this Linux host. No
high-bit-depth pixel-format constructor, output layout, or reconstruction path
was added by WP11.
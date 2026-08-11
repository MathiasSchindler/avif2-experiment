# 10-bit implementation WP1 syntax and metadata

## Result

10B-WP1 is complete. The decoder now parses the constrained profile-0 10-bit
4:2:0 stream through its exact frame and tile boundaries and exposes its pixel
format and byte layout through container metadata and `av2h_query`. Normative
10-bit sample storage and reconstruction remain unavailable.

## Admitted format

The only new valid pixel format is:

- profile 0;
- chroma format idc 0 (4:2:0);
- bit-depth idc 0 (10-bit);
- three planar Y/U/V planes;
- unsigned 16-bit sample storage;
- little-endian byte order; and
- two bytes per sample.

Eight-bit monochrome, 4:2:0, and 4:4:4 descriptors retain their existing
contracts. Ten-bit monochrome and 4:4:4 descriptors are not admitted in this
package. Twelve-bit, 10-bit 4:2:2, unsupported profile/chroma combinations,
quantization matrices, and base-Q-zero lossless frames remain unsupported.

For the locked 16x16 stream, public query reports:

| Plane | Dimensions | Row bytes | Offset | Size |
| --- | ---: | ---: | ---: | ---: |
| Y | 16x16 | 32 | 0 | 512 |
| U | 8x8 | 16 | 512 | 128 |
| V | 8x8 | 16 | 640 | 128 |

The total output size is 768 bytes.

## Syntax checkpoints

The base Q-index width is selected from sequence bit depth: 8 bits for the
existing 8-bit path and 9 bits for the admitted 10-bit path. The locked stream
parses with:

- base Q-index 256;
- uncompressed frame-header endpoint 25 bits;
- tile-group header size 4 bytes; and
- tile entropy endpoint inherited from the WP0 oracle.

`Av2hQIndex` remains a 16-bit domain in frame state, tile current state,
coefficient prefixes, and deblock cells. The WP1 fixture test proves the value
256 reaches tile CDF initialization intact. Existing wide-domain tests also
carry Q-index 400 through coefficient and deblock state. Ten-bit dequantization
arithmetic itself remains owned by 10B-WP3.

Every byte prefix shorter than the locked four-byte tile header fails frame
parsing. Direct mutations prove qmatrix and base-Q-zero rejection at the frame
owner. The generated negative corpus covers valid unsupported 10-bit 4:2:2 and
12-bit 4:2:0 streams plus malformed profile/chroma and truncation cases.

## Deliberate decode boundary

`av2h_query` succeeds for a canonical HEIF containing the locked stream and
returns the exact U16 little-endian descriptor, plane layout, output size, and
two-byte workspace size. `av2h_decode` then returns `AV2H_UNSUPPORTED` with
`AV2H_CONTEXT_WORKSPACE` before changing any output byte.

The boundary is owned by `av2h_frame_decode_workspace_init`, which still binds
only one-byte sample planes. This explicit gate replaces the previous accidental
layout mismatch and is the boundary 10B-WP2 will remove when it adds a
byte-width-aware initializer.

The raw decoder tool likewise exits 3 with `unsupported feature` and leaves a
pre-existing output file unchanged. It does not advertise a 10-bit raw output
extension yet.

## Validation

Run the focused package gate with:

```sh
make 10bit-wp1
```

It runs the frame, container, pixel-format, output-layout, workspace, and public
decode unit tests, the generated negative fixture corpus, and the WP0 real
decoder nonmutation guard. The ordinary `make test` suite remains the full
regression gate.
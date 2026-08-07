# Milestone 11: 8-bit monochrome

Milestone 11 is complete. The public decoder accepts the AV2 8-bit 4:0:0
still-picture profile in addition to the existing 8-bit 4:2:0 profile.

## Scope

- derive monochrome from `chroma_format_idc == 1` before conditionally parsing
  sequence and frame syntax;
- retain fixed three-slot internal arrays while tracking one active plane;
- write and validate canonical HEIF `pixi` with one 8-bit channel;
- report a contiguous Y-only public output with canonical zero U/V layouts;
- allocate, clear, validate, reconstruct, and filter only active planes; and
- skip chroma traversal while advancing every luma superblock root.

The sample type remains 8-bit. Luma prediction, transforms, reconstruction,
and filtering use the existing kernels and byte strides. The public API remains
allocator-free: callers provide the exact queried workspace and output.

## Public layout

For monochrome input, `Av2hInfo.image.channel_count` is one and
`Av2hInfo.output_size` is `width * height`. `planes[0]` describes packed Y.
`planes[1]` and `planes[2]` have zero width, height, row bytes, offset, and
size. Altering any queried layout field invalidates the query binding.

## Evidence

`tools/generate_m11_fixtures.py` reproducibly encodes and independently
decodes tiny 16x16, odd 17x15, ordinary 160x120, and high-detail 96x80
monochrome streams with pinned AVM v1.0.0 tools. OBU and decoded-Y hashes are
recorded in `tests/fixtures/monochrome-manifest.json`.

`public-decode-unit` muxes all four streams into canonical HEIF and pins their
complete Y output hashes. The ordinary case also checks undersized and
misaligned workspace, undersized output, stale query state, overlap, every
shorter container prefix, and preflight output nonmutation. `container-unit`
checks canonical one-channel writing/parsing and rejects structurally valid
one-vs-three-channel `pixi`/bitstream mismatches.

## Exclusions

4:2:2, 4:4:4, 10-bit, 12-bit, RGB conversion, restoration, and coding tools
outside the established still-picture profile remain unsupported and fail
closed. Existing 8-bit 4:2:0 output remains unchanged.
# 8-bit 4:4:4 support

8-bit 4:4:4 support is complete. The public decoder accepts the AV2 8-bit 4:4:4
still-picture profile in addition to the existing 8-bit monochrome and 4:2:0
profiles.

## Scope

- admit sequence profile 4 only with three-plane 8-bit 4:4:4 syntax;
- derive every active plane dimension and coordinate shift through shared
  checked geometry helpers;
- use full-resolution chroma partition, transform, reconstruction, neighbor,
  deblock, CDEF, CCSO, and GDF geometry;
- map CfL and MHCCP luma references directly to co-located 4:4:4 samples;
- report three contiguous full-resolution byte planes through query and
  decode; and
- render I400, I420, and I444 correctly in the Windows and WebAssembly
  adapters.

The sample type remains 8-bit. Unsupported profile/chroma pairs and 4:2:2
remain fail-closed. The public core remains freestanding and allocator-free.

## Public layout

For 4:4:4 input, `Av2hInfo.image.channel_count` is three and each public plane
has the visible image width and height. Row bytes equal plane width. Plane U
starts after `width * height` bytes and plane V starts after a second region of
the same size. Canonical HEIF `pixi` contains three 8-bit channels.

Workspace sizing uses the same full-resolution geometry for reconstructed and
filtered chroma storage. Checked arithmetic rejects dimensions or alignments
that cannot be represented without mutating caller output.

## Evidence

`tools/generate_8bit_444_fixtures.py` reproducibly encodes and independently
decodes tiny 16x16, odd 17x15, ordinary 160x120, and high-detail 96x80 I444
streams with pinned AVM v1.0.0 tools. The commands and source, OBU, and decoded
hashes are recorded in `tests/fixtures/i444-manifest.json`. IntraBC and
restoration are disabled to keep the corpus bounded to the supported coding
tool surface.

`public-decode-unit` muxes all four streams into canonical HEIF and pins their
complete planar output hashes. The high-detail output FNV-1a64 is
`0x2fe36a3632f85bb1`. The same suite retains exact monochrome and I420 fixture
coverage, while focused reconstruction and filter units cover geometry,
prediction, crop, workspace, and filter boundaries.

## Exclusions

4:2:2, 10-bit and 12-bit samples, RGB conversion in the production core,
restoration, and newly enabled coding tools remain unsupported. Existing
8-bit monochrome and 4:2:0 output layouts and decoded bytes are unchanged.

## Production-scale validation

Large 4:4:4 chroma coding leaves now emit capped 64x64 transform nodes in AVM
raster order. Each node owns its U/V coefficient contexts, CCTX,
reconstruction, and deblock metadata. Shared-chroma handoff retains up to four
pending transform pairs instead of overwriting one slot.

The original 12-artifact maximum-800-pixel corpus, including Palace 800x800,
matches pinned AVM byte-for-byte on Y, U, and V through all three supported
entry points:

- `tools/validate_8bit_444_corpus.py` covers the freestanding Windows decoder;
- `tools/validate_8bit_444_public.py` packs canonical HEIF and covers
  `av2h_query` plus `av2h_decode`; and
- `tools/validate_8bit_444_wasm.py` executes the browser WebAssembly ABI.

The Windows CTest gate also runs the maximum-size monochrome corpus and the
filter parity I420 parity matrix. The focused coefficient and reconstruction
units pin cropped-edge V contexts and AVM's 64-sample chroma directional
reference limits.

Build and validate the browser module with:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
python3 experimental/wasm/build.py --output-directory build/wasm-validation
python -m pip install wasmtime
python .\tools\validate_8bit_444_wasm.py --wasm .\build\wasm-validation\decoder.wasm
```

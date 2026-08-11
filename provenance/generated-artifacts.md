# Generated artifact provenance

All laboratory artifacts are generated below ignored `build/` paths and are
not source inputs to the freestanding core.

artifact laboratory source planes come from the project-owned deterministic generator
`tools/generate_i420_fixtures.py`. Pinned AVM v1.0.0 commit
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d` generates the original AV2 OBU
streams and decodes original and extracted logical streams. AVM retains its
upstream BSD-3-Clause license and is not incorporated into generated CC0 source.

Project-owned hosted tools normalize OBU carriage and use the project-owned CC0
core to mux, inspect, and extract canonical AV2-HEIF experimental v0 files. The
manifest at `build/artifact-lab/manifest.json` records source, binary, command,
configuration, and artifact hashes needed to identify a particular run.

Generated encoded bytes are test evidence, not normative format definitions or
embedded production fixtures. Reproducing them on another host may produce
different hosted binary hashes; the manifest records the exact tool identities
for that reason.

The 10-bit source corpus under `images/artifacts/10bit/` is derived from three
project-author-owned Google Pixel 8 Pro DNGs captured with the main and
ultrawide camera modules. The untracked source paths, source SHA-256 hashes,
raw metadata, development settings, color contract, plane layouts, tool
versions, and generated hashes are recorded in that directory's README and
manifest. `tools/generate_10bit_sources.py` reproduces the corpus using
ImageMagick's RAW decoder and FFmpeg; neither hosted tool is linked into the
freestanding product.

The retained 10B-WP6 pre-filter oracles under
`tests/fixtures/10bit/avm-reference/` were captured from AVM's
`prefiltered_frame_buffer` at pinned revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`. They are complete little-endian
10-bit Y/U/V planes before deblock or any later filter and are intentionally
separate from the generated `*-decoded.yuv` outputs. Their exact hashes are
recorded in `tests/fixtures/10bit/wp6-unfiltered.sha256`; production code does
not consume these test fixtures.
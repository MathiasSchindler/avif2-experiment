# AV2 quality viewer assets

The GitHub Pages application in `docs/` compares AV2 encodes of the same
Palazzo Pitti ceiling image and decodes local OBU files. AV2 frames are decoded
locally by the project WebAssembly decoder. Each scene also has browser-decoded
JPEG, WebP, and AVIF reference series at quality 10 through 90 in steps of 10,
plus quality 95. A logarithmic bits-per-pixel control selects the nearest
available AV2 and reference file. These formats use different quality scales;
matching bitrate does not imply matching visual quality. The two generated AV2
image sets cover QP 16 through QP 240 in steps of 16:

- the complete ceiling scaled to 960 x 960 pixels;
- a 1920 x 1920 source crop, rotated and scaled to 960 x 960 pixels for close
  inspection of lettering, stucco edges, and repeated texture.

## Build

The source derivative is stored at `source/palacio-pitti-2000.jpg`. Build the
current decoder module and validate the deployed page and existing assets with:

```sh
make wasm
```

This rebuilds the interface and decoder, then validates the existing OBU and
reference assets. Regenerate the complete asset series explicitly with:

```sh
make wasm VIEWER_BUILD_ARGS=
```

Asset regeneration requires FFmpeg, ImageMagick, and the pinned AVM encoder.
Resume an interrupted asset build without re-encoding completed files:

```sh
make wasm VIEWER_BUILD_ARGS=--resume-assets
```

The same commands work on macOS, Linux, and Windows. Use `--clang` or
`--encoder` to select tools outside `PATH` and the default
`build/reference/avm/` directory.

The build finishes by checking all 30 OBUs and 60 reference images under
`docs/assets/`, including sizes, hashes, format metadata, the current
WebAssembly module, strictly increasing quality-series sizes, and the absence
of intermediate I420 files.

Serve the result over HTTP:

```sh
python -m http.server 8080 --directory docs
```

Open <http://localhost:8080>. GitHub Pages publishes the same directory at
<https://mathiasschindler.github.io/avif2-experiment/>.

## Image license

The visual assets are derivatives of "Palacio Pitti, Florencia, Italia,
2022-09-18, DD 163-165 HDR.jpg" by Diego Delso, delso.photo, licensed under
[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/legalcode). The
[source file and full description are on Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Palacio_Pitti,_Florencia,_Italia,_2022-09-18,_DD_163-165_HDR.jpg).

The derivatives are distributed under the same CC BY-SA 4.0 license. Changes:
the Commons source derivative was scaled, cropped, rotated, converted to 8-bit
I420, and encoded as AV2, JPEG, WebP, and AVIF.
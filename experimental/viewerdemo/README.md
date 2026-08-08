# AV2 quality viewer experiment

This browser demo compares AV2 encodes of the same Palazzo Pitti ceiling image.
Both sides are decoded locally by the project WebAssembly decoder. The two image
sets cover QP 16 through QP 240 in steps of 16:

- the complete ceiling scaled to 960 x 960 pixels;
- a 1920 x 1920 source crop, rotated and scaled to 960 x 960 pixels for close
  inspection of lettering, stucco edges, and repeated texture.

## Build

The source derivative is stored at `source/palacio-pitti-2000.jpg`. Generate the
OBU series, site manifest, and current decoder WebAssembly module with:

```sh
make wasm
```

This rebuilds the interface and decoder, then validates the existing OBU
assets. Regenerate the complete asset series explicitly with:

```sh
make wasm VIEWER_BUILD_ARGS=
```

Resume an interrupted asset build without re-encoding completed OBU files:

```sh
make wasm VIEWER_BUILD_ARGS=--resume-assets
```

The same commands work on macOS, Linux, and Windows. Use `--clang` or
`--encoder` to select tools outside `PATH` and the default
`build/reference/avm/` directory.

The build finishes by checking all 30 manifest entries, OBU sizes and hashes,
the current WebAssembly module, and the absence of intermediate I420 files.

Serve the result over HTTP:

```sh
python -m http.server 8081 --directory experimental/viewerdemo/site
```

## Image license

The visual assets are derivatives of "Palacio Pitti, Florencia, Italia,
2022-09-18, DD 163-165 HDR.jpg" by Diego Delso, delso.photo, licensed under
[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/legalcode). The
[source file and full description are on Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Palacio_Pitti,_Florencia,_Italia,_2022-09-18,_DD_163-165_HDR.jpg).

The derivatives are distributed under the same CC BY-SA 4.0 license. Changes:
the Commons source derivative was scaled, cropped, rotated, converted to 8-bit
I420, and encoded as AV2.
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

```powershell
.\experimental\viewerdemo\build.ps1
```

After the first asset build, rebuild only the interface and decoder with:

```powershell
.\experimental\viewerdemo\build.ps1 -SkipAssets
```

Resume an interrupted asset build without re-encoding completed OBU files:

```powershell
.\experimental\viewerdemo\build.ps1 -ResumeAssets
```

The build finishes by checking all 30 manifest entries, OBU sizes and hashes,
the current WebAssembly module, and the absence of intermediate I420 files.

Serve the result over HTTP:

```powershell
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
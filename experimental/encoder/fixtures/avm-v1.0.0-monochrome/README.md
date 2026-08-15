# Constant monochrome AVM fixtures

These project-authored inputs and oracle outputs are released under CC0-1.0.
Each raw I420 input is constant 128 in every plane. AVM's raw reader requires
the neutral chroma bytes even with `--monochrome`; decoded outputs are I400.
`manifest.json` pins tools and commands and records all retained and decoded
hashes. The OBU files are oracle data only and are not encoder implementation.

Inspect boundaries and MSB-first payload bits with:

```sh
python3 experimental/encoder/tools/obu_inspect.py --bits \
  experimental/encoder/fixtures/avm-v1.0.0-monochrome/constant-128-8x8.obu
```

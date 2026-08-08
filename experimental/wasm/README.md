# Browser WebAssembly decoder experiment

This experiment compiles the freestanding AV2 decoder directly to WebAssembly
and displays a dropped `.obu` frame in a browser canvas. Decoding and conversion
from 8-bit I400, I420, or I444 planar output to canvas pixels remain local to
the browser.

## Build

From the repository root:

```sh
make wasm
```

The build requires Python and an LLVM installation containing the WebAssembly
backend and `wasm-ld`. It rebuilds the quality viewer around the existing OBU
assets, validates the result, then copies the shared module to
`docs/decoder.wasm` for GitHub Pages. The C wrapper and portable Python build
script remain in this directory. To build only the module or select a different
compiler or output directory, invoke the script directly:

```sh
python3 experimental/wasm/build.py \
	--clang /path/to/clang \
	--output-directory build/wasm-validation
```

## Run

The browser must load the module over HTTP rather than `file://`:

```sh
python -m http.server 8080 --directory docs
```

Open <http://localhost:8080>, then drop a single-frame closed-loop-key `.obu` file onto the page.

The wrapper accepts inputs up to 64 MiB and allows WebAssembly memory to grow to 512 MiB. The current decoder supports the same bitstream surface as the native decoder.

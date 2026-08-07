# Browser WebAssembly decoder experiment

This experiment compiles the freestanding AV2 decoder directly to WebAssembly
and displays a dropped `.obu` frame in a browser canvas. Decoding, I420 or
monochrome conversion, and 24-bit BMP export remain local to the browser.

## Build

From the repository root:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\experimental\wasm\build.ps1
```

The build requires an LLVM installation containing the WebAssembly backend and `wasm-ld`. It produces `docs/decoder.wasm` for GitHub Pages. The C wrapper and build script remain in this directory.

## Run

The browser must load the module over HTTP rather than `file://`:

```powershell
python -m http.server 8080 --directory docs
```

Open <http://localhost:8080>, then drop a single-frame closed-loop-key `.obu` file onto the page.

The wrapper accepts inputs up to 64 MiB and allows WebAssembly memory to grow to 512 MiB. The current decoder supports the same bitstream surface as the native decoder.

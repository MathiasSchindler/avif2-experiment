const elements = {
  runtimeStatus: document.querySelector("#runtime-status"),
  dropZone: document.querySelector("#drop-zone"),
  fileInput: document.querySelector("#file-input"),
  fileDetails: document.querySelector("#file-details"),
  fileName: document.querySelector("#file-name"),
  fileSize: document.querySelector("#file-size"),
  frameSize: document.querySelector("#frame-size"),
  decodeTime: document.querySelector("#decode-time"),
  message: document.querySelector("#message"),
  downloadButton: document.querySelector("#download-button"),
  emptyPreview: document.querySelector("#empty-preview"),
  canvas: document.querySelector("#canvas"),
};

const statusNames = [
  "OK",
  "invalid argument",
  "truncated input",
  "invalid data",
  "unsupported feature",
  "integer overflow",
  "out of memory",
  "not found",
  "limit exceeded",
];

let wasm;
let decodedBlob;
let decodedName;

function formatBytes(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MiB`;
}

function setMessage(text, kind = "") {
  elements.message.textContent = text;
  elements.message.className = `message ${kind}`.trim();
}

function clampByte(value) {
  return value < 0 ? 0 : value > 255 ? 255 : value;
}

function renderI420(i420, width, height) {
  const chromaWidth = Math.ceil(width / 2);
  const chromaHeight = Math.ceil(height / 2);
  const lumaSize = width * height;
  const chromaSize = chromaWidth * chromaHeight;
  const uOffset = lumaSize;
  const vOffset = uOffset + chromaSize;
  const context = elements.canvas.getContext("2d", { alpha: false });
  const image = context.createImageData(width, height);
  const rgba = image.data;

  for (let row = 0; row < height; row += 1) {
    const yRow = row * width;
    const uvRow = Math.floor(row / 2) * chromaWidth;
    for (let column = 0; column < width; column += 1) {
      const y = i420[yRow + column] - 16;
      const uvIndex = uvRow + Math.floor(column / 2);
      const cb = i420[uOffset + uvIndex] - 128;
      const cr = i420[vOffset + uvIndex] - 128;
      const destination = (yRow + column) * 4;
      rgba[destination] = clampByte((298 * y + 409 * cr + 128) >> 8);
      rgba[destination + 1] = clampByte((298 * y - 100 * cb - 208 * cr + 128) >> 8);
      rgba[destination + 2] = clampByte((298 * y + 516 * cb + 128) >> 8);
      rgba[destination + 3] = 255;
    }
  }

  elements.canvas.width = width;
  elements.canvas.height = height;
  context.putImageData(image, 0, 0);
  elements.canvas.hidden = false;
  elements.emptyPreview.hidden = true;
}

async function decodeFile(file) {
  if (!wasm || !file) return;
  elements.dropZone.setAttribute("aria-disabled", "true");
  elements.fileDetails.hidden = false;
  elements.fileName.textContent = file.name;
  elements.fileSize.textContent = formatBytes(file.size);
  elements.frameSize.textContent = "Decoding";
  elements.decodeTime.textContent = "—";
  elements.downloadButton.disabled = true;
  elements.canvas.hidden = true;
  elements.emptyPreview.hidden = false;
  decodedBlob = undefined;
  setMessage("Decoding in WebAssembly…");

  await new Promise(requestAnimationFrame);
  const started = performance.now();
  try {
    const input = new Uint8Array(await file.arrayBuffer());
    wasm.av2h_reset();
    const inputPointer = wasm.av2h_alloc(input.byteLength);
    if (!inputPointer) throw new Error("The input is too large for decoder memory.");
    new Uint8Array(wasm.memory.buffer, inputPointer, input.byteLength).set(input);
    const status = wasm.av2h_decode(inputPointer, input.byteLength);
    const resultPointer = wasm.av2h_result_pointer();
    const result = new Uint32Array(wasm.memory.buffer, resultPointer, 7);
    const [outputPointer, outputSize, width, height, resultStatus, cause, stage] = result;
    const elapsed = performance.now() - started;
    elements.decodeTime.textContent = `${elapsed.toFixed(1)} ms`;

    if (status !== 0 || resultStatus !== 0) {
      const name = statusNames[resultStatus] ?? `status ${resultStatus}`;
      throw new Error(`Decode failed: ${name} (cause ${cause}, stage ${stage}).`);
    }

    const i420 = new Uint8Array(outputSize);
    i420.set(new Uint8Array(wasm.memory.buffer, outputPointer, outputSize));
    renderI420(i420, width, height);
    elements.frameSize.textContent = `${width} × ${height}`;
    decodedBlob = new Blob([i420], { type: "application/octet-stream" });
    decodedName = `${file.name.replace(/\.obu$/i, "")}-${width}x${height}.i420`;
    elements.downloadButton.disabled = false;
    setMessage(`Decoded ${formatBytes(outputSize)} of planar I420 pixels.`, "success");
  } catch (error) {
    elements.frameSize.textContent = "Failed";
    setMessage(error instanceof Error ? error.message : String(error), "error");
  } finally {
    elements.dropZone.removeAttribute("aria-disabled");
    elements.fileInput.value = "";
  }
}

for (const eventName of ["dragenter", "dragover"]) {
  elements.dropZone.addEventListener(eventName, event => {
    event.preventDefault();
    elements.dropZone.classList.add("dragging");
  });
}
for (const eventName of ["dragleave", "drop"]) {
  elements.dropZone.addEventListener(eventName, event => {
    event.preventDefault();
    elements.dropZone.classList.remove("dragging");
  });
}
elements.dropZone.addEventListener("drop", event => {
  decodeFile(event.dataTransfer?.files[0]);
});
elements.dropZone.addEventListener("keydown", event => {
  if (event.key === "Enter" || event.key === " ") {
    event.preventDefault();
    elements.fileInput.click();
  }
});
elements.fileInput.addEventListener("change", () => decodeFile(elements.fileInput.files[0]));
elements.downloadButton.addEventListener("click", () => {
  if (!decodedBlob) return;
  const link = document.createElement("a");
  const url = URL.createObjectURL(decodedBlob);
  link.href = url;
  link.download = decodedName;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
});

try {
  const response = await fetch("decoder.wasm");
  const bytes = await response.arrayBuffer();
  const instance = await WebAssembly.instantiate(bytes, {});
  wasm = instance.instance.exports;
  elements.runtimeStatus.textContent = "Decoder ready";
  elements.runtimeStatus.classList.add("ready");
} catch (error) {
  elements.runtimeStatus.textContent = "Decoder unavailable";
  elements.runtimeStatus.classList.add("error");
  setMessage(`Could not load decoder.wasm: ${error.message}`, "error");
}

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

function renderMonochrome(luma, width, height) {
  const expectedSize = width * height;
  if (luma.byteLength !== expectedSize) {
    throw new Error(`Unexpected monochrome output size: ${luma.byteLength} bytes.`);
  }
  const context = elements.canvas.getContext("2d", { alpha: false });
  const image = context.createImageData(width, height);
  const rgba = image.data;

  for (let index = 0; index < expectedSize; index += 1) {
    const gray = clampByte((298 * (luma[index] - 16) + 128) >> 8);
    const destination = index * 4;
    rgba[destination] = gray;
    rgba[destination + 1] = gray;
    rgba[destination + 2] = gray;
    rgba[destination + 3] = 255;
  }

  elements.canvas.width = width;
  elements.canvas.height = height;
  context.putImageData(image, 0, 0);
  elements.canvas.hidden = false;
  elements.emptyPreview.hidden = true;
  return image;
}

function renderColorPlanar(planar, width, height, chromaFormat) {
  const chromaWidth = chromaFormat === 2 ? width : Math.ceil(width / 2);
  const chromaHeight = chromaFormat === 2 ? height : Math.ceil(height / 2);
  const lumaSize = width * height;
  const chromaSize = chromaWidth * chromaHeight;
  const uOffset = lumaSize;
  const vOffset = uOffset + chromaSize;
  const context = elements.canvas.getContext("2d", { alpha: false });
  const image = context.createImageData(width, height);
  const rgba = image.data;

  for (let row = 0; row < height; row += 1) {
    const yRow = row * width;
    const uvRow = (chromaFormat === 2 ? row : Math.floor(row / 2)) * chromaWidth;
    for (let column = 0; column < width; column += 1) {
      const y = planar[yRow + column] - 16;
      const uvIndex = uvRow + (chromaFormat === 2 ? column : Math.floor(column / 2));
      const cb = planar[uOffset + uvIndex] - 128;
      const cr = planar[vOffset + uvIndex] - 128;
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
  return image;
}

function renderPlanar(output, width, height, channelCount, chromaFormat) {
  if (channelCount === 1) return renderMonochrome(output, width, height);
  if (channelCount === 3) {
    return renderColorPlanar(output, width, height, chromaFormat);
  }
  throw new Error(`Unsupported decoded channel count: ${channelCount}.`);
}

function createBmp(image, width, height) {
  const rowSize = Math.ceil((width * 3) / 4) * 4;
  const pixelSize = rowSize * height;
  const bytes = new Uint8Array(54 + pixelSize);
  const header = new DataView(bytes.buffer);
  header.setUint16(0, 0x4d42, true);
  header.setUint32(2, bytes.byteLength, true);
  header.setUint32(10, 54, true);
  header.setUint32(14, 40, true);
  header.setInt32(18, width, true);
  header.setInt32(22, height, true);
  header.setUint16(26, 1, true);
  header.setUint16(28, 24, true);
  header.setUint32(34, pixelSize, true);

  for (let row = 0; row < height; row += 1) {
    const sourceRow = height - row - 1;
    const destinationRow = 54 + row * rowSize;
    for (let column = 0; column < width; column += 1) {
      const source = (sourceRow * width + column) * 4;
      const destination = destinationRow + column * 3;
      bytes[destination] = image.data[source + 2];
      bytes[destination + 1] = image.data[source + 1];
      bytes[destination + 2] = image.data[source];
    }
  }
  return new Blob([bytes], { type: "image/bmp" });
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
    const result = new Uint32Array(wasm.memory.buffer, resultPointer, 9);
    const [
      outputPointer,
      outputSize,
      width,
      height,
      resultStatus,
      cause,
      stage,
      channelCount,
      chromaFormat,
    ] = result;
    const elapsed = performance.now() - started;
    elements.decodeTime.textContent = `${elapsed.toFixed(1)} ms`;

    if (status !== 0 || resultStatus !== 0) {
      const name = statusNames[resultStatus] ?? `status ${resultStatus}`;
      throw new Error(`Decode failed: ${name} (cause ${cause}, stage ${stage}).`);
    }

    const output = new Uint8Array(outputSize);
    output.set(new Uint8Array(wasm.memory.buffer, outputPointer, outputSize));
    const image = renderPlanar(
      output, width, height, channelCount, chromaFormat);
    const pixelFormat = channelCount === 1 ? "monochrome" :
      chromaFormat === 2 ? "I444" : "I420";
    elements.frameSize.textContent = `${width} × ${height} · ${pixelFormat}`;
    decodedBlob = createBmp(image, width, height);
    decodedName = `${file.name.replace(/\.obu$/i, "")}-${width}x${height}.bmp`;
    elements.downloadButton.disabled = false;
    setMessage(`Decoded ${formatBytes(outputSize)} of planar pixels.`, "success");
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

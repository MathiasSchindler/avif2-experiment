const elements = {
  runtimeStatus: document.querySelector("#runtime-status"),
  fileInput: document.querySelector("#file-input"),
  samplesButton: document.querySelector("#samples-button"),
  downloadButton: document.querySelector("#download-button"),
  sceneButtons: document.querySelector("#scene-buttons"),
  leftSlider: document.querySelector("#left-slider"),
  rightSlider: document.querySelector("#right-slider"),
  leftQp: document.querySelector("#left-qp"),
  rightQp: document.querySelector("#right-qp"),
  rightMode: document.querySelector("#right-mode"),
  manualLeftQp: document.querySelector("#manual-left-qp"),
  manualRightQp: document.querySelector("#manual-right-qp"),
  zoomSlider: document.querySelector("#zoom-slider"),
  zoomValue: document.querySelector("#zoom-value"),
  stage: document.querySelector("#image-stage"),
  dropTarget: document.querySelector("#drop-target"),
  leftCanvas: document.querySelector("#left-canvas"),
  rightCanvas: document.querySelector("#right-canvas"),
  rightLayer: document.querySelector("#right-layer"),
  splitLine: document.querySelector("#split-line"),
  loading: document.querySelector("#loading"),
  leftSize: document.querySelector("#left-size"),
  rightSize: document.querySelector("#right-size"),
  leftTime: document.querySelector("#left-time"),
  rightTime: document.querySelector("#right-time"),
  sizeDifference: document.querySelector("#size-difference"),
  frameSize: document.querySelector("#frame-size"),
  pixelFormat: document.querySelector("#pixel-format"),
  baselineOptions: document.querySelector("#baseline-options"),
  baselineNote: document.querySelector("#baseline-note"),
  bppSlider: document.querySelector("#bpp-slider"),
  bppValue: document.querySelector("#bpp-value"),
  bppMin: document.querySelector("#bpp-min"),
  bppMax: document.querySelector("#bpp-max"),
  manualControls: document.querySelector(".manual-controls"),
  sourceDescription: document.querySelector("#source-description"),
  commonsLink: document.querySelector("#commons-link"),
};

const statusNames = [
  "OK", "invalid argument", "truncated input", "invalid data",
  "integer overflow", "limit exceeded", "out of memory",
  "unsupported feature",
];

const decodeStageNames = [
  "none", "initialization", "luma parsing", "luma reconstruction",
  "luma deblock", "luma commit", "chroma parsing",
  "chroma cross-transform", "chroma reconstruction", "chroma deblock",
  "chroma commit", "completeness check", "filtering", "output",
];

let wasm;
let manifest;
let sceneIndex = 0;
let decodeQueue = Promise.resolve();
let updateToken = 0;
let localSource;
let localSequence = 0;
let renderedLeft;
let rightBaselineId;
let selectedBaselineFormat = "avif";
let targetBpp = 1;
let bppMatching = true;
const imageCache = new Map();

function formatBytes(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MiB`;
}

function formatSignedBytes(bytes) {
  if (bytes === 0) return "0 B";
  return `${bytes > 0 ? "+" : "-"}${formatBytes(Math.abs(bytes))}`;
}

function bitsPerPixel(item, scene) {
  return item.bytes * 8 / (scene.width * scene.height);
}

function formatBpp(value) {
  return `${value < 1 ? value.toFixed(3) : value.toFixed(2)} bpp`;
}

function closestByBpp(items, scene, target) {
  return items.reduce((closest, item) =>
    Math.abs(bitsPerPixel(item, scene) - target)
      < Math.abs(bitsPerPixel(closest, scene) - target) ? item : closest
  );
}

function copyCanvas(source, destination) {
  destination.width = source.width;
  destination.height = source.height;
  destination.getContext("2d", { alpha: false }).drawImage(source, 0, 0);
}

function planarToCanvas(
  planar, width, height, channelCount, chromaFormat, bitDepth = 8
) {
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  const context = canvas.getContext("2d", { alpha: false });
  const image = context.createImageData(width, height);
  const rgba = image.data;
  const chromaWidth = chromaFormat === 2 ? width : Math.ceil(width / 2);
  const chromaHeight = chromaFormat === 2 ? height : Math.ceil(height / 2);
  const uOffset = width * height;
  const vOffset = uOffset + chromaWidth * chromaHeight;
  const sampleShift = bitDepth - 8;
  const sample = (index) => planar[index] >> sampleShift;

  for (let row = 0; row < height; row += 1) {
    const yRow = row * width;
    const uvRow = (chromaFormat === 2 ? row : Math.floor(row / 2)) * chromaWidth;
    for (let column = 0; column < width; column += 1) {
      const y = sample(yRow + column) - 16;
      const uvIndex = uvRow + (chromaFormat === 2 ? column : Math.floor(column / 2));
      const cb = channelCount === 1 ? 0 : sample(uOffset + uvIndex) - 128;
      const cr = channelCount === 1 ? 0 : sample(vOffset + uvIndex) - 128;
      const target = (yRow + column) * 4;
      rgba[target] = Math.max(0, Math.min(255, (298 * y + 409 * cr + 128) >> 8));
      rgba[target + 1] = Math.max(0, Math.min(255, (298 * y - 100 * cb - 208 * cr + 128) >> 8));
      rgba[target + 2] = Math.max(0, Math.min(255, (298 * y + 516 * cb + 128) >> 8));
      rgba[target + 3] = 255;
    }
  }
  context.putImageData(image, 0, 0);
  return canvas;
}

function sampleSource(scene, variant) {
  return {
    kind: "av2",
    key: `${scene.id}:${variant.qp}`,
    name: `QP ${variant.qp}`,
    bytes: variant.bytes,
    qp: variant.qp,
    async read() {
      const response = await fetch(`assets/${variant.file}`);
      if (!response.ok) throw new Error(`Could not load ${variant.file}.`);
      return new Uint8Array(await response.arrayBuffer());
    },
  };
}

function baselineSource(scene, baseline) {
  return {
    kind: "image",
    key: `${scene.id}:${baseline.id}`,
    name: `${baseline.format} Q${baseline.quality}`,
    bytes: baseline.bytes,
    format: baseline.format,
    quality: baseline.quality,
    setting: baseline.setting,
    file: baseline.file,
  };
}

function baselinesForFormat(scene, formatId) {
  return scene.baselines.filter(item => item.id.startsWith(`${formatId}-`));
}

function matchingRange(scene) {
  const baselines = baselinesForFormat(scene, selectedBaselineFormat);
  const av2Bpp = scene.variants.map(item => bitsPerPixel(item, scene));
  const baselineBpp = baselines.map(item => bitsPerPixel(item, scene));
  return {
    minimum: Math.max(Math.min(...av2Bpp), Math.min(...baselineBpp)),
    maximum: Math.min(Math.max(...av2Bpp), Math.max(...baselineBpp)),
  };
}

function sliderToBpp(value, range) {
  const position = value / 1000;
  return range.minimum * (range.maximum / range.minimum) ** position;
}

function bppToSlider(value, range) {
  return Math.round(
    Math.log(value / range.minimum) / Math.log(range.maximum / range.minimum) * 1000
  );
}

function updateBppControls(scene) {
  const range = matchingRange(scene);
  elements.bppSlider.value = bppToSlider(targetBpp, range);
  elements.bppValue.textContent = formatBpp(targetBpp);
  elements.bppMin.textContent = formatBpp(range.minimum);
  elements.bppMax.textContent = formatBpp(range.maximum);
  elements.bppSlider.setAttribute("aria-valuetext", formatBpp(targetBpp));
  for (const button of elements.baselineOptions.children) {
    const candidates = baselinesForFormat(scene, button.dataset.format);
    const closest = closestByBpp(candidates, scene, targetBpp);
    button.dataset.baseline = closest.id;
    button.title = `Match ${closest.format} ${closest.setting} at ${formatBpp(bitsPerPixel(closest, scene))}.`;
    button.querySelector("strong").textContent =
      `Q${closest.quality} / ${formatBpp(bitsPerPixel(closest, scene))}`;
    button.querySelector("small").textContent =
      `${formatBytes(closest.bytes)} / ${closest.setting.split(" / ")[1]}`;
    button.classList.toggle(
      "selected-b", bppMatching && button.dataset.format === selectedBaselineFormat
    );
    button.setAttribute(
      "aria-pressed", String(bppMatching && button.dataset.format === selectedBaselineFormat)
    );
  }
}

function syncBppMatch(fromSlider = false) {
  const scene = manifest.scenes[sceneIndex];
  const range = matchingRange(scene);
  if (fromSlider) {
    targetBpp = sliderToBpp(Number(elements.bppSlider.value), range);
  }
  targetBpp = Math.max(range.minimum, Math.min(range.maximum, targetBpp));
  const av2 = closestByBpp(scene.variants, scene, targetBpp);
  const baseline = closestByBpp(
    baselinesForFormat(scene, selectedBaselineFormat), scene, targetBpp
  );
  elements.leftSlider.value = scene.variants.indexOf(av2);
  rightBaselineId = baseline.id;
  if (fromSlider) elements.manualControls.open = false;
  updateBppControls(scene);
}

async function decodeAv2Source(source) {
  const input = await source.read();
  wasm.av2h_reset();
  const pointer = wasm.av2h_alloc(input.byteLength);
  if (!pointer) throw new Error("The decoder could not allocate input memory.");
  new Uint8Array(wasm.memory.buffer, pointer, input.byteLength).set(input);
  const started = performance.now();
  const status = wasm.av2h_decode(pointer, input.byteLength);
  const resultPointer = wasm.av2h_result_pointer();
  const result = new Uint32Array(wasm.memory.buffer, resultPointer, 12);
  const [
    outputPointer, outputSize, width, height, resultStatus, cause, stage,
    channelCount, chromaFormat, bitDepth, bytesPerSample, byteOrder,
  ] = result;
  const elapsed = performance.now() - started;
  if (status !== 0 || resultStatus !== 0) {
    const name = statusNames[resultStatus] ?? `status ${resultStatus}`;
    const causeName = statusNames[cause] ?? `cause ${cause}`;
    const stageName = decodeStageNames[stage] ?? `stage ${stage}`;
    throw new Error(
      `${source.name}: ${name} (cause: ${causeName}; stage: ${stageName}).`
    );
  }
  const pixels = new Uint8Array(outputSize);
  pixels.set(new Uint8Array(wasm.memory.buffer, outputPointer, outputSize));
  const samples = bytesPerSample === 2
    ? new Uint16Array(pixels.buffer) : pixels;
  return {
    canvas: planarToCanvas(
      samples, width, height, channelCount, chromaFormat, bitDepth
    ),
    width,
    height,
    channelCount,
    chromaFormat,
    pixelFormat: bytesPerSample === 2 && byteOrder === 1
      ? chromaFormat === 2 ? "10-bit yuv444p10le" : "10-bit yuv420p10le"
      : channelCount === 1
        ? "8-bit I400" : chromaFormat === 2 ? "8-bit I444" : "8-bit I420",
    elapsed,
    source,
  };
}

async function decodeImageSource(source) {
  const response = await fetch(`assets/${source.file}`);
  if (!response.ok) throw new Error(`Could not load ${source.file}.`);
  const blob = await response.blob();
  const url = URL.createObjectURL(blob);
  const image = new Image();
  const started = performance.now();
  try {
    image.src = url;
    await image.decode();
    const canvas = document.createElement("canvas");
    canvas.width = image.naturalWidth;
    canvas.height = image.naturalHeight;
    canvas.getContext("2d", { alpha: false }).drawImage(image, 0, 0);
    return {
      canvas,
      width: image.naturalWidth,
      height: image.naturalHeight,
      channelCount: 3,
      pixelFormat: source.format,
      elapsed: performance.now() - started,
      source,
    };
  } finally {
    URL.revokeObjectURL(url);
  }
}

function decodeSource(source) {
  if (imageCache.has(source.key)) return Promise.resolve(imageCache.get(source.key));
  const operation = () => source.kind === "image"
    ? decodeImageSource(source) : decodeAv2Source(source);
  const queued = source.kind === "image"
    ? operation() : decodeQueue.then(operation, operation);
  if (source.kind !== "image") decodeQueue = queued.catch(() => {});
  return queued.then(decoded => {
    imageCache.set(source.key, decoded);
    if (imageCache.size > 10) imageCache.delete(imageCache.keys().next().value);
    return decoded;
  });
}

function selectedSources() {
  if (localSource) return { left: localSource, right: localSource };
  const scene = manifest.scenes[sceneIndex];
  const baseline = scene.baselines.find(item => item.id === rightBaselineId);
  return {
    left: sampleSource(scene, scene.variants[Number(elements.leftSlider.value)]),
    right: baseline
      ? baselineSource(scene, baseline)
      : sampleSource(scene, scene.variants[Number(elements.rightSlider.value)]),
  };
}

function updateLabels() {
  const { left, right } = selectedSources();
  elements.leftQp.textContent = left.qp ?? "Local";
  elements.manualLeftQp.textContent = left.qp ?? "Local";
  elements.leftQp.setAttribute("aria-label", left.qp ? `A QP ${left.qp}` : "A local file");
  elements.rightMode.textContent = right.kind === "image" ? "" : right.qp ? "AV2 QP" : "";
  elements.rightQp.textContent = right.qp ?? (
    right.kind === "image" ? `${right.format} Q${right.quality}` : "Local"
  );
  elements.manualRightQp.textContent = right.qp ?? "-";
  elements.rightQp.setAttribute(
    "aria-label",
    right.kind === "image" ? `B reference ${right.format} quality ${right.quality}`
      : right.qp ? `B QP ${right.qp}` : "B local file"
  );
  elements.leftSlider.disabled = Boolean(localSource);
  elements.rightSlider.disabled = Boolean(localSource);
  elements.bppSlider.disabled = Boolean(localSource);
  elements.samplesButton.hidden = !localSource;
  elements.leftSize.textContent = formatBytes(left.bytes);
  elements.rightSize.textContent = formatBytes(right.bytes);
  const difference = right.bytes - left.bytes;
  elements.sizeDifference.textContent = formatSignedBytes(difference);
  if (!localSource) updateBppControls(manifest.scenes[sceneIndex]);
}

async function updateImages() {
  const token = ++updateToken;
  const { left, right } = selectedSources();
  updateLabels();
  elements.loading.hidden = false;
  try {
    const [leftDecoded, rightDecoded] = left.key === right.key
      ? await decodeSource(left).then(decoded => [decoded, decoded])
      : await Promise.all([decodeSource(left), decodeSource(right)]);
    if (token !== updateToken) return;
    copyCanvas(leftDecoded.canvas, elements.leftCanvas);
    copyCanvas(rightDecoded.canvas, elements.rightCanvas);
    renderedLeft = leftDecoded;
    elements.downloadButton.disabled = false;
    const leftBpp = left.bytes * 8 / (leftDecoded.width * leftDecoded.height);
    const rightBpp = right.bytes * 8 / (rightDecoded.width * rightDecoded.height);
    elements.leftTime.textContent = `${formatBpp(leftBpp)} / ${leftDecoded.elapsed.toFixed(1)} ms`;
    elements.rightTime.textContent = `${formatBpp(rightBpp)} / ${right.kind === "image" ? "browser " : ""}${rightDecoded.elapsed.toFixed(1)} ms`;
    elements.frameSize.textContent = `${leftDecoded.width} x ${leftDecoded.height}`;
    elements.stage.style.aspectRatio = `${leftDecoded.width} / ${leftDecoded.height}`;
    elements.pixelFormat.textContent = leftDecoded.pixelFormat;
    elements.runtimeStatus.textContent = localSource
      ? `Decoded ${localSource.name}`
      : "Decoder ready";
    elements.runtimeStatus.className = "runtime-status ready";
  } catch (error) {
    elements.runtimeStatus.textContent = error instanceof Error ? error.message : String(error);
    elements.runtimeStatus.className = "runtime-status error";
  } finally {
    if (token === updateToken) elements.loading.hidden = true;
  }
}

function renderControls() {
  const scene = manifest.scenes[sceneIndex];
  renderBaselines();
  if (bppMatching) syncBppMatch();
  updateLabels();
}

function renderBaselines() {
  const scene = manifest.scenes[sceneIndex];
  elements.baselineOptions.replaceChildren();
  elements.baselineNote.textContent = manifest.baseline_encoder.note;
  for (const formatId of ["jpeg", "webp", "avif"]) {
    const baseline = baselinesForFormat(scene, formatId)[0];
    const button = document.createElement("button");
    button.type = "button";
    button.dataset.format = formatId;
    const format = document.createElement("span");
    format.textContent = baseline.format;
    const size = document.createElement("strong");
    size.textContent = "-";
    const setting = document.createElement("small");
    setting.textContent = "-";
    button.append(format, size, setting);
    button.addEventListener("click", () => {
      localSource = undefined;
      bppMatching = true;
      selectedBaselineFormat = formatId;
      elements.manualControls.open = false;
      syncBppMatch();
      updateImages();
    });
    elements.baselineOptions.append(button);
  }
  updateBppControls(scene);
}

function setScene(index) {
  localSource = undefined;
  sceneIndex = index;
  for (const [buttonIndex, button] of [...elements.sceneButtons.children].entries()) {
    button.setAttribute("aria-pressed", String(buttonIndex === index));
  }
  renderControls();
  updateImages();
}

function updateSplit(value) {
  elements.rightLayer.style.clipPath = `inset(0 0 0 ${value}%)`;
  elements.splitLine.style.left = `${value}%`;
  elements.splitLine.setAttribute("aria-valuenow", value);
}

function updateZoom() {
  const value = Number(elements.zoomSlider.value);
  elements.zoomValue.textContent = `${value}%`;
  const transform = `scale(${value / 100})`;
  elements.leftCanvas.style.transform = transform;
  elements.rightCanvas.style.transform = transform;
}

function installStageDragging() {
  let dragging = false;
  const move = event => {
    if (!dragging) return;
    const bounds = elements.stage.getBoundingClientRect();
    const horizontal = Math.max(5, Math.min(95, (event.clientX - bounds.left) / bounds.width * 100));
    const vertical = Math.max(5, Math.min(95, (event.clientY - bounds.top) / bounds.height * 100));
    updateSplit(horizontal);
    const origin = `${horizontal}% ${vertical}%`;
    elements.leftCanvas.style.transformOrigin = origin;
    elements.rightCanvas.style.transformOrigin = origin;
  };
  elements.stage.addEventListener("pointerdown", event => {
    if (event.target.closest(".toolbar")) return;
    event.preventDefault();
    dragging = true;
    elements.stage.setPointerCapture(event.pointerId);
    move(event);
  });
  elements.stage.addEventListener("pointermove", move);
  elements.stage.addEventListener("pointerup", () => { dragging = false; });
  elements.stage.addEventListener("pointercancel", () => { dragging = false; });
}

function createBmp(canvas) {
  const context = canvas.getContext("2d", { alpha: false });
  const image = context.getImageData(0, 0, canvas.width, canvas.height);
  const rowSize = Math.ceil((canvas.width * 3) / 4) * 4;
  const pixelSize = rowSize * canvas.height;
  const bytes = new Uint8Array(54 + pixelSize);
  const header = new DataView(bytes.buffer);
  header.setUint16(0, 0x4d42, true);
  header.setUint32(2, bytes.byteLength, true);
  header.setUint32(10, 54, true);
  header.setUint32(14, 40, true);
  header.setInt32(18, canvas.width, true);
  header.setInt32(22, canvas.height, true);
  header.setUint16(26, 1, true);
  header.setUint16(28, 24, true);
  header.setUint32(34, pixelSize, true);
  for (let row = 0; row < canvas.height; row += 1) {
    const sourceRow = canvas.height - row - 1;
    const destinationRow = 54 + row * rowSize;
    for (let column = 0; column < canvas.width; column += 1) {
      const source = (sourceRow * canvas.width + column) * 4;
      const destination = destinationRow + column * 3;
      bytes[destination] = image.data[source + 2];
      bytes[destination + 1] = image.data[source + 1];
      bytes[destination + 2] = image.data[source];
    }
  }
  return new Blob([bytes], { type: "image/bmp" });
}

async function openLocalFile(file) {
  if (!file || !wasm) return;
  if (!file.name.toLowerCase().endsWith(".obu")) {
    elements.runtimeStatus.textContent = `${file.name}: choose an .obu file.`;
    elements.runtimeStatus.className = "runtime-status error";
    return;
  }
  localSource = {
    kind: "av2",
    key: `local:${++localSequence}`,
    name: file.name,
    bytes: file.size,
    async read() {
      return new Uint8Array(await file.arrayBuffer());
    },
  };
  elements.fileInput.value = "";
  await updateImages();
}

async function initialize() {
  try {
    const [manifestResponse, wasmResponse] = await Promise.all([
      fetch("assets/manifest.json", { cache: "no-cache" }),
      fetch("decoder.wasm?v=20260812-sdp", { cache: "no-cache" }),
    ]);
    if (!manifestResponse.ok || !wasmResponse.ok) throw new Error("Demo assets are unavailable.");
    manifest = await manifestResponse.json();
    const instance = await WebAssembly.instantiate(await wasmResponse.arrayBuffer(), {});
    wasm = instance.instance.exports;
    elements.leftSlider.max = manifest.scenes[0].variants.length - 1;
    elements.rightSlider.max = manifest.scenes[0].variants.length - 1;
    for (const [index, scene] of manifest.scenes.entries()) {
      const button = document.createElement("button");
      button.type = "button";
      button.textContent = scene.name;
      button.setAttribute("aria-pressed", String(index === 0));
      button.addEventListener("click", () => setScene(index));
      elements.sceneButtons.append(button);
    }
    elements.sourceDescription.textContent = manifest.source.description;
    elements.commonsLink.href = manifest.source.commons_url;
    elements.runtimeStatus.textContent = "Decoder ready";
    elements.runtimeStatus.className = "runtime-status ready";
    renderControls();
    await updateImages();
  } catch (error) {
    elements.loading.textContent = "Demo unavailable";
    elements.runtimeStatus.textContent = error instanceof Error ? error.message : String(error);
    elements.runtimeStatus.className = "runtime-status error";
  }
}

elements.leftSlider.addEventListener("input", () => {
  bppMatching = false;
  rightBaselineId = undefined;
  updateLabels();
});
elements.rightSlider.addEventListener("input", () => {
  bppMatching = false;
  rightBaselineId = undefined;
  updateLabels();
});
elements.leftSlider.addEventListener("change", updateImages);
elements.rightSlider.addEventListener("change", updateImages);
elements.bppSlider.addEventListener("input", () => {
  bppMatching = true;
  syncBppMatch(true);
  updateLabels();
});
elements.bppSlider.addEventListener("change", updateImages);
elements.zoomSlider.addEventListener("input", updateZoom);
elements.fileInput.addEventListener("change", () => openLocalFile(elements.fileInput.files[0]));
elements.samplesButton.addEventListener("click", () => setScene(sceneIndex));
elements.downloadButton.addEventListener("click", () => {
  if (!renderedLeft) return;
  const blob = createBmp(renderedLeft.canvas);
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = `${renderedLeft.source.name.replace(/\.obu$/i, "")}-${renderedLeft.width}x${renderedLeft.height}.bmp`;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
});
elements.splitLine.addEventListener("keydown", event => {
  if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
  event.preventDefault();
  const direction = event.key === "ArrowLeft" ? -1 : 1;
  const value = Number(elements.splitLine.getAttribute("aria-valuenow"));
  updateSplit(Math.max(5, Math.min(95, value + direction * 5)));
});
let fileDragDepth = 0;
function hasDraggedFiles(event) {
  return [...(event.dataTransfer?.types ?? [])].includes("Files");
}
window.addEventListener("dragenter", event => {
  if (!hasDraggedFiles(event)) return;
  event.preventDefault();
  fileDragDepth += 1;
  elements.stage.classList.add("dragging");
});
window.addEventListener("dragover", event => {
  if (!hasDraggedFiles(event)) return;
  event.preventDefault();
  if (event.dataTransfer) event.dataTransfer.dropEffect = "copy";
});
window.addEventListener("dragleave", event => {
  if (!hasDraggedFiles(event)) return;
  fileDragDepth = Math.max(0, fileDragDepth - 1);
  if (fileDragDepth === 0) elements.stage.classList.remove("dragging");
});
window.addEventListener("drop", event => {
  if (!hasDraggedFiles(event)) return;
  event.preventDefault();
  fileDragDepth = 0;
  elements.stage.classList.remove("dragging");
  openLocalFile(event.dataTransfer?.files[0]);
});
installStageDragging();
initialize();
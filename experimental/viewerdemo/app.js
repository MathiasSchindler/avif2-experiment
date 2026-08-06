const elements = {
  runtimeStatus: document.querySelector("#runtime-status"),
  sceneButtons: document.querySelector("#scene-buttons"),
  leftSlider: document.querySelector("#left-slider"),
  rightSlider: document.querySelector("#right-slider"),
  leftQp: document.querySelector("#left-qp"),
  rightQp: document.querySelector("#right-qp"),
  splitSlider: document.querySelector("#split-slider"),
  zoomSlider: document.querySelector("#zoom-slider"),
  zoomValue: document.querySelector("#zoom-value"),
  stage: document.querySelector("#image-stage"),
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
  sizeChart: document.querySelector("#size-chart"),
  sceneDescription: document.querySelector("#scene-description"),
  sourceDescription: document.querySelector("#source-description"),
  commonsLink: document.querySelector("#commons-link"),
};

const statusNames = [
  "OK", "invalid argument", "truncated input", "invalid data",
  "unsupported feature", "integer overflow", "out of memory",
  "not found", "limit exceeded",
];

let wasm;
let manifest;
let sceneIndex = 0;
let decodeQueue = Promise.resolve();
let updateToken = 0;
const imageCache = new Map();

function formatBytes(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MiB`;
}

function copyCanvas(source, destination) {
  destination.width = source.width;
  destination.height = source.height;
  destination.getContext("2d", { alpha: false }).drawImage(source, 0, 0);
}

function i420ToCanvas(i420, width, height) {
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  const context = canvas.getContext("2d", { alpha: false });
  const image = context.createImageData(width, height);
  const rgba = image.data;
  const chromaWidth = Math.ceil(width / 2);
  const chromaHeight = Math.ceil(height / 2);
  const uOffset = width * height;
  const vOffset = uOffset + chromaWidth * chromaHeight;

  for (let row = 0; row < height; row += 1) {
    const yRow = row * width;
    const uvRow = Math.floor(row / 2) * chromaWidth;
    for (let column = 0; column < width; column += 1) {
      const y = i420[yRow + column] - 16;
      const uvIndex = uvRow + Math.floor(column / 2);
      const cb = i420[uOffset + uvIndex] - 128;
      const cr = i420[vOffset + uvIndex] - 128;
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

function decodeVariant(scene, variant) {
  const key = `${scene.id}:${variant.qp}`;
  if (imageCache.has(key)) return Promise.resolve(imageCache.get(key));
  const operation = async () => {
    const response = await fetch(variant.file);
    if (!response.ok) throw new Error(`Could not load ${variant.file}.`);
    const input = new Uint8Array(await response.arrayBuffer());
    wasm.av2h_reset();
    const pointer = wasm.av2h_alloc(input.byteLength);
    if (!pointer) throw new Error("The decoder could not allocate input memory.");
    new Uint8Array(wasm.memory.buffer, pointer, input.byteLength).set(input);
    const started = performance.now();
    const status = wasm.av2h_decode(pointer, input.byteLength);
    const resultPointer = wasm.av2h_result_pointer();
    const result = new Uint32Array(wasm.memory.buffer, resultPointer, 7);
    const [outputPointer, outputSize, width, height, resultStatus, cause, stage] = result;
    const elapsed = performance.now() - started;
    if (status !== 0 || resultStatus !== 0) {
      const name = statusNames[resultStatus] ?? `status ${resultStatus}`;
      throw new Error(`${variant.file}: ${name} (cause ${cause}, stage ${stage}).`);
    }
    const pixels = new Uint8Array(outputSize);
    pixels.set(new Uint8Array(wasm.memory.buffer, outputPointer, outputSize));
    const decoded = { canvas: i420ToCanvas(pixels, width, height), elapsed };
    imageCache.set(key, decoded);
    if (imageCache.size > 8) {
      imageCache.delete(imageCache.keys().next().value);
    }
    return decoded;
  };
  const queued = decodeQueue.then(operation, operation);
  decodeQueue = queued.catch(() => {});
  return queued;
}

function selectedVariants() {
  const scene = manifest.scenes[sceneIndex];
  return {
    scene,
    left: scene.variants[Number(elements.leftSlider.value)],
    right: scene.variants[Number(elements.rightSlider.value)],
  };
}

function updateLabels() {
  const { scene, left, right } = selectedVariants();
  elements.leftQp.textContent = left.qp;
  elements.rightQp.textContent = right.qp;
  elements.leftSize.textContent = formatBytes(left.bytes);
  elements.rightSize.textContent = formatBytes(right.bytes);
  const difference = right.bytes - left.bytes;
  const percentage = Math.abs(difference) / Math.max(left.bytes, right.bytes) * 100;
  elements.sizeDifference.textContent = `${difference > 0 ? "+" : difference < 0 ? "-" : ""}${percentage.toFixed(1)}%`;
  elements.frameSize.textContent = `${scene.width} x ${scene.height}`;
  for (const button of elements.sizeChart.children) {
    const qp = Number(button.dataset.qp);
    button.classList.toggle("selected-a", qp === left.qp);
    button.classList.toggle("selected-b", qp === right.qp);
  }
}

async function updateImages() {
  const token = ++updateToken;
  const { scene, left, right } = selectedVariants();
  updateLabels();
  elements.loading.hidden = false;
  try {
    const [leftDecoded, rightDecoded] = await Promise.all([
      decodeVariant(scene, left), decodeVariant(scene, right),
    ]);
    if (token !== updateToken) return;
    copyCanvas(leftDecoded.canvas, elements.leftCanvas);
    copyCanvas(rightDecoded.canvas, elements.rightCanvas);
    elements.leftTime.textContent = `${leftDecoded.elapsed.toFixed(1)} ms decode`;
    elements.rightTime.textContent = `${rightDecoded.elapsed.toFixed(1)} ms decode`;
  } catch (error) {
    elements.runtimeStatus.textContent = error instanceof Error ? error.message : String(error);
    elements.runtimeStatus.className = "runtime-status error";
  } finally {
    if (token === updateToken) elements.loading.hidden = true;
  }
}

function renderChart() {
  const scene = manifest.scenes[sceneIndex];
  const maximum = Math.max(...scene.variants.map(variant => variant.bytes));
  elements.sizeChart.replaceChildren();
  elements.sizeChart.style.setProperty("--variant-count", scene.variants.length);
  for (const [index, variant] of scene.variants.entries()) {
    const button = document.createElement("button");
    button.type = "button";
    button.dataset.qp = variant.qp;
    button.title = `QP ${variant.qp}: ${formatBytes(variant.bytes)}. Set B to this value.`;
    button.style.setProperty("--bar-height", `${Math.max(4, Math.sqrt(variant.bytes / maximum) * 116)}px`);
    button.innerHTML = `<span>${variant.qp}</span>`;
    button.addEventListener("click", () => {
      elements.rightSlider.value = index;
      updateImages();
    });
    elements.sizeChart.append(button);
  }
  elements.sceneDescription.textContent = `${scene.description} ${scene.transformation}`;
  updateLabels();
}

function setScene(index) {
  sceneIndex = index;
  for (const [buttonIndex, button] of [...elements.sceneButtons.children].entries()) {
    button.setAttribute("aria-pressed", String(buttonIndex === index));
  }
  renderChart();
  updateImages();
}

function updateSplit(value) {
  elements.rightLayer.style.clipPath = `inset(0 0 0 ${value}%)`;
  elements.splitLine.style.left = `${value}%`;
  elements.splitSlider.value = value;
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
    dragging = true;
    elements.stage.setPointerCapture(event.pointerId);
    move(event);
  });
  elements.stage.addEventListener("pointermove", move);
  elements.stage.addEventListener("pointerup", () => { dragging = false; });
  elements.stage.addEventListener("pointercancel", () => { dragging = false; });
}

async function initialize() {
  try {
    const [manifestResponse, wasmResponse] = await Promise.all([
      fetch("manifest.json"), fetch("decoder.wasm"),
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
    renderChart();
    await updateImages();
  } catch (error) {
    elements.loading.textContent = "Demo unavailable";
    elements.runtimeStatus.textContent = error instanceof Error ? error.message : String(error);
    elements.runtimeStatus.className = "runtime-status error";
  }
}

elements.leftSlider.addEventListener("input", updateLabels);
elements.rightSlider.addEventListener("input", updateLabels);
elements.leftSlider.addEventListener("change", updateImages);
elements.rightSlider.addEventListener("change", updateImages);
elements.splitSlider.addEventListener("input", event => updateSplit(event.target.value));
elements.zoomSlider.addEventListener("input", updateZoom);
installStageDragging();
initialize();
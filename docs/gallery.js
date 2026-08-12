const elements = {
  status: document.querySelector("#runtime-status"),
  preset: document.querySelector("#preset-select"),
  gallery: document.querySelector("#gallery"),
  revision: document.querySelector("#revision"),
  dialog: document.querySelector("#detail-dialog"),
  detailTitle: document.querySelector("#detail-title"),
  detailCredit: document.querySelector("#detail-credit"),
  detailClose: document.querySelector("#detail-close"),
  detailVariant: document.querySelector("#detail-variant"),
  detailZoom: document.querySelector("#detail-zoom"),
  detailZoomValue: document.querySelector("#detail-zoom-value"),
  detailStage: document.querySelector("#detail-stage"),
  detailCanvas: document.querySelector("#detail-canvas"),
  detailLoading: document.querySelector("#detail-loading"),
  detailSpec: document.querySelector("#detail-spec"),
  detailSize: document.querySelector("#detail-size"),
  detailTime: document.querySelector("#detail-time"),
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
let decodeQueue = Promise.resolve();
let detailSource;
const cards = new Map();
const decoded = new Map();
const decodedCacheLimit = 18;

function formatBytes(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MiB`;
}

function formatVariant(variant) {
  const format = variant.pixel_format === "i400"
    ? "mono" : variant.pixel_format.toUpperCase();
  return `${variant.preparation.slice(2)} edge · ${format} · QP ${variant.qp}`;
}

function copyCanvas(source, destination) {
  destination.width = source.width;
  destination.height = source.height;
  destination.getContext("2d", { alpha: false }).drawImage(source, 0, 0);
}

function planarToCanvas(planar, width, height, channelCount, chromaFormat) {
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
  for (let row = 0; row < height; row += 1) {
    const yRow = row * width;
    const uvRow = (chromaFormat === 2 ? row : Math.floor(row / 2)) * chromaWidth;
    for (let column = 0; column < width; column += 1) {
      const y = planar[yRow + column] - 16;
      const uv = uvRow + (chromaFormat === 2 ? column : Math.floor(column / 2));
      const cb = channelCount === 1 ? 0 : planar[uOffset + uv] - 128;
      const cr = channelCount === 1 ? 0 : planar[vOffset + uv] - 128;
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

async function decodeVariant(source, variant) {
  if (decoded.has(variant.id)) {
    const cached = decoded.get(variant.id);
    decoded.delete(variant.id);
    decoded.set(variant.id, cached);
    return cached;
  }
  const operation = async () => {
    const response = await fetch(`reference/${variant.file}`);
    if (!response.ok) throw new Error(`Could not load ${variant.id}.`);
    const input = new Uint8Array(await response.arrayBuffer());
    wasm.av2h_reset();
    const pointer = wasm.av2h_alloc(input.byteLength);
    if (!pointer) throw new Error(`${source.name}: input allocation failed.`);
    new Uint8Array(wasm.memory.buffer, pointer, input.byteLength).set(input);
    const started = performance.now();
    const status = wasm.av2h_decode(pointer, input.byteLength);
    const values = new Uint32Array(
      wasm.memory.buffer, wasm.av2h_result_pointer(), 12
    );
    const [outputPointer, outputSize, width, height, resultStatus, cause, stage,
      channelCount, chromaFormat, bitDepth] = values;
    if (status !== 0 || resultStatus !== 0) {
      const name = statusNames[resultStatus] ?? `status ${resultStatus}`;
      const causeName = statusNames[cause] ?? `cause ${cause}`;
      const stageName = decodeStageNames[stage] ?? `stage ${stage}`;
      throw new Error(`${source.name}: ${name} (${causeName}; ${stageName}).`);
    }
    if (bitDepth !== 8) throw new Error(`${source.name}: expected 8-bit output.`);
    const planar = new Uint8Array(outputSize);
    planar.set(new Uint8Array(wasm.memory.buffer, outputPointer, outputSize));
    return {
      canvas: planarToCanvas(planar, width, height, channelCount, chromaFormat),
      elapsed: performance.now() - started,
    };
  };
  const result = decodeQueue.then(operation, operation);
  decodeQueue = result.catch(() => {});
  decoded.set(variant.id, result);
  if (decoded.size > decodedCacheLimit) decoded.delete(decoded.keys().next().value);
  return result;
}

function variantForPreset(source) {
  const [preparation, pixelFormat, qp] = elements.preset.value.split("|");
  return source.variants.find(variant =>
    variant.preparation === preparation &&
    variant.pixel_format === pixelFormat &&
    variant.qp === Number(qp)
  );
}

function fillVariants(select, source, selectedId) {
  select.replaceChildren(...source.variants.map(variant => {
    const option = document.createElement("option");
    option.value = variant.id;
    option.textContent = formatVariant(variant);
    option.selected = variant.id === selectedId;
    return option;
  }));
}

async function renderCard(source, variant) {
  const card = cards.get(source.id);
  card.variant.value = variant.id;
  card.size.textContent = formatBytes(variant.bytes);
  card.loading.textContent = "Decoding AV2";
  card.loading.hidden = false;
  card.button.style.aspectRatio = `${variant.width} / ${variant.height}`;
  try {
    const result = await decodeVariant(source, variant);
    if (card.variant.value !== variant.id) return;
    copyCanvas(result.canvas, card.canvas);
    card.canvas.setAttribute(
      "aria-label", `${source.name}, ${formatVariant(variant)}`
    );
  } catch (error) {
    card.loading.textContent = "Decode failed";
    elements.status.textContent = error instanceof Error ? error.message : String(error);
    elements.status.className = "runtime-status error";
    return;
  }
  if (card.variant.value === variant.id) card.loading.hidden = true;
}

function makeLink(label, href) {
  const link = document.createElement("a");
  link.textContent = label;
  link.href = href;
  return link;
}

function createCard(source, index) {
  const item = document.createElement("article");
  item.className = "gallery-item";
  const button = document.createElement("button");
  button.className = "image-button";
  button.type = "button";
  button.setAttribute("aria-label", `Inspect ${source.name}`);
  const canvas = document.createElement("canvas");
  const loading = document.createElement("span");
  loading.className = "loading";
  loading.textContent = "Waiting";
  button.append(canvas, loading);

  const body = document.createElement("div");
  body.className = "item-body";
  const heading = document.createElement("div");
  heading.className = "item-heading";
  const title = document.createElement("div");
  title.className = "item-title";
  const name = document.createElement("h3");
  name.textContent = source.name;
  const creator = document.createElement("p");
  creator.textContent = source.attribution.credit_line;
  title.append(name, creator);
  const number = document.createElement("span");
  number.className = "item-number";
  number.textContent = String(index + 1).padStart(2, "0");
  heading.append(title, number);

  const variantRow = document.createElement("div");
  variantRow.className = "item-variant";
  const variant = document.createElement("select");
  variant.setAttribute("aria-label", `${source.name} variant`);
  fillVariants(variant, source, variantForPreset(source).id);
  const size = document.createElement("span");
  size.className = "item-size";
  variantRow.append(variant, size);

  const links = document.createElement("div");
  links.className = "item-links";
  links.append(
    makeLink(source.creator.name, source.creator.url),
    makeLink("Source", source.commons_url),
    makeLink(source.license.spdx, source.license.url),
    makeLink("Metadata", `reference/${source.metadata}`)
  );
  body.append(heading, variantRow, links);
  item.append(button, body);
  cards.set(source.id, { item, button, canvas, loading, variant, size });
  variant.addEventListener("change", () => {
    const selected = source.variants.find(entry => entry.id === variant.value);
    renderCard(source, selected);
  });
  button.addEventListener("click", () => openDetail(source, variant.value));
  return item;
}

async function updateDetail() {
  const source = detailSource;
  const variant = source.variants.find(
    entry => entry.id === elements.detailVariant.value
  );
  elements.detailLoading.textContent = "Decoding AV2";
  elements.detailLoading.hidden = false;
  elements.detailSpec.textContent = `${variant.width} × ${variant.height} · ${formatVariant(variant)}`;
  elements.detailSize.textContent = formatBytes(variant.bytes);
  try {
    const result = await decodeVariant(source, variant);
    if (source !== detailSource || variant.id !== elements.detailVariant.value) return;
    copyCanvas(result.canvas, elements.detailCanvas);
    elements.detailCanvas.setAttribute("aria-label", source.name);
    elements.detailTime.textContent = `${result.elapsed.toFixed(1)} ms decode`;
    elements.detailZoom.value = "100";
    updateDetailZoom();
    elements.detailStage.scrollTo(0, 0);
  } catch (error) {
    elements.detailLoading.textContent = error instanceof Error ? error.message : String(error);
    return;
  }
  elements.detailLoading.hidden = true;
}

function openDetail(source, variantId) {
  detailSource = source;
  elements.detailTitle.textContent = source.name;
  elements.detailCredit.textContent = source.attribution.credit_line;
  fillVariants(elements.detailVariant, source, variantId);
  elements.dialog.showModal();
  updateDetail();
}

function updateDetailZoom() {
  const zoom = Number(elements.detailZoom.value);
  elements.detailZoomValue.textContent = `${zoom}%`;
  elements.detailCanvas.style.width = `${elements.detailCanvas.width * zoom / 100}px`;
  elements.detailCanvas.style.height = `${elements.detailCanvas.height * zoom / 100}px`;
}

function installLazyDecoding() {
  const observer = new IntersectionObserver(entries => {
    for (const entry of entries) {
      if (!entry.isIntersecting) continue;
      const source = manifest.sources.find(item => item.id === entry.target.dataset.source);
      const card = cards.get(source.id);
      const variant = source.variants.find(item => item.id === card.variant.value);
      renderCard(source, variant);
      observer.unobserve(entry.target);
    }
  }, { rootMargin: "300px" });
  for (const [sourceId, card] of cards) {
    card.item.dataset.source = sourceId;
    observer.observe(card.item);
  }
}

async function initialize() {
  try {
    const [manifestResponse, wasmResponse] = await Promise.all([
      fetch("reference/manifest.json", { cache: "no-cache" }),
      fetch("decoder.wasm?v=20260812-sdp", { cache: "no-cache" }),
    ]);
    if (!manifestResponse.ok || !wasmResponse.ok) throw new Error("Gallery assets are unavailable.");
    manifest = await manifestResponse.json();
    if (manifest.schema !== "av2h-reference-gallery-v1") {
      throw new Error("Unsupported gallery manifest.");
    }
    const instance = await WebAssembly.instantiate(await wasmResponse.arrayBuffer(), {});
    wasm = instance.instance.exports;
    elements.gallery.replaceChildren(...manifest.sources.map(createCard));
    elements.revision.textContent = `AVM ${manifest.avm_revision.slice(0, 12)} · ${manifest.source_count} sources · ${manifest.variant_count} variants`;
    elements.status.textContent = `${manifest.source_count} images ready`;
    elements.status.className = "runtime-status ready";
    installLazyDecoding();
  } catch (error) {
    elements.status.textContent = error instanceof Error ? error.message : String(error);
    elements.status.className = "runtime-status error";
  }
}

elements.preset.addEventListener("change", () => {
  for (const source of manifest.sources) renderCard(source, variantForPreset(source));
});
elements.detailVariant.addEventListener("change", updateDetail);
elements.detailZoom.addEventListener("input", updateDetailZoom);
elements.detailClose.addEventListener("click", () => elements.dialog.close());
elements.dialog.addEventListener("click", event => {
  if (event.target === elements.dialog) elements.dialog.close();
});

initialize();
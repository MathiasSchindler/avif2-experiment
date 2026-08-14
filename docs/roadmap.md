# Active roadmap

**Status:** current unfinished work as of 14 August 2026

Completed implementation plans and release evidence are indexed in
[README.md](README.md). This file contains only work that remains open.

## Reliability and maintainability

1. Keep all configured image-reference locks in the normal test gate and all
   locally present reference streams in the extended exact-parity gate.
2. Move diagnostics-enabled mutable observations from process-global storage into
   caller-owned per-decode state so concurrent decodes do not race.
3. Preserve internal failure provenance instead of translating unsupported
   state or capacity conditions into misleading public statuses.

Every decoder maintenance change must run its focused tests and the complete
native reference parity gate. Release changes also require WebAssembly,
freestanding, no-libc, sanitizer, and relevant platform gates.

## Capability evidence

1. Broaden malformed-input and cross-feature vectors for the admitted TCQ,
   quantization-matrix, ALT_Q segmentation, delta-Q, restoration, Intra-DIP,
   transform-partition, IBP, MRL, and secondary-transform subsets.
2. Add durable platform evidence where a capability currently relies primarily
   on hosted native and WebAssembly corpus parity.
3. Keep the machine-readable suite recipes, locks, capability matrix, and
   public README synchronized when a profile boundary changes.

## Encoder research

The measured encoder backlog remains in
[research/feature.md](research/feature.md). Its
near-term work is to compare target-baseline rate-distortion and decode cost,
broaden restoration and quantization evidence only from observed demand, and
keep production presets explicit rather than relying on AVM defaults.

## Deferred product scope

4:2:2, film grain, color conversion, streaming PNG, container transforms,
metadata expansion, alpha auxiliary items, grids, and bounded parallelism are
separate product slices. Each requires a defined public contract, workspace
accounting, malformed-input tests, exact reference evidence, and documentation
before admission.
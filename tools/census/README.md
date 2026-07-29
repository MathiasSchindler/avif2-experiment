# AV2 feature census

`av2-census` decodes raw `.obu` files with the pinned AVM reference decoder and
emits deterministic JSON containing unique coding-block feature tuples, their
occurrence counts, and their first frame/position.

The tool has no runtime dependency beyond the C runtime. Its only source and
link dependency is the pinned AVM checkout documented in `vendor/README.md`.
It deliberately uses AVM inspection data rather than implementing another AV2
parser.

## Build

Prepare AVM first:

```powershell
./scripts/setup-avm.ps1
```

Configure the census with the same LLVM-MinGW toolchain used by AVM:

```powershell
$toolchainFile = (Resolve-Path `
  vendor/avm/cmake/toolchains/x86_64-mingw-gcc.cmake).Path
cmake -S tools/census -B build/census -G Ninja `
  "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
cmake --build build/census
```

## Use

Write one census to standard output:

```powershell
./build/census/av2-census.exe images/artifacts/emerald-192x96-qp208.obu
```

Aggregate multiple files into one document:

```powershell
./build/census/av2-census.exe -o build/artifact-census.json `
  images/artifacts/emerald-192x96-qp208.obu `
  images/artifacts/schwaebisch-417x277-qp208.obu
```

## Status report

Regenerate `status.md` and its corpus JSON from every `.obu` in
`images/artifacts`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  tools/census/generate_status.ps1
```

Rebuild only the Markdown report from the existing corpus JSON:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  tools/census/generate_status.ps1 -UseExistingCensus
```

The generator accepts overrides for `-CensusExecutable`,
`-ArtifactsDirectory`, `-CensusJson`, `-SupportManifest`, and `-OutputPath`.
Corpus refreshes write to a per-run staging file and replace the canonical JSON
only after a successful decode.

Decoder support classifications live in `support.json`. The reviewed manifest
separates implementation evidence from AVM-observed demand and classifies each
reported family or value as `supported`, `partial`, `unsupported`, or `unknown`.
The generator rejects unknown statuses, missing evidence, unmatched observed
feature values, and support families without a corresponding demand category.
Update the manifest when decoder evidence changes, then regenerate `status.md`.

The `features` array reports coding-block geometry, luma/chroma modes and angle
deltas, MRL and multi-line MRL, FSC, IBP availability, intrabc, transform skip,
primary and secondary transforms, Q index, CfL, palette use, and mode syntax
indices. Non-skipped coding blocks contribute one occurrence per luma transform
origin; skipped coding blocks contribute one occurrence. Entries are ordered by
first occurrence.

The pinned AVM build currently has `CONFIG_ACCOUNTING=0`, so the census does
not yet include entropy-symbol names, CDF contexts, EOB values, or coefficient
levels. Those require rebuilding AVM with accounting enabled; they are not
inferred from reconstructed pixels or transform metadata.

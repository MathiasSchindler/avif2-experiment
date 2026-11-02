# Hosted reference dependencies

Code below this directory is separately licensed reference software. It is not
part of the future CC0 freestanding library and must not appear in production
include paths or link graphs.

Reference checkouts are ignored by Git and obtained locally through explicit
setup scripts. This README remains tracked to document their provenance,
licensing, and build boundaries.

## AVM

- Source: <https://github.com/AOMediaCodec/avm.git>
- Tag: `v1.0.0`
- Commit: `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`
- License: BSD 3-Clause; retain `avm/LICENSE` and all upstream notices
- Purpose: encode one-frame AV2 OBU fixtures and differentially decode original
  and reconstructed logical streams for carriage experiment, and generate and decode
  the offline artifact laboratory artifact matrix
- Setup: `scripts/setup-avm.sh`
- Native Windows setup: `scripts/setup-avm.ps1` (LLVM-MinGW UCRT)
- Build output: `build/reference/avm/avmenc` and
  `build/reference/avm/avmdec` (with `.exe` on Windows)

The setup script checks out only the pinned revision, configures a Release
hosted build, and builds the two command-line tools needed by the experiment.
The carriage experiment manifest records source and binary hashes because compiler and
host differences can change generated executables.

## Moving AVM oracle

The optional second oracle follows upstream `main`; it has no committed
revision pin and does not replace the v1.0.0 oracle.

- Source and license: the same AVM repository and upstream notices as above
- Checkout: `vendor/avm-current`
- Setup/update: `scripts/setup-avm.sh --current`, or
  `scripts/setup-avm.ps1 -Current` on Windows
- Build output: `build/reference/avm-current`; Windows ARM64 uses
  `build/reference/avm-arm64-current`
- Build provenance: `oracle-revision.txt`, written only after a successful build
- Checks: `tools/check_avm_current.py`

Setup refuses local source modifications before fetching or checking out the
new upstream revision. The compatibility check does not fetch, rejects a
modified checkout or stale revision marker, and records the actual source
revision and encoder/decoder binary SHA-256 values for every run. Results are
diagnostic evidence, not replacements for the pinned golden hashes. Neither
AVM checkout is a production decoder dependency.
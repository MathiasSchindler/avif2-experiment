# Hosted reference dependencies

Code below this directory is separately licensed reference software. It is not
part of the future CC0 freestanding library and must not appear in production
include paths or link graphs.

Reference checkouts are ignored by Git and obtained locally through pinned
setup scripts. This README remains tracked to document their provenance,
licensing, and build boundaries.

## AVM

- Source: <https://github.com/AOMediaCodec/avm.git>
- Tag: `v1.0.0`
- Commit: `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`
- License: BSD 3-Clause; retain `avm/LICENSE` and all upstream notices
- Purpose: encode one-frame AV2 OBU fixtures and differentially decode original
  and reconstructed logical streams for Milestone 0, and generate and decode
  the offline Milestone 3 artifact matrix
- Setup: `scripts/setup-avm.sh`
- Build output: `build/reference/avm/avmenc` and
  `build/reference/avm/avmdec`

The setup script checks out only the pinned revision, configures a Release
hosted build, and builds the two command-line tools needed by the experiment.
The Milestone 0 manifest records source and binary hashes because compiler and
host differences can change generated executables.
# Documentation index

This index separates current contracts from active work and historical records.
A completed plan may remain technically useful evidence, but it must not be
read as the current capability boundary.

## Current contract

- [Project overview](../README.md): public formats, build entry points,
  validation summary, security, and licensing.
- [Decoder capabilities](capabilities.md): authoritative supported and
  fail-closed profile boundaries.
- [Security policy](../SECURITY.md): threat model and vulnerability reporting.
- [Local AV2 specification](spec/README.md): project-defined bitstream and
  container interpretation.
- [Image corpus](../images/README.md): provenance, recipes, suites, and locks.

## Active roadmap

- [Current roadmap](roadmap.md): unfinished reliability, maintainability, and
  product work.
- [Coding opportunities](research/feature.md): encoder experiments and research
  backlog. Capability statements there are explanatory; the capability matrix
  remains authoritative.

## Release evidence

Files under `dev/` record focused implementation and validation work. The main
release summaries include:

- [10-bit release gate](dev/10bit-wp14.md)
- [Decoder refactoring plan](dev/decoder-refactoring-plan.md)
- [I444 release evidence](dev/chroma-444.md)
- [Filter parity](dev/filter-parity.md)
- [Advanced prediction](dev/advanced-prediction.md)

These records are append-only evidence unless a factual or link error needs
correction. New current requirements belong in the capability matrix or active
roadmap, not in completed work-package narratives.

## Historical design and completed plans

The following documents are frozen historical records:

- [Original implementation plan](history/plan.md)
- [10-bit implementation plan](history/10bitplan.md)
- [10-bit preparation plan](history/10bitplan-prepwork.md)
- [Dated AV2 still-image design report](history/avif-2.md)

Their status notices point readers back to current documentation. Historical
references use repository-relative paths so evidence remains unambiguous after
the move.

## Documentation rules

1. Update `capabilities.md` whenever a decoder feature is admitted or rejected.
2. Add an active-roadmap item only for unfinished work; move completed results
   to a focused evidence record.
3. Include the exact AVM revision, profile tuple, and parity gate in capability
   claims.
4. Label dated research and completed plans explicitly; do not rewrite them to
   resemble current specifications.
5. Audit incoming links before renaming or removing duplicate evidence files.
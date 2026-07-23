# Milestone 8: high-detail and advanced prediction

Milestone 8 is planned. It owns the broadest remaining syntax union rather than
mixing that risk with filter integration or API design.

## Scope

- complete the high-detail fixture's luma and chroma traversal;
- implement every reached large or secondary transform and coefficient-range
  path;
- complete CFL and intraBC prediction, displacement, references, and sample
  reconstruction;
- replace sparse transform or CDF admission with fuller normative machinery
  where the reached frequency set makes sparse handling fragile; and
- retain deterministic failure for restoration and unreached profile features.

## Exit gate

The high-detail fixture must match pinned AVM on complete unfiltered Y, U, and
V planes, including every reached CFL and intraBC block. The four-fixture set
must then have complete unfiltered parity under sanitizers and the strict
freestanding dependency gate.

Milestone 8 does not run mandatory frame filters and does not expose a public
decoder.
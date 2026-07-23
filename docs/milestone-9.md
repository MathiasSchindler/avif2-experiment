# Milestone 9: mandatory filters and filtered parity

Milestone 9 is planned. Focused private deblock, CDEF, CCSO, and GDF kernels
already pass sanitizer vectors, but no fixture-level filtered parity is claimed.

## Scope

- populate complete deblock and CCSO metadata from committed leaves;
- run deblock, CDEF, CCSO, and GDF in normative order over coded planes;
- preserve immutable source planes where a later filter classifies against an
  earlier stage;
- compare stage-local hashes and samples against pinned AVM evidence; and
- crop coded planes to display dimensions only at the defined output boundary.

Restoration remains unsupported for the accepted profile and must reject before
final-plane mutation.

## Exit gate

Tiny, odd, ordinary, and high-detail fixtures must match pinned AVM
sample-exactly on final filtered Y, U, and V planes. Stage-local checks must
identify deblock, CDEF, CCSO, or GDF as the owning layer for any mismatch.

Milestone 9 does not expose a public decoder.
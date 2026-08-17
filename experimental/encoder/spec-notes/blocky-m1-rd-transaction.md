# M1 block-local RD transaction boundary

Status: **complete; M1 admitted**.

`--search rd` retains DC first, then directional, Smooth-family, and Paeth
predictor ordering with skip, DC-only, and bounded-AC residual candidates.
Candidate trials must restore range-writer position/range, adaptive CDFs,
coefficient and mode contexts, reconstructed availability, and provisional
reconstruction before the next candidate. The native
`m1-state-transaction-test` exercises those state classes without heap use.

The production path now trials each candidate against the live warm
range-writer and snapshots the writer, adaptive CDFs, secondary-transform CDF,
coefficient/mode/partition contexts, reconstruction availability, and
provisional reconstructed samples. `m1-state-transaction-test` proves repeated
trial rollback and winner-only commit. The complete-stream oracle verifies
local choices on one through four leaves, including a Paeth+AC winner whose
forbidden secondary-transform symbol is omitted.

Color performs one `(0,0)` trial, four U trials, and four V trials. Every
qdelta replay consumes the stored luma modes and coefficients; the oracle
compares the active candidate Y bytes across all nine points, and emitted
telemetry reports `rd_qdelta_trials=9 rd_qdelta_y_preserved=1`.

The complete 27-case gate passes with exact local/AVM/project parity, six
strict full-image Pareto wins, 6,206 fewer aggregate bytes, and 31,503,407
lower aggregate Y SSE. Evidence: `metrics/blocky-m1-gate.json`.

# Basic horizontal/vertical intra model for fixed production leaves

Status: the bounded search policy is integrated in production. The cardinal
subset remains explicit behind `--search directional`; `--search
directional-exact` retains the exhaustive validator path, and default/search-off
remains legacy DC-only. DIP,
smooth, Paeth, angular deltas, edge filtering,
upsampling, nonzero MRLS, IBP, CfL, and MHCCP are outside this first surface.

## Enablement and syntax

The current production sequence signals DIP off and edge filtering, MRLS, CfL,
MHCCP, IBP, and intra IDTX available. This first model keeps those transmitted
sequence fields unchanged. A cardinal luma mode must additionally code
`mrl_index=0`; merely replacing the existing DC `y_mode_index` is not
conforming. Color leaves code `is_cfl=0`. Pure 90/180 cardinal prediction
returns before the enabled edge-filter and IBP machinery, so those tools do
not change these kernels. A later clean profile may disable them, but that is
not assumed by the frozen production-surface syntax.

For every luma leaf the ordered surface is:

1. `y_mode_set=0`;
2. `y_mode_index` in context 0, 1, or 2, equal to the count of directional
   left/above mode neighbors;
3. `fsc_mode=0`; and
4. for V/H, `mrl_index=0` in context 0.

At 8x8 with no reordering, DC/V/H indices are 0/5/6. Larger fixed leaves
reorder directional joint modes using left then above neighbors, followed by
the normative default order. With a DC/V/H-only candidate set, V/H remain
within the first eight symbols, so `y_mode_offset` is never needed.

Color codes `is_cfl=0`, then `uv_mode`. If colocated luma is V/H, UV context 1
places the colocated mode at symbol 0 and DC at symbol 1. With colocated DC,
context 0 maps DC/V/H to symbols 0/5/6. Chroma inherits a zero angle delta
when it matches luma and has no separate MRLS syntax.

## References and kernels

Prediction is per transform in reconstruction order, not once per partition
leaf. Luma 16x16, 8x16, and 16x8 leaves therefore expose references created by
earlier 8x8 transforms in that same leaf. Above reconstructed state persists
across superblock rows. Left reconstructed pixels naturally persist across
adjacent superblocks in a row; the first transform at x=0 has no left edge.

Pure vertical copies the reconstructed top sample for each column into every
row. Pure horizontal copies the reconstructed left sample for each row into
every column. A short available edge repeats its final sample. If the selected
edge is absent, V uses `left[0]` or neutral 127; H uses `above[0]` or neutral
129. These asymmetric neutral values are normative midpoint-minus/plus-one,
not DC's neutral 128. Cardinal 90/180 prediction returns before edge filtering,
interpolation, or IBP.

## Deterministic comparison against DC

The model clones the exact forward range-writer and adaptive CDF state.
Production performs deterministic complete-stream DC/V/H trials and scores:

```text
cost_q8 = total plane SSE * 256 + complete stream bits
```

Directional admission additionally requires no per-plane SSE or byte
regression. Ties retain DC, then V, then H. Complete coefficient-rate and OBU
cost are therefore included.

## Frozen evidence and limits

`tests/directional_intra_vectors.json` locks flat, horizontal/vertical
gradients, quadrant edges, and a hash-identified prepared pitti-ceiling crop
for 8x8, 8x16, 16x8, and 16x16 in I400/I420/I444. It also locks missing/short
edge behavior, exact syntax tell transitions, and a 128x128 two-by-two
superblock availability trace.

Local model tests establish syntax mapping, rate accounting, stable ties, the
requested matrix, and cross-leaf/superblock availability. Test-only drivers
link the pinned AVM library's 8x8/16x16 V/H kernels and the project decoder's
cardinal dispatcher; both reproduce the frozen copy kernels exactly. A
`tests/test_directional_intra_production.py` locks search-off compatibility,
I400/I420/I444 complete-stream parity between pinned AVM and the project
decoder, no SSE/byte regression, and three targeted wins. The default remains
off because the 90-case production policy remains too expensive for an
implicit default.

## Production bounded directional candidate evaluation

`tools/directional_fast_search.py` validates the production two-stage policy.
The prefilter measures source-edge DC/V/H predictor SSE after fitting the
single transform-local constant residual already represented by the production
DC coefficient. Pictures no larger than 64x64 are always shortlisted. Larger
pictures are shortlisted only when the best cardinal score is at most
3686/4096 of DC. This stable threshold prunes 18 of the locked 90 cases before
any directional stream trial.

The independent 140-case model uses transactional range-writer and CDF clones.
Each leaf trial starts from the committed state, measures its exact tell delta,
and is discarded without mutation. A candidate is safely pruned when its
distortion-only lower bound already exceeds the incumbent exact RD cost. This
evaluates 253 of 420 candidates, prunes 167, and agrees with all 140 exhaustive
choices. The winner alone advances the writer and adaptive CDF state.

For production, shortlisted pictures use the validator-exact full-stream
DC/V/H result. Each mode encode rebuilds the writer and CDFs from the same
state, and only the selected mode is encoded again into caller-visible output.
A distortion-times-256 lower bound safely skips exact rate comparison once a
candidate already loses to the incumbent. The result is committed only if
bytes and every plane SSE are no worse than a fresh search-off encode; this is
the stream-level transaction that prevents qdelta or entropy-state changes
from being mistaken for directional gain. Stable comparison order is DC, V, H,
so exact ties fall back to DC.

The frozen `metrics/directional-fast-search.json` covers the locked 90 cases
and the independent 140 cases. It records 90/90 safe-oracle decision agreement,
72 shortlisted cases, 18 early-pruned cases, 12 committed wins, and deterministic
reruns for all 72 shortlisted results. Exhaustive search changed 15 outputs;
the nonregression transaction rejected three and committed 12. The integrated
policy runtime is 262.186 s, below the 600 s gate.
Aggregate bytes improve 278134 to 276771 and aggregate Y/U/V SSE does not
regress. The nine I400 horizontal-edge, vertical-edge, and diagonal-edge cases
form the directional subset: 525 to 456 bytes (13.143%) at identical SSE.
The independent complete decode matrix records 276771 production bytes and
decoded aggregate Y/U/V improvements of 3622/44314/529398, with exact pinned
AVM/project parity, deterministic reruns, and a 7263744-byte peak workspace.

The ordinary default remains search-off because this runtime is acceptable for
an explicit quality mode but too expensive as an implicit default. Explicit
search-off streams retain their locked hashes.

**`encoder-directional-intra-production` status: DONE.**

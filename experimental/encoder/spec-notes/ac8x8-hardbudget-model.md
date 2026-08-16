# Whole-image I400 AC hard-budget model

Status: **blocked** for todo `encoder-ac8x8-hardbudget-model`.

`tools/ac8x8_hardbudget_model.py` is independent of the production encoder
entry point. Its hosted C oracle includes the unchanged scalar AC
implementation in a test translation unit and evaluates complete streams with
the real partition transcript, adaptive coefficient CDF state, range-writer
flush, OBU headers, and downstream DC prediction. It never adds isolated
coefficient bit estimates.

For each locked 64x64 I400 case, the model starts at DC fallback. It samples
16 blocks evenly over the whole image and deterministically raises each
block's searched-position prefix through the current 16-position, 8-nonzero,
level-63 candidate surface. At every greedy step it re-searches all blocks in
decode order and re-encodes the complete stream. The best exact SSE gain per
incremental complete-stream byte is retained, up to 32 steps. Dominated states
are removed, then a multiple-choice knapsack selects one state per case under
the exact 786-byte aggregate budget.

The locked result selects the 18 DC streams: 786 bytes, Y SSE 191,088,000,
zero AC-subset wins, and zero AC-subset gain. Those streams retain the frozen
local == pinned AVM == project parity evidence. The 18 cases evaluate 288
frontier states in 56.96 seconds; the slowest case is 6.85 seconds.

The best reachable 8%-gain frontier has only 7 wins, subset SSE 164,950,576,
and needs 523 subset bytes, or 1,048 aggregate bytes: 262 bytes above the
locked budget. No candidate at any modeled rate improves either qindex-180
line case. Therefore more search allocation cannot meet the 9-win gate.

Exact next requirement: introduce a byte-neutral or byte-saving candidate for
both `horizontal-lines-2-q180` and `vertical-lines-2-q180`, and save at least
262 bytes from the current reachable 7-win frontier (or produce a different
9-win, 8%-gain frontier within 786 bytes). Production core remains unchanged.

Run:

```sh
python3 experimental/encoder/tools/ac8x8_hardbudget_model.py
python3 -m unittest \
  experimental/encoder/tests/test_ac8x8_hardbudget_model.py
```

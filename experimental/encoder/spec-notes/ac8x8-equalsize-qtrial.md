# Equal-size bounded AC qindex trials

Status: **BLOCKED** at the locked targeted AC-subset frontier.

This is an orchestration experiment only. Production codec code and the CLI
contract are unchanged. For each of the 18 targeted and 12 prepared I400
comparisons in the locked 90-case DC-only baseline, the tool:

1. reproduces and decoder-checks the DC-only stream at the requested qindex;
2. tries at most 16 monotonically increasing qindices, from the requested
   value through 255, using the explicit general `--search bounded` path;
3. measures the actual finished OBU byte count;
4. decodes every trial that fits the per-case DC-only byte budget with pinned
   AVM and the project decoder, and ranks it by decoded Y SSE; and
5. retains DC-only unless a fitting AC stream has strictly lower SSE.

The user-visible semantics cannot honestly call the result “qindex Q”: Q is a
quality anchor and byte-budget reference. A selected AC stream may signal a
higher qindex (nominally lower quality), and that emitted qindex must be shown.
Trials never move below the requested qindex, so the policy does not spend
more bytes in pursuit of an unannounced higher-quality setting.

`metrics/ac8x8-equalsize-qtrial.json` is the frozen result. Every selected
stream is at or below its own DC-only whole-stream byte count, deterministic,
and exact between the two decoders. Prepared cases cannot regress because the
strict DC fallback participates in selection.

The admission gate still fails. The checkerboard and two-pixel line cases have
29-byte DC streams. No tested complete AC stream improves their reconstruction
within 29 bytes; the first fitting trials have fallen back to equivalent
DC-only behavior. Thus the nine-case locked AC subset has neither the required
8% aggregate SSE gain nor its nine minimum wins. Qindex trial rate control
cannot solve that syntax-rate floor; a sparser coefficient search, better
prediction, or an aggregate (rather than per-comparison) budget would be a
different experiment.

There is a second blocker: 33 byte-fitting, non-selected AC trials are rejected
by pinned AVM, concentrated in prepared images and diagonal-edge q60/q100.
They are excluded before ranking, and every selected result remains
dual-decoder exact. This parity frontier also prevents treating the current
general AC path as a prepared-corpus rate-control surface.

Run:

```sh
python3 experimental/encoder/tools/ac8x8_equalsize_qtrial.py
python3 -m unittest \
  experimental/encoder/tests/test_ac8x8_equalsize_qtrial.py
```

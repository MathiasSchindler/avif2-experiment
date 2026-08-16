# Explicit luma AC color admission gate

Status: **pass** for todo `encoder-ac8x8-production`.

`tools/ac8x8_color_gate.py` is the deterministic admission scaffold for
explicit production luma 8x8 AC in I400, I420, and I444. It builds 12 focused
cases from four luma classes (odd detail, even texture, an edge, and a
multisuperblock crop from the SHA-256-locked prepared corpus), both neutral and
non-neutral chroma, and qindex 60/100/180 contexts.

The gate requires:

- successful explicit CLI and caller-workspace API reachability in all formats;
- exact local/API, pinned AVM, and project reconstruction;
- deterministic rerun hashes and default == explicit-search-off hashes;
- a strict luma SSE win over search-off for every targeted case;
- unchanged U/V reconstruction when only luma search changes;
- at most 8,192 OBU bytes, 5 seconds per encode, 1 MiB workspace, and 32 MiB
  disposable gate workspace; and
- passing invalid-option, undersized-capacity, result atomicity, and CLI atomic
  output paths.

Default-on equal-rate admission is deliberately not required. Size is still
reported without hiding the cost.

Production result: all 12 cases are reachable and deterministic with exact
local, pinned AVM, and project reconstruction. U/V are byte-identical to the
DC path. Eleven cases improve luma SSE; the prepared multisuperblock I400 case
retains its valid baseline. Bounded streams total 1,686 bytes versus 777
search-off bytes (+909). Maximum workspace is 98,864 bytes, median runtime is
about 22 ms, and worst runtime is about 164 ms. No blockers remain.

Run:

```sh
make -C experimental/encoder \
  build/av2-encoder-experiment build/ac8x8-test \
  build/ac8x8-color-gate-driver
python3 experimental/encoder/tools/ac8x8_color_gate.py
python3 -m unittest experimental/encoder/tests/test_ac8x8_color_gate.py
```

Frozen evidence is `metrics/ac8x8-color-gate.json`; the runner always removes
its disposable workspace.

# AV2 frame U/V quantizer-delta model

Status: isolated syntax, reconstruction, parity, and bounded RD model. No M3
production syntax or search path consumes these deltas.

For the admitted 8-bit color still profile, `base_q_idx` is an unsigned
8-bit value. A sequence header first signals `separate_uv_delta_q`. It then
signals `equal_ac_dc_q`. When equality is false, the sequence carries fixed
5-bit base Y-DC and UV-DC offsets coded as `value - (-23)`, each followed by
its frame-delta-enable flag. Color always carries the 5-bit UV-AC base offset
and its enable flag. Thus every sequence base offset is in `[-23, 8]`.
Equality requires base Y-DC zero, disables both DC frame-delta flags, and
makes the UV-DC base equal the UV-AC base.

An enabled frame delta is one zero bit for value zero. A nonzero value is one
presence bit followed by a 7-bit two's-complement literal, giving the exact
range `[-64, 63]`. If separate UV is disabled, V copies U. If it is enabled,
one `different_uv` bit precedes U; V literals are present only when that bit
is one. With equal AC/DC, each plane's DC frame delta copies its AC delta.
Enabling tied UV deltas changes no sequence-header bit count relative to the
existing color profile: it changes the UV-AC enable bit from zero to one.
Its frame payload costs 1 bit for shared zero, 8 bits for shared nonzero, and
1 plus the U and V code lengths when they differ. Independent DC/AC adds
exactly 12 sequence bits (the Y-DC and UV-DC base/enable fields); its shared
frame cost is the two enabled U code lengths, and distinct V adds one flag
plus both V code lengths. Byte alignment, OBU lengths, and entropy changes are
therefore measured from complete streams rather than estimated from these
raw-bit counts.

For plane P and coefficient class C, the effective 8-bit lookup index is
`clip(base_q_idx + sequence_base_delta[C] + frame_delta[P][C], 1, 255)`.
The sole lossless exception is base index zero with total delta at most zero,
which selects lookup entry 64 directly. The 25-entry lookup is
`64,40,41,43,44,45,47,48,49,51,52,54,55,57,59,60,62,64,66,68,70,72,74,76,78`;
indices at least 25 repeat entries 1..24 with a left shift every 24 indices.
U and V therefore share sequence base offsets but may have distinct frame
offsets and distinct dequant steps.

Coefficient entropy q-context is derived only from unmodified
`base_q_idx`: `0` through 90, `1` through 140, `2` through 190, then `3`.
Plane deltas do not reinitialize or select coefficient CDFs. Decoder-local
changes are confined to the sequence flags/base offsets, five frame delta
members, per-plane DC/AC dequant tables (and encoder reciprocal tables), the
lossless predicate, UV qmatrix sharing when separate UV is off, and U/V
deblock quantizer offsets. Segmentation and block delta-q alter the input
qindex before the same plane offsets are applied.

The deterministic DC-profile candidate set is shared deltas
`{0,-16,-8,8,16}` plus at most one independently best U/V pair from that same
set. DC and AC are tied per plane, avoiding the 12 extra sequence bits needed
to enable independent DC and AC. Candidates are re-quantized, entropy-coded,
and compared by exact complete-stream bytes. A candidate is admitted only
within two bytes of the zero-delta stream; selection minimizes weighted
`U SSE + V SSE`, then bytes, delta magnitude, shared-before-separate, and
signed delta order. Y syntax, levels, reconstruction, and SSE are invariant.

`tests/m3_plane_qdelta_vectors.json` freezes all candidates and selected
streams for I420/I444 at base-q context boundaries 1, 90, 91, 140, 141, 190,
191, and 255 over neutral, color-gradient, and prepared chroma blocks. The
selected streams are byte-exact between local reconstruction, pinned AVM, and
the project decoder. It also freezes a 31-byte independent U/V DC/AC probe
with sequence UV bases `-2/+3` and frame
`U(dc,ac)=(-8,+8), V(dc,ac)=(+16,-16)`.

Selected non-neutral improvements include I420 q90 `4456 -> 4440` SSE for
one byte, q141 `4600 -> 4456` for two bytes with distinct U/V, and q190
`14364 -> 10068` for one byte. I444 q141 improves `30400 -> 30272` for two
bytes and q190 improves `84681 -> 84041` for two bytes. Neutral blocks always
retain zero delta and zero SSE; candidates unable to improve under the
two-byte cap retain the baseline. I400 sequence/frame generation remains
untouched, and the isolated model rejects monochrome format id 1.

# M1 exact syntax: 8×8 constant I400 closed-loop key still

Status: raw syntax and the independent entropy prefix are resolved. This is an
encoder research note derived from the published specification and black-box
decoder behavior, not copied AVM implementation material.

## Evidence and target choice

Normative source: **AV2 Bitstream & Decoding Process Specification v1.0.0**,
28 May 2026, <https://av2.aomedia.org/v1.0.0/index.html>, especially §§4.11.3,
4.11.8, 5.2, 5.4, 5.18, 5.20, 8.2, and 8.3. `prob_inc.h` and
`para_adjustment_list.h` are normative specification attachments. They are not
AVM source.

Black-box evidence used pinned `build/reference/avm/avmenc`/`avmdec` and the
separately implemented `build/host-cmake/av2heif-decoder`. AVM accepts 1×1,
2×2, 4×4, and 8×8 monochrome stills, all in 21 bytes at QP 255. **8×8 is the
smallest practical M1 target**: it is still only one coded luma block, but it
reaches a real 8×8 leaf after the frame-edge-forced 64→32→16 splits. Both
decoders produce exactly 64 bytes of value 128.

The observed reference stream is:

```text
01 08
0a 04 820488bf187e6f1022
07 10 e3fe6c00 8d80
```

The helpers derive the first 19 bytes from named syntax fields. `8d80` is
retained only as independent black-box entropy evidence; it is not claimed to
be the unique encoding of the tile symbols.

## Corrected earlier assumptions

1. AV2 Annex-B OBU framing is an outer canonical little-endian LEB128 length.
   The length covers the complete OBU: one-byte header, optional one-byte
   header extension, and payload. It does **not** include its own LEB128 bytes.
   There is no AV1-style `obu_has_size_field` bit in the AV2 OBU header.
2. The one-byte header is `extension_flag f(1), obu_type f(5),
   obu_tlayer_id f(2)`. Thus `08`, `04`, and `10` are Temporal Delimiter,
   Sequence Header, and Closed Loop Key headers.
3. `uvlc(0)` is `1`; `uvlc(v)` is `floor(log2(v+1))` zeroes followed by
   `v+1` in binary. A 32-zero prefix is forbidden; the maximum conforming value
   is `2^32-2`.
4. For `ns(n)`, `w=FloorLog2(n)+1`, `m=2^w-n`, and the lower `m` values use
   `w-1` bits. For `ns(5)`, values 0..4 are `00,01,10,110,111`.
5. The Sequence Header is an extensible OBU. After its syntax it therefore has
   `obu_extension_flag=0`, then a trailing one and zero fill. Frame/tile-group
   arithmetic data instead consumes its trailing marker through
   `exit_symbol()`; ordinary OBU `trailing_bits()` is not applied again.
6. AV2 v1.0.0 specifies the **decoder** interval process, initialization, and
   legal termination, not a unique forward range-writer byte sequence.
   Different encoder terminations may be conforming. Matching AVM's `8d80`
   byte-for-byte is validator evidence, not a normative encoder requirement.

## Complete Sequence Header call and field order

The top-level order is:

```text
identity/profile/single-picture/level/chroma/depth
dimensions and optional crop
layer/dependency state (all inferred for single picture and layer zero)
sequence_partition_config
sequence_segment_config
sequence_intra_config
sequence_inter_config
sequence_scc_config
sequence_transform_quant_entropy_config
sequence_filter_config
sequence_tile_config
film_grain_params_present
extensible-OBU flag and trailing bits
```

Single-picture inference removes timing/decoder-model, motion-mode,
order-hint, screen-content-selection, average-CDF, and CDEF-on-skip signaling.
Monochrome inference removes SDP and every chroma-only raw field.

Exact transmitted ledger (bit offsets are relative to the nine-byte payload):

| Bit | Width | Field | Encoding/value |
|---:|---:|---|---|
| 0 | 1 | `seq_header_id` | `uvlc(0)=1` |
| 1 | 5 | `seq_profile_idc` | 0 |
| 6 | 1 | `single_picture_header_flag` | 1 |
| 7 | 5 | `seq_level_idx` | 0 |
| 12 | 3 | `chroma_format_idc` | `uvlc(1)=010` (I400) |
| 15 | 3 | `bit_depth_idc` | `uvlc(1)=010` (8-bit) |
| 18,22 | 4 each | width/height bit counts minus 1 | 2,2 |
| 26,29 | 3 each | maximum width/height minus 1 | 7,7 |
| 32 | 1 | cropping present | 0 |
| 33,34 | 1 each | 256/128 superblock | 0,0 (64×64) |
| 35,36 | 1 each | extended/uneven partitions | 1,1 |
| 37 | 1 | reduced PB aspect ratio | 0 |
| 38,39 | 1 each | extended/sequence segmentation | 0,0 |
| 40..45 | 1 each | DIP, edge, MRLS, CFL, MHCCP, IBP | 0,1,1,1,1,1 |
| 46..48 | 1 each | ref-MV bank, disable/constrain DRL reorder | 1,0,0 |
| 49 | 2 | `seq_max_bvp_drl_bits_minus_1` | `ns(3,2)=11` |
| 51,52 | 1 each | frame BVP change, BAWP | 0,1 |
| 53..59 | 1 each | FSC, intra IST, inter IST, reduced TX, TCQ, parity, equal AC/DC | 1,1,1,0,0,0,1 |
| 60..64 | 1 each | cross-tile disable, CDEF, GDF, restoration, CCSO | 0,0,0,0,0 |
| 65 | 2 | `df_par_bits_minus_2` | 1 |
| 67,68 | 1 each | sequence tile info, film grain | 0,0 |
| 69 | 1 | extensible OBU extension flag | 0 |
| 70,71 | 1 each | trailing one, trailing zero | 1,0 |

This produces `820488bf187e6f1022`.

## Complete frame-header order

For a single-picture CLK the inferred state is KEY, intra, immediately output,
no order-hint bits, no frame-size override, all reference slots refreshed, and
no primary reference. The complete operational order remains:

```text
frame_header_info:
  MFH/sequence IDs; inferred frame type/output/reference state
  frame_size
  screen_content_params; intrabc_params
  disable_cdf_update
  tile_info
  quantization_params
  primary-reference/CDF setup
  segmentation_params
  setup_qm_params
  delta_q_params
  lossless derivation and parity/TCQ derivation
  deblocking_filter_params
  gdf_params; cdef_params; lr_params; ccso_params
  read_tx_mode
  frame_reference_mode
  skip_mode_params
  BAWP/warp inference
  reduced_tx_set
  global_motion_params
  film_grain_config
byte_alignment
tile_group_payload
```

Disabled sequence tools and intra-frame conditions cause GDF, CDEF,
restoration, CCSO, reference mode, skip mode, BAWP, warp, global motion, and
film grain to emit no bits. Exact raw ledger:

| Bit | Width | Field | Value |
|---:|---:|---|---:|
| 0,1 | 1 each | `cur_mfh_id`, sequence-header ID (`uvlc`) | 0,0 |
| 2..6 | 1 each | screen tools, integer MV, intrabc, CDF disable, uniform tiles | 1,0,0,0,1 |
| 7 | 8 | `base_q_idx` | 255 |
| 15,16 | 1 each | segmentation, qmatrix | 0,0 |
| 17 | 1 | delta-Q present | 1 |
| 18 | 2 | delta-Q resolution | 2 |
| 20,21 | 1 each | luma horizontal/vertical deblock application | 1,1 |
| 22,23 | 1 each | corresponding deblock delta-Q present | 0,0 |
| 24 | 1 | TX mode select | 0 (largest) |
| 25 | 2 | reduced TX set | 0 |
| 27..31 | 5 | `byte_alignment()` zeroes | 0 |

This produces `e3fe6c00`; the remaining two payload bytes are arithmetic data.

## Tile traversal, skip, and mode syntax

`MiRows=MiCols=2`, while the sequence superblock is 64×64. At `(0,0)`,
`partition_implied_at_boundary` and the allowed-partition checks force:

```text
64×64 SPLIT → 32×32 SPLIT → 16×16 SPLIT → 8×8
```

Out-of-frame split children return before reading symbols. At 8×8,
`PARTITION_NONE` is legal and the observed stream decodes `do_split=0`.
Therefore the only partition CDF symbol is the 8×8 `do_split`; the three larger
splits are not coded.

The leaf then follows `decode_block → mode_info → intra_frame_mode_info`.
`skip_mode` is inferred zero for every intra frame. With `allow_intrabc=0`,
the intra path also fixes `skip_flag=0` before mode parsing; no `skip_mode` or
`skip` symbol is present. Segmentation, GDF, CDEF, CCSO, and chroma syntax are
absent. The remaining entropy decisions select the luma intra mode and
transform/coefficient state. Their exact values are deliberately treated as
CDF-coded state, not raw fields; a raw-bit ledger must not mislabel them.

## Range decoder and termination

For a tile of `sz` bytes (§8.2.2):

```text
numBits = min(sz*8, 15)
buf = f(numBits)
SymbolValue = 0x7fff XOR (buf << (15-numBits))
SymbolRange = 0x8000
SymbolMaxBits = sz*8 - 15
```

For `8d80`, `buf=0x46c0`, `SymbolValue=0x393f`,
`SymbolRange=0x8000`, and `SymbolMaxBits=1`. `read_bool()` and generic
`read_symbol()` are executable in `tools/m1_syntax.py`; callers supply the
normative selected CDF and probability-increment row rather than embedding AVM
tables. The probability reduction constant is `EC_PROB_SHIFT=7`. Thus
`pp=((f >> 7) << 4)+Prob_Inc[N-2][symbol]`; the earlier Python model
incorrectly shifted by 6, doubling the CDF-derived component while leaving the
increment unchanged.

At `exit_symbol()`, conformance requires `SymbolMaxBits >= -14`. The trailing
position is `position - min(15, SymbolMaxBits+15)`; that bit must be one, all
following bits through the byte-aligned padding end must be zero, and unused
positive `SymbolMaxBits` are skipped. This is the tile-group trailing rule and
is distinct from Sequence Header `trailing_bits()` and frame-header
`byte_alignment()`.

## Executable evidence

`tools/m1_syntax.py` implements exact UVLC, ns, canonical LEB128, OBU framing,
the complete transmitted Sequence Header and frame-header raw fields, forced
partition trace, and decoder interval initialization/boolean/symbol/exit
checks. `tests/test_m1_syntax.py`:

- locks UVLC/ns/LEB128 vectors;
- flips every Sequence Header bit and every meaningful frame-header bit;
- checks exact OBU length semantics and field-derived bytes;
- checks range initialization and termination padding; and
- when both validators exist, rebuilds the stream and requires both to decode
  the same 8×8 plane of value 128.

Run:

```sh
python3 -m unittest experimental/encoder/tests/test_m1_syntax.py
```

## Entropy resolution and configuration boundary

There is no remaining raw-bit or range-scaling uncertainty for this M1
profile. The reconstructed 21-byte QP-255 stream intentionally encodes this
ordered tile transcript:

```text
do_split=0
delta_q_abs=1, delta_q_sign_bit=1        (CurrentQIndex 255 -> 251)
y_mode_set=0, y_mode_index=0             (DC_PRED)
fsc_mode=0
has_palette_y=0
all_zero=1                               (8x8 DCT_DCT, eob=0)
```

`has_palette_y=0` is inferred on this path rather than an additional
range-coded operation. CDF updates are enabled (`disable_cdf_update=0`). Each
selected CDF is used only once in this tile, so adaptation cannot affect a
later decision. Literal `delta_q_sign_bit` is equiprobable and is not
CDF-adapted. The independently selected first rows are:

```text
do_split:   TileDoSplitCdf[0][0] = {28084, 3, 0}
delta_q_abs: TileDeltaQCdf       =
  {16594, 23325, 26424, 28225, 29358, 30099, 30613, 56, 0}
```

`8d80` initializes `(SymbolValue, SymbolRange, SymbolMaxBits)` to
`(0x393f, 0x8000, 1)`. With `EC_PROB_SHIFT=7`, the hash-locked v1.0.0 rows
decode `do_split=0`, then `delta_q_abs=1`, then sign bit 1. The former value 4
was solely the Python model's erroneous shift of 6; it was not caused by CDF
adaptation, row selection, or AVM-specific tables.

`tools/m1_entropy.py` also distinguishes the two CDF representations. Official
tables store cumulative values and the decoder forms `f=32768-cdf[symbol]`.
The C range-writer API accepts already-inverted thresholds and scales them
directly. Its increment is `floor(16*(N-symbol-1)/N)`, exactly matching the
published `Prob_Inc` rows. Fixed and adaptive C writer bytes (`2b798240` and
`387980`) round-trip through the independent Python decoder, including final
CDF state and tile termination.

Provenance hashes:

```text
spec index.html  b267c96181f40799ec1c866ca8de3e936804617b71ac19437430d516aa73f1e3
prob_inc.h       d32d08e0e65c78968b87967d5b33e84493e8029dc1f4bc4d50e888e3f77ffeea
para_adjustment  0488e7e0a8d16a89f53ba8cf1ec8e449a9a3e74aa34ba2fa49e287159336ef80
do_split CDF     bb40c072c47561c4742cb96cd6e91cb1ea18ba153862e3e6bd1f88ef19c0b692
delta-Q CDF      cfdf6d6220bc37e8b29839948c4762502c408777fb73caaf1a1c85761bf74aa2
```

The checked-in AVM fixture is a different 19-byte stream:

```text
01 08
0a 04 820488bf385eea0811
05 10 e20000 10
```

Its sequence configuration differs, its frame is lossless (`base_q_idx=0`),
and its one-byte tile is `10`. That configuration has no delta-Q decision.
Only the common initial `do_split=0` is compared by the focused entropy test;
feeding fixture bytes through the reconstructed QP-255 transcript would
consume unrelated symbols. The earlier “AVM fixture delta_q_abs=1” wording
conflated this fixture with the separately reconstructed, decoder-accepted
`8d80` stream.

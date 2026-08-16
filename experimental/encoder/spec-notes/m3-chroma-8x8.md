# M3 bounded chroma DC profile

Status: verified bounded 8-bit I420/I444 DC-only admission profile.

The sequence header derives `seq_profile_idc=0,chroma_format_idc=0` for I420
and `seq_profile_idc=4,chroma_format_idc=2` for I444. Both use 8-bit depth,
64×64 sequence superblocks, separate luma/chroma partition trees, DCT-only
chroma, shared U/V quantization, and the existing filter-free still-picture
configuration. The frame header retains qindex 1..255 and disables qmatrix,
delta-q, and deblock.

I420 active planes are `cw=(width+1)/2`, `ch=(height+1)/2`. The bounded profile admits every width and height from 1 through 1440 for both
formats: 2,073,600 dimension pairs each. All qindices 1..255 are supported.

The 8×8 path codes one shared partition. Luma uses
the existing DC path. Chroma codes DC mode, non-CfL, one U and one V transform,
and one signed DC coefficient per plane. U begins with skip context 6. V uses
context 0 when U is zero and context 6 otherwise. Chroma coefficient signs are
bypass bits; magnitudes 1..5 use the low-frequency UV EOB base CDF, with the
normative high-range remainder syntax above 5.

The hash-locked CDF generator reads:

- `default_is_cfl_cdf.h`
- `default_uv_mode_cfl_not_allowed_cdf.h`
- `default_v_txb_skip_cdf.h`
- `default_eob_pt_16_cdf.h`
- `default_eob_pt_64_cdf.h`
- `default_coeff_base_lf_eob_uv_cdf.h`
- `default_coeff_br_uv_cdf.h`

The I444 inverse DC path uses the existing 8×8 final shift of 11. I420's 4×4
chroma inverse DC uses final shift 10. Each plane searches all encodable signed
levels and minimizes SSE over every active sample. Since this profile has one
transform per plane, prediction is 128 and there are no reconstructed
neighbors. Its stream hashes remain frozen.

For the 16×16 coded luma leaf, the luma transform-node mapping is the raster
2×2 grid `(0,0),(8,0),(0,8),(8,8)`. Under I420 subsampling the chroma coding
node maps to one 8×8 U transform and one 8×8 V transform. Active source
samples are extended to each coded luma transform by replicating the last
active row and column. Reconstruction is produced on coded canvases and only
the active `width×height`, `cw×ch`, `cw×ch` rectangles are committed.

Boundary leaves may have 16×16, 16×8, 8×16, or 8×8 luma-reference geometry.
The separate chroma tree uses `2*ceil(axis/8)` MI cells, not
`4*ceil(axis/16)`. The 8×8 chroma node is forced `PARTITION_NONE` and consumes
no split symbol. I420 rectangular transforms are 4×8/8×4 with EOB-32;
I444 rectangular transforms are 8×16/16×8 with EOB-128. Skip contexts use
the maximum transform axis, while neighbor state and DC prediction use exact
width and height. Rectangular inverse input is scaled by 2896/4096.

Extended probes support nonzero chroma residuals. The zero-U
I420 V skip context uses coding offset 3; nonzero U adds the normative offset
6. Luma and chroma search use reconstructed-neighbor prediction in syntax
traversal order with edge-replicated source padding.

Workspace alignment is 2 for extended geometry (legacy 8×8 remains alignment
1). Layout reports active dimensions/sizes, coded dimensions/sizes, and
transform counts. Storage contains coded reconstruction, staged active crop,
aligned signed levels, and a 256-byte staged stream. No caller-visible output
is modified before all syntax and capacity checks succeed; workspace/stream,
output/output, output/input (including cross-plane), and same-plane aliases
are rejected.

`tests/m3_chroma_vectors.json` freezes the original four 8×8 streams plus
1×1, 9×9, 9×15, and 15×9 I420 streams and twelve general/boundary streams.
The validator matrix checks
local reconstruction against hash-pinned AVM v1.0.0 and the project decoder.
The C geometry matrix crosses 1,2,7,8,9,15,16,17,63,64,65 on both axes.
The multileaf path traverses each 64×64 superblock luma tree
before its chroma tree. Full superblock chroma roots inherit the luma root's
implicit horizontal split, then visit 16×16 luma-reference leaves depth-first.
I420 maps each leaf to one 8×8 U/V transform; I444 maps it to one 16×16 U/V
transform. A partial root is horizontally split only when both remaining
rounded axes exceed 32 pixels; this reconciles 48-pixel and 16/32-pixel tails
at 64-pixel superblock boundaries. Search and syntax use the same leaf order.

U and V maintain separate above/left magnitude and sign state. U skip context
is its neighbor count plus 6. V uses its own neighbors and adds 6 when current
U is nonzero. Adaptive CDFs and above contexts persist for the tile; left
partition, coefficient, and sign state reset at each superblock row.

Parity probes cover flat, generated non-flat, asymmetric U/V, U-to-V coupling,
mixed odd/even axes, 8/16/64/128 boundaries, partial and complete
superblocks, 896×424, 896×448, 896×504, 896×598, 598×896, 896×672, and 896×896, plus qindex boundaries 1, 90, 91,
140, 141, 190, 191, and 255. Local reconstruction is byte-identical to pinned
AVM and the project decoder. The frozen manifest has 20 streams. Probes
include 8×9, 17×16, 24×24, 64×65, 1440×960, 960×1440, and 1440×1440.
Any axis above 1440 is closed; streams through 896 retain frozen level
selection.

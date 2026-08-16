# Typed I400/I420/I444 luma AC8x8 search design

Status: implemented in the production caller-workspace API and CLI.

## Decision and API

Extend the `enc_ac8x8_*` family and reuse the existing `EncM3Image8`,
`EncM3MutableImage8`, and `EncM3PixelFormat` descriptors. Do not add AC
coefficient/state policy to `enc_m3_intra_yuv8_q_workspace`: M3 intra search
selects complete prediction-mode streams, while AC8x8 has a much larger
coefficient workspace, a separate admission result, and a 256-transform work
bound.

The proposed public surface is:

```c
typedef enum {
    ENC_AC8X8_SEARCH_OFF = 0,
    ENC_AC8X8_SEARCH_BOUNDED = 1
} EncAc8x8CoefficientSearch;

typedef struct {
    size_t size, alignment, stream_capacity;
    size_t plane_width[3], plane_height[3], plane_size[3];
    size_t coded_plane_width[3], coded_plane_height[3];
    size_t coded_plane_size[3], transform_count[3];
    size_t coded_reconstruction_offset;
    size_t baseline_reconstruction_offset;
    size_t luma_coefficients_offset;
    size_t chroma_dc_levels_offset;
    size_t entropy_state_offset, entropy_state_size;
    size_t stream_staging_offset, stream_staging_count;
} EncAc8x8YuvWorkspaceLayout;

typedef struct {
    size_t stream_size, baseline_stream_size, candidate_stream_size;
    size_t workspace_size;
    size_t plane_size[3], transform_count[3];
    uint64_t squared_error[3];
    uint64_t baseline_squared_error[3];
    uint64_t candidate_squared_error[3];
    size_t searched_blocks, ac_blocks, candidate_trials;
    unsigned int qindex;
    EncM3PixelFormat format;
    int8_t u_qdelta, v_qdelta;
    uint8_t luma_mode, chroma_mode;
    int admitted;
} EncAc8x8YuvResult;

EncStatus enc_ac8x8_query_yuv8_workspace(
    EncM3PixelFormat format, size_t width, size_t height,
    EncAc8x8YuvWorkspaceLayout *layout);

EncStatus enc_ac8x8_yuv8_q_workspace(
    const EncM3Image8 *input, unsigned int qindex,
    EncAc8x8CoefficientSearch search,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    EncM3MutableImage8 *reconstruction,
    EncAc8x8YuvResult *result);
```

The existing `EncAc8x8Search` numeric values and
`enc_ac8x8_i400_q_workspace` remain ABI/source compatible. The I400 wrapper
must forward through descriptors and produce byte-identical stream,
reconstruction, result admission, 256-block cap, and tie order. The smaller
coefficient enum is for the new typed API; values 2..4 remain CLI/M3
prediction modes, not accepted coefficient-search values.

CLI parsing should use one internal tagged enum/table:

| text | engine |
|---|---|
| `off` | typed AC8x8 with coefficient search off / existing M3 DC |
| `bounded` | typed AC8x8 bounded luma coefficients |
| `directional` | M3 directional |
| `directional-exact` | M3 directional exact |
| `intra` | M3 intra |

## Validation and failure contract

Query accepts formats I400/I420/I444 and axes 1..1440. I400 requires only Y;
color requires all three planes. For each active plane, validate non-null
data, `stride >= plane_width`, and
`capacity >= (height-1)*stride+width` with checked arithmetic.

Reject every overlap among workspace, stream, all active input spans, and all
active reconstruction spans; reject input/input, cross-plane input/output, and
output/output aliases. Zero stream capacity may use a null stream. The result
object must not alias any descriptor, plane, stream, or workspace span. An
invalid result alias leaves all memory unchanged; after a valid distinct
result pointer is established, every later failure zeroes it and leaves
stream/reconstruction bytes unchanged. Geometry or span arithmetic overflow
returns `ENC_OVERFLOW`; undersized caller storage returns `ENC_OUT_OF_SPACE`;
an axis above the production bound returns `ENC_UNSUPPORTED`; an unknown
format, aliases, and malformed descriptors return `ENC_INVALID_ARGUMENT`.

Four stream-capacity slices are reserved: locked DC baseline, candidate tile,
candidate frame payload, and candidate complete stream. Baseline active
reconstruction is staged separately. Candidate reconstruction remains on
coded canvases and is cropped directly to caller planes only after selection.
Thus capacity failure and late syntax failure are atomic.

## Search, UV preservation, and entropy transaction

Traverse in the existing M3 tile/superblock/leaf/transform order. First encode
the exact M3 DC baseline. Freeze its U/V qdelta, mode, signed DC levels,
reconstruction, transform order, and syntax decisions. The candidate changes
only each Y transform from one DC level to 64 DCT_DCT levels. U/V retain one
signed DC level per existing transform, including U-to-V skip coupling.

For exact I400 compatibility, the first 256 Y transforms rank 16 analysis
positions and test `{0, ideal-2..ideal+2}`; later transforms retain the current
one-position/six-choice DC refinement. Limits remain eight nonzeros and
magnitude 63. Ties are cost, SSE, exact range-writer rate, total magnitude,
then raster coefficient vector. No hash table, random seed, time budget, or
thread scheduling affects candidates.

Whole-image admission remains exactly compatible with I400: select the
candidate only when its active Y SSE is strictly lower than the locked DC
baseline. A larger candidate stream is reported, not hidden and not rejected.
U/V SSE and reconstruction are identical by construction. An SSE tie retains
the complete baseline stream and all baseline reconstructions.

Candidate rate must not call the standalone fresh-CDF
`enc_ac8x8_coeff_rate`. Partition/mode syntax is committed once, then every
coefficient trial starts from a checkpoint of the live range writer and live
tile CDF state. A bounded rollback journal saves each touched CDF row before
its first update; after a rejected trial it restores rows and writer scalar
state/position. The range writer only appends committed bytes, so bytes beyond
the restored position need not be cleared. Coefficient above/left contexts are
derived before trials and committed only for the selected level vector.

The journal bound is 128 unique rows per trial. Each entry contains a
32-bit byte offset in `EncSyntaxCdfs`, a 16-bit word count, and up to 18 saved
`uint16_t` words; 128 padded entries occupy 5,632 bytes and the 8,192-byte
section leaves room for bookkeeping. Exceeding either bound is an internal
`ENC_OVERFLOW` before caller-visible commit, never dynamic allocation.

CDFs and above contexts persist for the whole tile. Partition, coefficient,
mode, U, and V left contexts reset at the existing superblock-row boundary.
Y context commits precede the corresponding chroma syntax. U and V keep
separate neighbor/sign state, and V observes the selected U nonzero state.
These transaction boundaries are required for decoder parity; copying or
resetting state per transform is incorrect.

## 896x896 projections

`tools/ac8x8_color_search_design.py` models the integrated layout on LP64.
It includes the measured 58,190-byte `EncSyntaxCdfs`, a range checkpoint,
8,192-byte CDF rollback journal, transform scratch, neighbor state, full Y
coefficient storage, U/V DC storage, and four stream slices.

| format | active/coded bytes | coefficient bytes | workspace | stream cap |
|---|---:|---:|---:|---:|
| I400 | 802,816 / 802,816 | 1,605,632 | 6,493,736 | 803,072 |
| I420 | 1,204,224 / 1,204,224 | 1,618,176 | 7,309,096 | 803,072 |
| I444 | 2,408,448 / 2,408,448 | 1,618,176 | 9,717,544 | 803,072 |

There are 12,544 Y transforms, 256 fully searched transforms, and at most
98,304 six-choice coefficient trials. The current separate I400 AC workspace
is 11,266,561 bytes at this geometry, so integration removes duplicated M3
canvases/workspaces even for I444.

On 2026-08-16, the current optimized encoder measured 896x896 I400 at 1.01 s
off and 3.64 s bounded (3.63 s user); I420 DC at 4.09 s and I444 DC at
8.28 s. Adding the measured I400 AC increment gives implementation projections
of 6.72 s for I420 and 10.91 s for I444. These are sizing projections, not a
performance gate; the eventual implementation must benchmark the transactional
writer and retain the deterministic 98,304-trial ceiling.

Run the prototype checks with:

```sh
python3 experimental/encoder/tools/ac8x8_color_search_design.py --check
python3 -m unittest \
  experimental/encoder/tests/test_ac8x8_color_search_design.py
```

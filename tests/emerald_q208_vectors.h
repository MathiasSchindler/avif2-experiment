#ifndef AV2H_TESTS_EMERALD_Q208_VECTORS_H
#define AV2H_TESTS_EMERALD_Q208_VECTORS_H

#include "decoder/tile.h"

typedef struct {
    uint16_t position;
    int16_t quantized;
    int32_t dequantized;
} EmeraldQ208Coefficient;

typedef struct {
    size_t tell;
    uint32_t tell_fraction;
    uint16_t range;
    uint64_t difference;
    uint16_t count;
} EmeraldQ208EntropyPoint;

typedef struct {
    uint16_t superblock_row;
    uint16_t superblock_column;
    EmeraldQ208EntropyPoint luma_end;
    EmeraldQ208EntropyPoint chroma_end;
} EmeraldQ208Handoff;

typedef struct {
    uint16_t row;
    uint16_t column;
    Av2hBlockSize block_size;
    Av2hFirstLeafIntraMode mode;
    int8_t angle_delta;
    uint16_t tx_width;
    uint16_t tx_height;
    Av2hFirstTxType tx_type;
    uint16_t eob;
    uint8_t all_zero;
    uint16_t coefficient_count;
    uint16_t coefficient_offset;
    uint16_t nonzero_count;
    size_t syntax_start_tell;
    size_t syntax_end_tell;
    uint32_t predictor_djb2;
    uint32_t prefilter_djb2;
    uint64_t predictor_fnv1a64;
    uint64_t prefilter_fnv1a64;
} EmeraldQ208LumaLeaf;

typedef enum {
    EMERALD_Q208_UV_DC = 0,
    EMERALD_Q208_UV_HORIZONTAL = 2,
    EMERALD_Q208_UV_CFL = 13
} EmeraldQ208UvMode;

typedef struct {
    uint16_t tx_width;
    uint16_t tx_height;
    Av2hFirstTxType tx_type;
    uint16_t eob;
    uint8_t all_zero;
    uint16_t coefficient_offset;
    uint16_t nonzero_count;
    size_t syntax_start_tell;
    size_t syntax_end_tell;
    uint32_t prefilter_djb2;
    uint64_t prefilter_fnv1a64;
} EmeraldQ208ChromaPlane;

typedef struct {
    uint16_t row;
    uint16_t column;
    /* Pinned AVM numeric value; decoder integration maps its local enum. */
    uint8_t reference_block_size;
    EmeraldQ208UvMode mode;
    uint8_t cfl_index;
    size_t mode_end_tell;
    EmeraldQ208ChromaPlane u;
    EmeraldQ208ChromaPlane v;
} EmeraldQ208ChromaLeaf;

typedef struct {
    uint16_t width;
    uint16_t height;
    const char *text_sha256;
    uint32_t sample_djb2;
    uint64_t sample_fnv1a64;
} EmeraldQ208PlaneHash;

typedef struct {
    uint16_t changed_samples;
    uint32_t absolute_delta_sum;
    uint8_t maximum_absolute_delta;
} EmeraldQ208PlaneDelta;

static const EmeraldQ208Handoff emerald_q208_handoffs[] = {
    {
        0U, 0U,
        { 135U, 8784471U, 63728U,
          UINT64_C(17769239708914679807), 27U },
        { 185U, 12065917U, 60672U,
          UINT64_C(11668981025706344447), 17U }
    },
    {
        0U, 16U,
        { 308U, 20123609U, 62784U,
          UINT64_C(12054339411492470783), 14U },
        { 337U, 22076426U, 36120U,
          UINT64_C(352759039412666367), 25U }
    },
    {
        0U, 32U,
        { 403U, 26353565U, 60160U,
          UINT64_C(13025463439710053887), 39U },
        { 412U, 26993294U, 35488U,
          UINT64_C(8438612231952531455), 30U }
    },
    {
        16U, 0U,
        { 496U, 32448666U, 60000U,
          UINT64_C(15697440701283303423), 26U },
        { 501U, 32793887U, 49840U,
          UINT64_C(10723181515811848191), 21U }
    },
    {
        16U, 16U,
        { 575U, 37680024U, 33888U,
          UINT64_C(5492978447321923583), 27U },
        { 580U, 37991461U, 40240U,
          UINT64_C(7232597859588112383), 22U }
    },
    {
        16U, 32U,
        { 718U, 47046758U, 35696U,
          UINT64_C(8147574675866648575), 16341U },
        { 731U, 47862749U, 52224U,
          UINT64_C(14123288431433875455), 16328U }
    }
};

/*
 * Sparse entries losslessly expand to the complete coefficient vectors.
 * All-zero leaves have no entries; ignored reference scratch contents are
 * deliberately not treated as decoded coefficients.
 */
static const EmeraldQ208Coefficient emerald_q208_luma_coefficients[] = {
    { 0U, -2, -1984 },
    { 1U, 2, 1984 },
    { 33U, 1, 992 },
    { 0U, -1, -992 },
    { 0U, -1, -1984 },
    { 0U, 2, 3968 },
    { 16U, 1, 1984 },
    { 32U, -1, -1984 },
    { 0U, 4, 7936 },
    { 32U, 1, 1984 },
    { 0U, -1, -992 },
    { 2U, 1, 992 },
    { 3U, 1, 992 },
    { 33U, -1, -992 },
    { 64U, 1, 992 },
    { 65U, 1, 992 },
    { 66U, -1, -992 },
    { 67U, -1, -992 },
    { 96U, 1, 992 },
    { 97U, 1, 992 },
    { 129U, -1, -992 },
    { 0U, -2, -992 },
    { 1U, 1, 496 },
    { 2U, -1, -496 },
    { 1U, 1, 992 },
    { 32U, -1, -992 },
    { 33U, -1, -992 },
    { 64U, 1, 992 },
    { 0U, -1, -1984 },
    { 1U, -1, -1984 },
    { 18U, 1, 1984 },
    { 32U, 1, 1984 },
    { 0U, 2, 1984 },
    { 1U, -1, -992 },
    { 2U, -1, -992 },
    { 32U, -1, -992 },
    { 33U, 1, 992 },
    { 34U, 1, 992 },
    { 64U, -1, -992 },
    { 98U, -1, -992 },
    { 160U, 1, 992 },
    { 0U, -1, -992 },
    { 32U, 1, 992 },
    { 0U, 1, 496 },
    { 1U, -1, -496 },
    { 64U, 1, 496 },
    { 1U, 1, 496 },
    { 3U, -1, -496 },
    { 4U, 1, 496 },
    { 33U, 1, 496 },
    { 34U, -1, -496 },
    { 35U, -1, -496 },
    { 36U, 1, 496 },
    { 64U, -1, -496 },
    { 66U, -1, -496 },
    { 96U, -1, -496 },
    { 128U, 1, 496 },
    { 131U, 1, 496 },
    { 160U, -1, -496 },
    { 192U, -1, -496 },
    { 194U, 1, 496 },
    { 224U, 1, 496 },
    { 257U, 1, 496 },
    { 258U, 1, 496 },
    { 3U, 2, 992 },
    { 5U, -1, -496 },
    { 32U, -2, -992 },
    { 34U, 1, 496 },
    { 35U, 1, 496 },
    { 36U, -1, -496 },
    { 37U, -1, -496 },
    { 65U, 1, 496 },
    { 66U, -1, -496 },
    { 96U, 1, 496 },
    { 132U, 1, 496 },
    { 226U, 1, 496 },
    { 1U, -1, -992 },
    { 34U, 1, 992 },
    { 35U, -1, -992 },
    { 64U, 1, 992 },
    { 65U, 1, 992 },
    { 66U, -1, -992 },
    { 0U, 2, 1984 },
    { 1U, -1, -992 },
    { 32U, -1, -992 },
    { 33U, 1, 992 },
    { 66U, -1, -992 },
    { 96U, -1, -992 },
    { 160U, 1, 992 },
    { 1U, 1, 992 },
    { 32U, -2, -1984 },
    { 33U, -1, -992 },
    { 66U, -1, -992 },
    { 103U, 1, 992 },
    { 129U, 1, 992 },
    { 132U, -1, -992 },
    { 162U, 1, 992 }
};

static const EmeraldQ208LumaLeaf emerald_q208_luma_leaves[] = {
    { 0U, 0U, AV2H_BLOCK_32X32, AV2H_INTRA_DC, 0, 32U, 32U,
      AV2H_TX_DCT_DCT, 5U, 0U, 1024U, 0U, 3U, 7U, 27U,
      0xdc1a9505U, 0x7087a616U, UINT64_C(0xd495a039fba13383),
      UINT64_C(0x9dbb22928fcd18c2) },
    { 0U, 8U, AV2H_BLOCK_32X32, AV2H_INTRA_DC, 0, 32U, 32U,
      AV2H_TX_DCT_DCT, 1U, 0U, 1024U, 3U, 1U, 27U, 32U,
      0x203eff0fU, 0xefea66dfU, UINT64_C(0x3659250de9f6d6ad),
      UINT64_C(0xad79559316284c8d) },
    { 8U, 0U, AV2H_BLOCK_32X8, AV2H_INTRA_DC, 0, 32U, 8U,
      AV2H_TX_DCT_DCT, 1U, 0U, 256U, 4U, 1U, 41U, 46U,
      0x18c07a1aU, 0x86de6ac0U, UINT64_C(0x39826d2a1c063af2),
      UINT64_C(0xa5ef17f25ad6310c) },
    { 10U, 0U, AV2H_BLOCK_16X16, AV2H_INTRA_DC, 0, 16U, 16U,
      AV2H_TX_DCT_DCT, 0U, 1U, 256U, 5U, 0U, 47U, 52U,
      0x255cb505U, 0x255cb505U, UINT64_C(0xb82bcce632df2383),
      UINT64_C(0xb82bcce632df2383) },
    { 10U, 4U, AV2H_BLOCK_16X16, AV2H_INTRA_SMOOTH, 0, 16U, 16U,
      AV2H_TX_ADST_ADST, 4U, 0U, 256U, 5U, 3U, 52U, 72U,
      0x255cb505U, 0x7959e335U, UINT64_C(0xb82bcce632df2383),
      UINT64_C(0x945432b2b385109f) },
    { 14U, 0U, AV2H_BLOCK_32X8, AV2H_INTRA_DC, 0, 32U, 8U,
      AV2H_TX_ADST_DCT, 2U, 0U, 256U, 8U, 2U, 74U, 89U,
      0x255cb505U, 0x82fa6505U, UINT64_C(0xb82bcce632df2383),
      UINT64_C(0x092c7a263d92d983) },
    { 8U, 8U, AV2H_BLOCK_32X32, AV2H_INTRA_SMOOTH, 0, 32U, 32U,
      AV2H_TX_DCT_DCT, 19U, 0U, 1024U, 10U, 11U, 90U, 135U,
      0xc151574aU, 0xbb0ab043U, UINT64_C(0xff58ae8ac8064af4),
      UINT64_C(0x215d88f01a5f29a3) },
    { 0U, 16U, AV2H_BLOCK_64X32, AV2H_INTRA_DC, 0, 64U, 32U,
      AV2H_TX_DCT_DCT, 6U, 0U, 1024U, 21U, 3U, 186U, 202U,
      0xf72d9505U, 0x83a65d05U, UINT64_C(0x18b5bf534a60ab83),
      UINT64_C(0xe6530393f4999583) },
    { 8U, 16U, AV2H_BLOCK_64X16, AV2H_INTRA_DC, 0, 64U, 16U,
      AV2H_TX_DCT_DCT, 5U, 0U, 512U, 24U, 4U, 205U, 224U,
      0x6b75fd8fU, 0xe133f5b9U, UINT64_C(0xdffea026714d52a5),
      UINT64_C(0xf2b8d13de7ae3e67) },
    { 12U, 16U, AV2H_BLOCK_16X16, AV2H_INTRA_D113, 0, 16U, 16U,
      AV2H_TX_DCT_DCT, 0U, 1U, 256U, 28U, 0U, 228U, 239U,
      0x7fab3a56U, 0x7fab3a56U, UINT64_C(0x2cad43d0d0843bde),
      UINT64_C(0x2cad43d0d0843bde) },
    { 12U, 20U, AV2H_BLOCK_16X16, AV2H_INTRA_SMOOTH, 0, 16U, 16U,
      AV2H_TX_ADST_ADST, 9U, 0U, 256U, 28U, 4U, 240U, 265U,
      0xb9919536U, 0x5bc9af2dU, UINT64_C(0xc46df07821c3aa3e),
      UINT64_C(0xff659e319f036167) },
    { 12U, 24U, AV2H_BLOCK_32X16, AV2H_INTRA_HORIZONTAL, 0, 32U, 16U,
      AV2H_TX_DCT_DCT, 18U, 0U, 512U, 32U, 9U, 265U, 308U,
      0xd5cf0b05U, 0xcf9a30fbU, UINT64_C(0x540b849d83b071e3),
      UINT64_C(0x492a93cefccda81f) },
    { 0U, 32U, AV2H_BLOCK_16X64, AV2H_INTRA_D157, 2, 16U, 64U,
      AV2H_TX_DCT_DCT, 4U, 0U, 512U, 41U, 2U, 343U, 366U,
      0x68f29704U, 0xfe18403aU, UINT64_C(0xeaea81b422c18efc),
      UINT64_C(0x42406a82b717292e) },
    { 0U, 36U, AV2H_BLOCK_16X64, AV2H_INTRA_D135, 3, 16U, 64U,
      AV2H_TX_DCT_DCT, 0U, 1U, 512U, 43U, 0U, 366U, 385U,
      0x5ab6d6cfU, 0x5ab6d6cfU, UINT64_C(0xf1e601db31b84669),
      UINT64_C(0xf1e601db31b84669) },
    { 0U, 40U, AV2H_BLOCK_32X64, AV2H_INTRA_DC, 0, 32U, 64U,
      AV2H_TX_DCT_DCT, 4U, 0U, 1024U, 43U, 3U, 386U, 403U,
      0xf9865e12U, 0x02dfd214U, UINT64_C(0x31b61625892ea7aa),
      UINT64_C(0x4d5f3515440a0cfc) },
    { 16U, 0U, AV2H_BLOCK_64X32, AV2H_INTRA_HORIZONTAL, 0, 64U, 32U,
      AV2H_TX_DCT_DCT, 58U, 0U, 1024U, 46U, 18U, 415U, 496U,
      0x55a59505U, 0xe8ce8b65U, UINT64_C(0x8a3a3c6dc2e2ab83),
      UINT64_C(0x38c6f132a11d4afd) },
    { 16U, 16U, AV2H_BLOCK_64X32, AV2H_INTRA_D203, 2, 64U, 32U,
      AV2H_TX_DCT_DCT, 48U, 0U, 1024U, 64U, 12U, 503U, 575U,
      0x42a3dbb7U, 0xce052797U, UINT64_C(0x1e274b1de9f13b0b),
      UINT64_C(0x1a97ede48cb511bd) },
    { 16U, 32U, AV2H_BLOCK_32X16, AV2H_INTRA_D203, 2, 32U, 16U,
      AV2H_TX_DCT_DCT, 14U, 0U, 512U, 76U, 6U, 585U, 617U,
      0xcf6c04c3U, 0x08dbda35U, UINT64_C(0x6bf6677b5bad793d),
      UINT64_C(0xb531694eca0bbdb3) },
    { 16U, 40U, AV2H_BLOCK_32X16, AV2H_INTRA_D135, 0, 32U, 16U,
      AV2H_TX_DCT_DCT, 16U, 0U, 512U, 82U, 7U, 618U, 655U,
      0xcd705c62U, 0x06610d3aU, UINT64_C(0x571a373b939cdf42),
      UINT64_C(0x43aabbf9c9885f8c) },
    { 20U, 32U, AV2H_BLOCK_64X16, AV2H_INTRA_SMOOTH_HORIZONTAL, 0,
      64U, 16U, AV2H_TX_DCT_DCT, 63U, 0U, 512U, 89U, 8U, 657U, 718U,
      0x1a2385fbU, 0x3ce2f851U, UINT64_C(0x77fee41eaf8efad5),
      UINT64_C(0x18cbdd0abf2b25c1) }
};

static const int16_t emerald_q208_representative_qcoeff[1024U] = {
    [0U] = -2, [1U] = 2, [33U] = 1
};

static const int32_t emerald_q208_representative_dqcoeff[1024U] = {
    [0U] = -1984, [1U] = 1984, [33U] = 992
};

static const EmeraldQ208Coefficient emerald_q208_chroma_coefficients[] = {
    { 0U, 2, 1984 },
    { 0U, -1, -992 },
    { 0U, -2, -1984 },
    { 1U, 1, 992 },
    { 32U, 1, 992 },
    { 64U, 1, 992 },
    { 65U, -1, -992 },
    { 0U, 1, 992 },
    { 32U, -1, -992 },
    { 0U, -1, -992 },
    { 0U, -1, -992 },
    { 0U, -1, -992 },
    { 0U, 1, 992 }
};

static const EmeraldQ208ChromaLeaf emerald_q208_chroma_leaves[] = {
    {
        0U, 0U, 11U, EMERALD_Q208_UV_DC, 0U, 137U,
        { 32U, 16U, AV2H_TX_DCT_DCT, 1U, 0U, 0U, 1U, 137U, 147U,
          0x84861505U, UINT64_C(0x1db18b60c050bb83) },
        { 32U, 16U, AV2H_TX_DCT_DCT, 1U, 0U, 1U, 1U, 147U, 151U,
          0xdac2b505U, UINT64_C(0x6782d84a18ff3d83) }
    },
    {
        8U, 0U, 11U, EMERALD_Q208_UV_DC, 0U, 154U,
        { 32U, 16U, AV2H_TX_DCT_DCT, 8U, 0U, 2U, 5U, 154U, 176U,
          0xb7ce6651U, UINT64_C(0x94bf5ee9d88346f9) },
        { 32U, 16U, AV2H_TX_DCT_DCT, 2U, 0U, 7U, 2U, 176U, 185U,
          0x94787305U, UINT64_C(0x9ededb42bf96dda3) }
    },
    {
        0U, 16U, 11U, EMERALD_Q208_UV_DC, 0U, 311U,
        { 32U, 16U, AV2H_TX_DCT_DCT, 1U, 0U, 9U, 1U, 311U, 314U,
          0x07ca7505U, UINT64_C(0xca0662531cec2d83) },
        { 32U, 16U, AV2H_TX_DCT_DCT, 0U, 1U, 10U, 0U, 314U, 315U,
          0xdac2b505U, UINT64_C(0x6782d84a18ff3d83) }
    },
    {
        8U, 16U, 28U, EMERALD_Q208_UV_DC, 0U, 323U,
        { 32U, 4U, AV2H_TX_DCT_DCT, 0U, 1U, 10U, 0U, 323U, 324U,
          0x99cbc65bU, UINT64_C(0x5e6100ec45335215) },
        { 32U, 4U, AV2H_TX_DCT_DCT, 0U, 1U, 10U, 0U, 324U, 325U,
          0xc5077464U, UINT64_C(0xdaa961858356560e) }
    },
    {
        10U, 16U, 8U, EMERALD_Q208_UV_HORIZONTAL, 0U, 330U,
        { 16U, 8U, AV2H_TX_DCT_DCT, 0U, 1U, 10U, 0U, 330U, 330U,
          0x46db0105U, UINT64_C(0x62fdded0878ad203) },
        { 16U, 8U, AV2H_TX_DCT_DCT, 0U, 1U, 10U, 0U, 330U, 330U,
          0xc66d3005U, UINT64_C(0xd8bfad0c0b116d53) }
    },
    {
        10U, 24U, 8U, EMERALD_Q208_UV_DC, 0U, 332U,
        { 16U, 8U, AV2H_TX_DCT_DCT, 0U, 1U, 10U, 0U, 332U, 333U,
          0x8f285307U, UINT64_C(0xbf7a9ff290c6cba9) },
        { 16U, 8U, AV2H_TX_DCT_DCT, 0U, 1U, 10U, 0U, 333U, 333U,
          0xcb010a1cU, UINT64_C(0xe38b194e13d3cb06) }
    },
    {
        14U, 16U, 28U, EMERALD_Q208_UV_DC, 0U, 337U,
        { 32U, 4U, AV2H_TX_DCT_DCT, 0U, 1U, 10U, 0U, 337U, 337U,
          0x8310b64eU, UINT64_C(0xd96198d0401fc716) },
        { 32U, 4U, AV2H_TX_DCT_DCT, 0U, 1U, 10U, 0U, 337U, 337U,
          0x759b15abU, UINT64_C(0x1f9658ffb9576097) }
    },
    {
        0U, 32U, 12U, EMERALD_Q208_UV_CFL, 1U, 409U,
        { 32U, 32U, AV2H_TX_DCT_DCT, 1U, 0U, 10U, 1U, 409U, 412U,
          0x65f90e5dU, UINT64_C(0x54c6383b5aeed3ad) },
        { 32U, 32U, AV2H_TX_DCT_DCT, 0U, 1U, 11U, 0U, 412U, 412U,
          0x159bffe0U, UINT64_C(0xcf4eb1a1fd0b37de) }
    },
    {
        16U, 0U, 11U, EMERALD_Q208_UV_HORIZONTAL, 0U, 497U,
        { 32U, 16U, AV2H_TX_DCT_DCT, 1U, 0U, 11U, 1U, 497U, 500U,
          0x5c79f505U, UINT64_C(0x799d049769264d83) },
        { 32U, 16U, AV2H_TX_DCT_DCT, 0U, 1U, 12U, 0U, 500U, 501U,
          0x0d19f505U, UINT64_C(0xb6eb9b0edd974d83) }
    },
    {
        16U, 16U, 11U, EMERALD_Q208_UV_DC, 0U, 579U,
        { 32U, 16U, AV2H_TX_DCT_DCT, 0U, 1U, 12U, 0U, 579U, 580U,
          0xdc5baba5U, UINT64_C(0xeccdb094c6983239) },
        { 32U, 16U, AV2H_TX_DCT_DCT, 0U, 1U, 12U, 0U, 580U, 580U,
          0xd3061a25U, UINT64_C(0x2a147b51a98dbc2d) }
    },
    {
        16U, 32U, 11U, EMERALD_Q208_UV_DC, 0U, 723U,
        { 32U, 16U, AV2H_TX_DCT_DCT, 0U, 1U, 12U, 0U, 723U, 724U,
          0x3b1b92efU, UINT64_C(0x5c3170ca2ec7b177) },
        { 32U, 16U, AV2H_TX_DCT_DCT, 1U, 0U, 12U, 1U, 724U, 731U,
          0xaeb8d14bU, UINT64_C(0xc85c62b606187fed) }
    }
};

/* SHA-256 covers the canonical evidence text; sample hashes cover raster data. */
static const EmeraldQ208PlaneHash emerald_q208_prefilter_hashes[] = {
    {
        192U, 96U,
        "43d2663e61718494e65bbb2ffcc532d3b8e7fcf61d992dee039cc0238b6ead35",
        0x98948d0bU, UINT64_C(0xae6462fbe548bedb)
    },
    {
        96U, 48U,
        "6a7281648b397be7bac5d388191b794c987a12b101ef7200153c35a4ac20b197",
        0x11bca394U, UINT64_C(0xfc902ef42c44aba8)
    },
    {
        96U, 48U,
        "df02c644d963a3c0f18cf493dc2426efc82d13bb1ad6349c6ffb11376f564d13",
        0xfc9b2b18U, UINT64_C(0xd1c0b4d179f58f4a)
    }
};

static const EmeraldQ208PlaneHash emerald_q208_final_hashes[] = {
    {
        192U, 96U,
        "583292a3570303579c3d372beddd334fe13985abe93af52cf35d58f9ab2eceb3",
        0x091df68dU, UINT64_C(0x2f4e42ba4af114fd)
    },
    {
        96U, 48U,
        "ff6be9432b94cc8260cc07e6de47c054fb4df298d186e8b23ba2b21b306cb94b",
        0xb30a1bc9U, UINT64_C(0x6bd8ce08081d4bdf)
    },
    {
        96U, 48U,
        "ea936f8ef9359bc3796c84bef089fbb12aa75c78d87d4f00a9ea6802d8783597",
        0x40d0686eU, UINT64_C(0x96a4677b5fdbf2f4)
    }
};

static const EmeraldQ208PlaneDelta emerald_q208_deblock_deltas[] = {
    { 4852U, 20760U, 37U },
    { 1120U, 3415U, 14U },
    { 824U, 1718U, 7U }
};

static const char emerald_q208_final_i420_sha256[] =
    "403c2e7245ec484984e60c1eaf8bd1b47487c0ba6b60afc916b9532025fc51ad";

static const EmeraldQ208EntropyPoint emerald_q208_tile_done = {
    731U, 47862749U, 52224U,
    UINT64_C(14123288431433875455), 16328U
};

#define EMERALD_Q208_TILE_HAS_OVERFLOW 0U
#define EMERALD_Q208_TILE_CORRUPTED 0U
#define EMERALD_Q208_TILE_PHASE AV2H_TILE_DECODE_DONE

#endif

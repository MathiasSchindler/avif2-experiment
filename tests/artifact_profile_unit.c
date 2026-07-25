#include "decoder/frame_output.h"
#include "emerald_q208_vectors.h"

#include <stdio.h>

#define STREAM_CAPACITY 65536U
#define MAX_MI_COLUMNS 128U
#define MAX_MI_ROWS 96U
#define MAX_GDF_BLOCKS 128U
#define MAX_CDEF_UNITS 128U
#define EMERALD_I420_SIZE 27648U
#define EMERALD_FILTER_MEMORY_SIZE 80000U

static int failures;
static unsigned char stream[STREAM_CAPACITY];
static uint8_t luma_pixels[512U * 341U];
static uint8_t luma_reconstructed[MAX_MI_COLUMNS * MAX_MI_ROWS];
static uint8_t emerald_u_pixels[96U * 48U];
static uint8_t emerald_v_pixels[96U * 48U];
static uint8_t emerald_u_predictor[96U * 48U];
static uint8_t emerald_v_predictor[96U * 48U];
static uint8_t emerald_u_reconstructed[(96U / 4U) * (48U / 4U)];
static uint8_t emerald_v_reconstructed[(96U / 4U) * (48U / 4U)];
static uint8_t emerald_u_predictor_reconstructed[(96U / 4U) * (48U / 4U)];
static uint8_t emerald_v_predictor_reconstructed[(96U / 4U) * (48U / 4U)];
static _Alignas(Av2hDeblockCell)
    uint8_t emerald_filter_memory[EMERALD_FILTER_MEMORY_SIZE];
static uint8_t emerald_output[EMERALD_I420_SIZE];
static uint8_t emerald_reference[EMERALD_I420_SIZE];

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

#define REQUIRE(condition)                                                     \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: required check failed: %s\n",           \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
            return;                                                            \
        }                                                                      \
    } while (0)

static void check_emerald_q208_coefficient_slice(
    const EmeraldQ208Coefficient *coefficients,
    size_t coefficient_total,
    size_t offset,
    size_t count,
    size_t vector_size) {
    size_t index;
    CHECK(offset + count <= coefficient_total);
    if (offset + count > coefficient_total) return;
    for (index = 0U; index < count; ++index) {
        size_t other;
        const EmeraldQ208Coefficient *coefficient =
            &coefficients[offset + index];
        CHECK(coefficient->position < vector_size);
        CHECK(coefficient->quantized != 0);
        CHECK(coefficient->dequantized != 0);
        for (other = index + 1U; other < count; ++other) {
            CHECK(coefficient->position !=
                  coefficients[offset + other].position);
        }
    }
}

static int emerald_q208_sha256_is_hex(const char *digest) {
    size_t index;
    for (index = 0U; index < 64U; ++index) {
        char value = digest[index];
        if (!((value >= '0' && value <= '9') ||
              (value >= 'a' && value <= 'f'))) {
            return 0;
        }
    }
    return digest[64U] == '\0';
}

static void check_emerald_q208_vector_fixture(void) {
    static const uint8_t luma_handoff_indices[] = {
        6U, 11U, 14U, 15U, 16U, 19U
    };
    static const uint8_t chroma_handoff_indices[] = {
        1U, 6U, 7U, 8U, 9U, 10U
    };
    size_t coefficient_offset = 0U;
    size_t active_count = 0U;
    size_t all_zero_count = 0U;
    size_t index;

    CHECK(sizeof(emerald_q208_handoffs) /
          sizeof(emerald_q208_handoffs[0]) == 6U);
    CHECK(sizeof(emerald_q208_luma_leaves) /
          sizeof(emerald_q208_luma_leaves[0]) == 20U);
    CHECK(sizeof(emerald_q208_luma_coefficients) /
          sizeof(emerald_q208_luma_coefficients[0]) == 97U);
    for (index = 0U;
         index < sizeof(emerald_q208_luma_leaves) /
                     sizeof(emerald_q208_luma_leaves[0]);
         ++index) {
        const EmeraldQ208LumaLeaf *leaf = &emerald_q208_luma_leaves[index];
        size_t capped_width = leaf->tx_width > 32U ? 32U : leaf->tx_width;
        size_t capped_height =
            leaf->tx_height > 32U ? 32U : leaf->tx_height;
        CHECK(leaf->coefficient_offset == coefficient_offset);
        CHECK(leaf->coefficient_count == capped_width * capped_height);
        CHECK(leaf->syntax_start_tell <= leaf->syntax_end_tell);
        CHECK(leaf->predictor_djb2 != 0U);
        CHECK(leaf->prefilter_djb2 != 0U);
        CHECK(leaf->predictor_fnv1a64 != 0U);
        CHECK(leaf->prefilter_fnv1a64 != 0U);
        if (leaf->all_zero) {
            ++all_zero_count;
            CHECK(leaf->eob == 0U);
            CHECK(leaf->nonzero_count == 0U);
        } else {
            ++active_count;
            CHECK(leaf->eob != 0U);
            CHECK(leaf->nonzero_count != 0U);
        }
        check_emerald_q208_coefficient_slice(
            emerald_q208_luma_coefficients,
            sizeof(emerald_q208_luma_coefficients) /
                sizeof(emerald_q208_luma_coefficients[0]),
            leaf->coefficient_offset, leaf->nonzero_count,
            leaf->coefficient_count);
        coefficient_offset += leaf->nonzero_count;
    }
    CHECK(coefficient_offset ==
          sizeof(emerald_q208_luma_coefficients) /
              sizeof(emerald_q208_luma_coefficients[0]));
    CHECK(active_count == 17U);
    CHECK(all_zero_count == 3U);

    for (index = 0U; index < 1024U; ++index) {
        int16_t quantized = 0;
        int32_t dequantized = 0;
        size_t coefficient_index;
        for (coefficient_index = 0U; coefficient_index < 3U;
             ++coefficient_index) {
            const EmeraldQ208Coefficient *coefficient =
                &emerald_q208_luma_coefficients[coefficient_index];
            if (coefficient->position == index) {
                quantized = coefficient->quantized;
                dequantized = coefficient->dequantized;
                break;
            }
        }
        CHECK(emerald_q208_representative_qcoeff[index] == quantized);
        CHECK(emerald_q208_representative_dqcoeff[index] == dequantized);
    }

    coefficient_offset = 0U;
    active_count = 0U;
    all_zero_count = 0U;
    CHECK(sizeof(emerald_q208_chroma_leaves) /
          sizeof(emerald_q208_chroma_leaves[0]) == 11U);
    CHECK(sizeof(emerald_q208_chroma_coefficients) /
          sizeof(emerald_q208_chroma_coefficients[0]) == 13U);
    for (index = 0U;
         index < sizeof(emerald_q208_chroma_leaves) /
                     sizeof(emerald_q208_chroma_leaves[0]);
         ++index) {
        const EmeraldQ208ChromaLeaf *leaf =
            &emerald_q208_chroma_leaves[index];
        const EmeraldQ208ChromaPlane *planes[2U] = {
            &leaf->u, &leaf->v
        };
        size_t plane;
        CHECK(leaf->mode == EMERALD_Q208_UV_DC ||
              leaf->mode == EMERALD_Q208_UV_HORIZONTAL ||
              leaf->mode == EMERALD_Q208_UV_CFL);
        CHECK((leaf->mode == EMERALD_Q208_UV_CFL) ==
              (leaf->cfl_index == 1U));
        for (plane = 0U; plane < 2U; ++plane) {
            const EmeraldQ208ChromaPlane *expected = planes[plane];
            size_t vector_size =
                (size_t)expected->tx_width * expected->tx_height;
            CHECK(expected->tx_type == AV2H_TX_DCT_DCT);
            CHECK(expected->coefficient_offset == coefficient_offset);
            CHECK(expected->syntax_start_tell <=
                  expected->syntax_end_tell);
            CHECK(expected->prefilter_djb2 != 0U);
            CHECK(expected->prefilter_fnv1a64 != 0U);
            if (expected->all_zero) {
                ++all_zero_count;
                CHECK(expected->eob == 0U);
                CHECK(expected->nonzero_count == 0U);
            } else {
                ++active_count;
                CHECK(expected->eob != 0U);
                CHECK(expected->nonzero_count != 0U);
            }
            check_emerald_q208_coefficient_slice(
                emerald_q208_chroma_coefficients,
                sizeof(emerald_q208_chroma_coefficients) /
                    sizeof(emerald_q208_chroma_coefficients[0]),
                expected->coefficient_offset, expected->nonzero_count,
                vector_size);
            coefficient_offset += expected->nonzero_count;
        }
    }
    CHECK(coefficient_offset ==
          sizeof(emerald_q208_chroma_coefficients) /
              sizeof(emerald_q208_chroma_coefficients[0]));
    CHECK(active_count == 8U);
    CHECK(all_zero_count == 14U);

    for (index = 0U; index < 6U; ++index) {
        const EmeraldQ208Handoff *handoff = &emerald_q208_handoffs[index];
        const EmeraldQ208LumaLeaf *luma =
            &emerald_q208_luma_leaves[luma_handoff_indices[index]];
        const EmeraldQ208ChromaLeaf *chroma =
            &emerald_q208_chroma_leaves[chroma_handoff_indices[index]];
        size_t chroma_end = chroma->u.syntax_end_tell >
                            chroma->v.syntax_end_tell ?
                            chroma->u.syntax_end_tell :
                            chroma->v.syntax_end_tell;
        CHECK(luma->syntax_end_tell == handoff->luma_end.tell);
        CHECK(chroma_end == handoff->chroma_end.tell);
        CHECK(handoff->luma_end.tell < handoff->chroma_end.tell);
    }

    CHECK(sizeof(emerald_q208_prefilter_hashes) /
          sizeof(emerald_q208_prefilter_hashes[0]) == 3U);
    for (index = 0U;
         index < sizeof(emerald_q208_prefilter_hashes) /
                     sizeof(emerald_q208_prefilter_hashes[0]);
         ++index) {
        const EmeraldQ208PlaneHash *hash =
            &emerald_q208_prefilter_hashes[index];
        CHECK(hash->width != 0U);
        CHECK(hash->height != 0U);
        CHECK(emerald_q208_sha256_is_hex(hash->text_sha256));
        CHECK(hash->sample_djb2 != 0U);
        CHECK(hash->sample_fnv1a64 != 0U);
    }
    CHECK(sizeof(emerald_q208_final_hashes) /
          sizeof(emerald_q208_final_hashes[0]) == 3U);
    CHECK(sizeof(emerald_q208_deblock_deltas) /
          sizeof(emerald_q208_deblock_deltas[0]) == 3U);
    for (index = 0U;
         index < sizeof(emerald_q208_final_hashes) /
                     sizeof(emerald_q208_final_hashes[0]);
         ++index) {
        const EmeraldQ208PlaneHash *hash =
            &emerald_q208_final_hashes[index];
        CHECK(emerald_q208_sha256_is_hex(hash->text_sha256));
        CHECK(hash->sample_djb2 != 0U);
        CHECK(hash->sample_fnv1a64 != 0U);
        CHECK(emerald_q208_deblock_deltas[index].changed_samples != 0U);
        CHECK(emerald_q208_deblock_deltas[index].absolute_delta_sum != 0U);
        CHECK(emerald_q208_deblock_deltas[index].maximum_absolute_delta != 0U);
    }
    CHECK(emerald_q208_sha256_is_hex(emerald_q208_final_i420_sha256));

    CHECK(emerald_q208_tile_done.tell == 731U);
    CHECK(emerald_q208_tile_done.tell_fraction == 47862749U);
    CHECK(emerald_q208_tile_done.range == 52224U);
    CHECK(emerald_q208_tile_done.difference ==
          UINT64_C(14123288431433875455));
    CHECK(emerald_q208_tile_done.count == 16328U);
    CHECK(EMERALD_Q208_TILE_HAS_OVERFLOW == 0U);
    CHECK(EMERALD_Q208_TILE_CORRUPTED == 0U);
    CHECK(EMERALD_Q208_TILE_PHASE == AV2H_TILE_DECODE_DONE);
}

typedef struct {
    uint16_t position;
    int32_t quantized;
    int32_t dequantized;
} ArtifactCoefficient;

typedef struct {
    size_t mode_before;
    size_t mode_after_set;
    size_t mode_after_index;
    size_t mode_after_fsc;
    size_t mode_after_mrl;
    Av2hFirstTxSize tx_size;
    uint16_t eob;
    size_t prefix_after_zero;
    size_t prefix_after_point;
    size_t prefix_after_eob;
    Av2hFirstTxType primary_type;
    uint8_t secondary_symbol;
    size_t transform_after;
    uint16_t coefficient_count;
    uint16_t nonzero_count;
    int32_t dc_value;
    size_t levels_before;
    size_t levels_after;
    size_t coefficients_after;
    const ArtifactCoefficient *coefficients;
    size_t coefficients_count;
} ArtifactDecodedExpectation;

typedef struct {
    const char *name;
    uint16_t width;
    uint16_t height;
    uint16_t base_q_index;
    uint8_t allow_intrabc;
    uint8_t enable_gdf;
    uint8_t enable_cdef;
    uint8_t enable_ccso;
    size_t header_bits;
    uint8_t gdf_mode;
    uint8_t gdf_qp_index;
    uint8_t gdf_scale_index;
    Av2hBlockSize first_block;
    size_t first_tell;
    Av2hStatus first_status;
    const ArtifactDecodedExpectation *decoded;
} ArtifactExpectation;

static uint64_t block_pixel_hash(const uint8_t *pixels, size_t stride,
                                 uint16_t width, uint16_t height) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t row;
    size_t column;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            hash ^= pixels[row * stride + column];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static uint32_t trace_sample_hash(const uint8_t *pixels, size_t stride,
                                  uint16_t width, uint16_t height) {
    uint32_t hash = 5381U;
    size_t row;
    size_t column;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            hash = (hash * 33U) ^ pixels[row * stride + column];
        }
    }
    return hash;
}

static void check_decoded_block(
    const ArtifactDecodedExpectation *expected,
    const Av2hTileLumaLeaf *leaf,
    const Av2hFirstCoeffBlock *block) {
    size_t index;
    CHECK(leaf->mode.before_mode_bits == expected->mode_before);
    CHECK(leaf->mode.after_mode_set_bits == expected->mode_after_set);
    CHECK(leaf->mode.after_mode_index_bits == expected->mode_after_index);
    CHECK(leaf->mode.after_fsc_bits == expected->mode_after_fsc);
    CHECK(leaf->mode.after_mrl_bits == expected->mode_after_mrl);
    CHECK(leaf->prefix.tx_size == expected->tx_size);
    CHECK(leaf->prefix.eob == expected->eob);
    CHECK(leaf->prefix.after_all_zero_bits == expected->prefix_after_zero);
    CHECK(leaf->prefix.after_eob_point_bits == expected->prefix_after_point);
    CHECK(leaf->prefix.after_eob_bits == expected->prefix_after_eob);
    CHECK(leaf->transform.primary_type == expected->primary_type);
    CHECK(leaf->transform.secondary_symbol == expected->secondary_symbol);
    CHECK(leaf->transform.after_secondary_bits == expected->transform_after);
    CHECK(block->coefficient_count == expected->coefficient_count);
    CHECK(block->nonzero_count == expected->nonzero_count);
    CHECK(block->dc_value == expected->dc_value);
    CHECK(block->before_levels_bits == expected->levels_before);
    CHECK(block->after_levels_bits == expected->levels_after);
    CHECK(block->after_coefficients_bits == expected->coefficients_after);
    for (index = 0U; index < block->coefficient_count; ++index) {
        int32_t quantized = 0;
        int32_t dequantized = 0;
        size_t expected_index;
        for (expected_index = 0U;
             expected_index < expected->coefficients_count;
             ++expected_index) {
            if (expected->coefficients[expected_index].position == index) {
                quantized =
                    expected->coefficients[expected_index].quantized;
                dequantized =
                    expected->coefficients[expected_index].dequantized;
                break;
            }
        }
        CHECK(block->quantized[index] == quantized);
        CHECK(block->dequantized[index] == dequantized);
    }
}

static void check_advanced_frontier(
    const ArtifactExpectation *expected,
    const Av2hTileLumaLeaf *leaf) {
    if (expected->gdf_mode == 2U && expected->base_q_index != 176U) {
        CHECK(leaf->mode.before_mode_bits == leaf->mode.after_ccso_bits);
        CHECK(leaf->mode.fsc_mode == 0U);
        switch (expected->base_q_index) {
            case 48U:
                CHECK(leaf->mode.before_mode_bits == 12U);
                CHECK(leaf->mode.after_mode_set_bits == 13U);
                CHECK(leaf->mode.after_mode_index_bits == 14U);
                CHECK(leaf->mode.mode == AV2H_INTRA_DC);
                CHECK(leaf->prefix.before_all_zero_bits == 14U);
                CHECK(leaf->prefix.after_all_zero_bits == 16U);
                CHECK(leaf->prefix.after_eob_point_bits == 20U);
                CHECK(leaf->prefix.after_eob_bits == 25U);
                CHECK(leaf->prefix.eob_point == 7U);
                CHECK(leaf->prefix.eob == 45U);
                break;
            case 64U:
                CHECK(leaf->mode.before_mode_bits == 24U);
                CHECK(leaf->mode.after_mode_index_bits == 25U);
                CHECK(leaf->mode.mode == AV2H_INTRA_DC);
                CHECK(leaf->prefix.before_all_zero_bits == 0U);
                break;
            case 96U:
                CHECK(leaf->mode.before_mode_bits == 13U);
                CHECK(leaf->mode.after_mode_index_bits == 17U);
                CHECK(leaf->mode.mode == AV2H_INTRA_D45);
                CHECK(leaf->prefix.before_all_zero_bits == 17U);
                CHECK(leaf->prefix.after_all_zero_bits == 18U);
                CHECK(leaf->prefix.eob_point == 8U);
                CHECK(leaf->prefix.eob_offset_bits == 6U);
                CHECK(leaf->prefix.eob_extra == 61U);
                CHECK(leaf->prefix.after_eob_point_bits == 22U);
                CHECK(leaf->prefix.after_eob_extra_bit == 23U);
                CHECK(leaf->prefix.after_eob_bits == 28U);
                CHECK(leaf->prefix.eob == 126U);
                CHECK(leaf->transform.primary_type == AV2H_TX_DCT_DCT);
                CHECK(leaf->transform.primary_signaled == 0U);
                CHECK(leaf->transform.before_primary_bits == 28U);
                CHECK(leaf->transform.after_primary_bits == 28U);
                CHECK(leaf->transform.secondary_signaled == 0U);
                CHECK(leaf->transform.after_secondary_bits == 28U);
                break;
            case 112U:
                CHECK(leaf->mode.before_mode_bits == 20U);
                CHECK(leaf->mode.after_mode_index_bits == 21U);
                CHECK(leaf->mode.mode == AV2H_INTRA_DC);
                CHECK(leaf->prefix.before_all_zero_bits == 21U);
                CHECK(leaf->prefix.after_eob_bits == 27U);
                CHECK(leaf->prefix.eob == 2U);
                CHECK(leaf->transform.after_secondary_bits == 32U);
                CHECK(leaf->transform.primary_type == AV2H_TX_DCT_DCT);
                break;
            case 128U:
                CHECK(leaf->mode.before_mode_bits == 9U);
                CHECK(leaf->mode.after_mode_set_bits == 13U);
                CHECK(leaf->mode.after_mode_index_bits == 17U);
                CHECK(leaf->mode.mode == AV2H_INTRA_VERTICAL);
                CHECK(leaf->mode.angle_delta == 2);
                CHECK(leaf->mode.fsc_block_context == 6U);
                CHECK(leaf->mode.after_fsc_bits == 17U);
                break;
            default:
                break;
        }
    } else if (expected->base_q_index == 176U &&
               expected->first_block == AV2H_BLOCK_32X64) {
        CHECK(leaf->mode.before_mode_bits == 10U);
        CHECK(leaf->mode.after_mode_index_bits == 11U);
        CHECK(leaf->mode.after_fsc_bits == 11U);
        CHECK(leaf->mode.fsc_block_context == 6U);
        CHECK(leaf->mode.fsc_mode == 0U);
    } else if (expected->base_q_index == 192U &&
               expected->first_block == AV2H_BLOCK_64X64) {
        CHECK(leaf->mode.before_mode_bits == 3U);
        CHECK(leaf->mode.after_mode_index_bits == 5U);
        CHECK(leaf->mode.after_fsc_bits == 5U);
        CHECK(leaf->mode.fsc_block_context == 6U);
        CHECK(leaf->mode.fsc_mode == 0U);
    } else if (expected->base_q_index == 208U &&
               expected->first_block == AV2H_BLOCK_64X16) {
        CHECK(leaf->mode.before_mode_bits == 8U);
        CHECK(leaf->mode.after_mode_index_bits == 9U);
        CHECK(leaf->mode.after_fsc_bits == 9U);
        CHECK(leaf->mode.fsc_block_context == 6U);
        CHECK(leaf->mode.fsc_mode == 0U);
    } else if (expected->base_q_index == 128U &&
               expected->first_block == AV2H_BLOCK_8X16) {
        CHECK(leaf->mode.after_cdef_bits == 15U);
        CHECK(leaf->mode.cdef_strength_index == 2U);
        CHECK(leaf->mode.after_ccso_bits == 20U);
        CHECK(leaf->mode.ccso_present_mask == 7U);
        CHECK(leaf->mode.before_mode_bits == 20U);
        CHECK(leaf->mode.after_mode_index_bits == 21U);
        CHECK(leaf->prefix.after_eob_bits == 27U);
        CHECK(leaf->prefix.eob == 6U);
        CHECK(leaf->transform.after_primary_bits == 28U);
        CHECK(leaf->transform.after_secondary_bits == 32U);
    } else if (expected->base_q_index == 112U &&
               expected->first_block == AV2H_BLOCK_32X64) {
        CHECK(leaf->mode.after_cdef_bits == 6U);
        CHECK(leaf->mode.cdef_strength_index == 1U);
        CHECK(leaf->mode.after_ccso_bits == 8U);
        CHECK(leaf->mode.ccso_present_mask == 2U);
        CHECK(leaf->mode.ccso_block_control[1] == 1U);
        CHECK(leaf->mode.after_mode_index_bits == 9U);
        CHECK(leaf->mode.fsc_block_context == 6U);
    } else if (expected->base_q_index == 224U &&
               expected->width == 333U) {
        CHECK(leaf->mode.after_ccso_bits == 5U);
        CHECK(leaf->mode.ccso_present_mask == 5U);
        CHECK(leaf->mode.ccso_block_control[0] == 1U);
        CHECK(leaf->mode.ccso_block_control[2] == 1U);
        CHECK(leaf->mode.after_mode_index_bits == 6U);
        CHECK(leaf->mode.before_palette_bits == 6U);
        CHECK(leaf->mode.after_palette_bits == 7U);
        CHECK(leaf->mode.palette_mode == 0U);
    } else if (expected->base_q_index == 80U &&
               expected->width == 431U) {
        CHECK(leaf->mode.after_cdef_bits == 4U);
        CHECK(leaf->mode.cdef_strength_index == 2U);
        CHECK(leaf->mode.after_ccso_bits == 7U);
        CHECK(leaf->mode.ccso_present_mask == 6U);
        CHECK(leaf->mode.ccso_block_control[1] == 1U);
        CHECK(leaf->mode.ccso_block_control[2] == 1U);
        CHECK(leaf->mode.after_mode_index_bits == 9U);
    } else if (expected->base_q_index == 144U) {
        CHECK(leaf->mode.after_cdef_bits == 10U);
        CHECK(leaf->mode.cdef_strength_index == 0U);
        CHECK(leaf->mode.after_ccso_bits == 15U);
        CHECK(leaf->mode.ccso_present_mask == 7U);
        CHECK(leaf->mode.after_mode_index_bits == 16U);
    } else if (expected->base_q_index == 80U &&
               expected->width == 384U) {
        CHECK(leaf->mode.after_cdef_bits == 3U);
        CHECK(leaf->mode.cdef_strength_index == 1U);
        CHECK(leaf->mode.after_ccso_bits == 5U);
        CHECK(leaf->mode.ccso_present_mask == 2U);
        CHECK(leaf->mode.after_mode_index_bits == 6U);
        CHECK(leaf->mode.before_palette_bits == 6U);
        CHECK(leaf->mode.after_palette_bits == 6U);
        CHECK(leaf->mode.palette_mode == 0U);
    } else if (expected->base_q_index == 176U &&
               expected->first_block == AV2H_BLOCK_64X64) {
        CHECK(leaf->mode.after_ccso_bits == 8U);
        CHECK(leaf->mode.after_mode_index_bits == 9U);
        CHECK(leaf->mode.before_palette_bits == 9U);
        CHECK(leaf->mode.after_palette_bits == 9U);
        CHECK(leaf->mode.palette_mode == 0U);
    }
    if (expected->first_block == AV2H_BLOCK_32X64) {
        CHECK(leaf->prefix.tx_size == AV2H_TX_32X64);
        CHECK(leaf->prefix.tx_size_context == 4U);
        if (expected->base_q_index == 112U) {
            CHECK(leaf->prefix.before_all_zero_bits == 9U);
            CHECK(leaf->prefix.after_all_zero_bits == 10U);
            CHECK(leaf->prefix.after_eob_point_bits == 11U);
            CHECK(leaf->prefix.after_eob_bits == 15U);
            CHECK(leaf->prefix.eob == 22U);
            CHECK(leaf->transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(leaf->transform.before_primary_bits == 15U);
            CHECK(leaf->transform.after_primary_bits == 15U);
            CHECK(leaf->transform.secondary_symbol == 0U);
            CHECK(leaf->transform.after_secondary_bits == 19U);
        } else {
            CHECK(leaf->prefix.before_all_zero_bits == 11U);
            CHECK(leaf->prefix.after_all_zero_bits == 11U);
            CHECK(leaf->prefix.after_eob_point_bits == 14U);
            CHECK(leaf->prefix.after_eob_bits == 16U);
            CHECK(leaf->prefix.eob == 8U);
            CHECK(leaf->transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(leaf->transform.before_primary_bits == 16U);
            CHECK(leaf->transform.after_primary_bits == 16U);
            CHECK(leaf->transform.secondary_symbol == 0U);
            CHECK(leaf->transform.after_secondary_bits == 20U);
        }
    } else if (expected->first_block == AV2H_BLOCK_64X16) {
        CHECK(leaf->prefix.tx_size == AV2H_TX_64X16);
        CHECK(leaf->prefix.tx_size_context == 3U);
        if (expected->base_q_index == 144U) {
            CHECK(leaf->prefix.before_all_zero_bits == 16U);
            CHECK(leaf->prefix.after_all_zero_bits == 16U);
            CHECK(leaf->prefix.after_eob_point_bits == 18U);
            CHECK(leaf->prefix.after_eob_bits == 21U);
            CHECK(leaf->prefix.eob == 10U);
            CHECK(leaf->transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(leaf->transform.before_primary_bits == 21U);
            CHECK(leaf->transform.after_primary_bits == 21U);
            CHECK(leaf->transform.primary_signaled == 1U);
            CHECK(leaf->transform.primary_long_side_symbol == 1U);
            CHECK(leaf->transform.primary_symbol == 0U);
            CHECK(leaf->transform.secondary_symbol == 0U);
            CHECK(leaf->transform.after_secondary_bits == 26U);
        } else {
            CHECK(leaf->prefix.before_all_zero_bits == 9U);
            CHECK(leaf->prefix.after_all_zero_bits == 17U);
            CHECK(leaf->prefix.after_eob_point_bits == 17U);
            CHECK(leaf->prefix.after_eob_bits == 17U);
            CHECK(leaf->prefix.all_zero == 1U);
            CHECK(leaf->prefix.eob == 0U);
        }
    } else if (expected->first_block == AV2H_BLOCK_64X64) {
        CHECK(leaf->prefix.tx_size == AV2H_TX_64X64);
        CHECK(leaf->prefix.tx_size_context == 4U);
        if (expected->base_q_index == 224U) {
            CHECK(leaf->prefix.before_all_zero_bits == 7U);
            CHECK(leaf->prefix.after_all_zero_bits == 7U);
            CHECK(leaf->prefix.after_eob_point_bits == 9U);
            CHECK(leaf->prefix.after_eob_bits == 9U);
            CHECK(leaf->prefix.eob == 1U);
            CHECK(leaf->transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(leaf->transform.after_secondary_bits == 9U);
        } else if (expected->base_q_index == 176U) {
            CHECK(leaf->prefix.before_all_zero_bits == 9U);
            CHECK(leaf->prefix.after_all_zero_bits == 9U);
            CHECK(leaf->prefix.after_eob_point_bits == 13U);
            CHECK(leaf->prefix.after_eob_bits == 13U);
            CHECK(leaf->prefix.eob == 3U);
            CHECK(leaf->transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(leaf->transform.before_primary_bits == 13U);
            CHECK(leaf->transform.after_primary_bits == 13U);
            CHECK(leaf->transform.after_secondary_bits == 0U);
        } else if (expected->base_q_index == 128U) {
            CHECK(leaf->prefix.before_all_zero_bits == 17U);
            CHECK(leaf->prefix.after_all_zero_bits == 17U);
            CHECK(leaf->prefix.after_eob_point_bits == 19U);
            CHECK(leaf->prefix.after_eob_bits == 23U);
            CHECK(leaf->prefix.eob == 27U);
            CHECK(leaf->transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(leaf->transform.before_primary_bits == 23U);
            CHECK(leaf->transform.after_primary_bits == 23U);
            CHECK(leaf->transform.after_secondary_bits == 0U);
        } else if (expected->base_q_index == 192U) {
            CHECK(leaf->prefix.before_all_zero_bits == 5U);
            CHECK(leaf->prefix.after_all_zero_bits == 5U);
            CHECK(leaf->prefix.after_eob_point_bits == 13U);
            CHECK(leaf->prefix.after_eob_bits == 18U);
            CHECK(leaf->prefix.eob == 49U);
            CHECK(leaf->transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(leaf->transform.before_primary_bits == 18U);
            CHECK(leaf->transform.after_primary_bits == 18U);
            CHECK(leaf->transform.after_secondary_bits == 18U);
        } else if (expected->width == 384U) {
            CHECK(leaf->prefix.before_all_zero_bits == 6U);
            CHECK(leaf->prefix.after_all_zero_bits == 10U);
            CHECK(leaf->prefix.after_eob_point_bits == 15U);
            CHECK(leaf->prefix.after_eob_bits == 20U);
            CHECK(leaf->prefix.eob == 46U);
            CHECK(leaf->transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(leaf->transform.before_primary_bits == 20U);
            CHECK(leaf->transform.after_primary_bits == 20U);
            CHECK(leaf->transform.after_secondary_bits == 20U);
        } else {
            CHECK(leaf->prefix.before_all_zero_bits == 9U);
            CHECK(leaf->prefix.after_all_zero_bits == 12U);
            CHECK(leaf->prefix.after_eob_point_bits == 17U);
            CHECK(leaf->prefix.after_eob_bits == 23U);
            CHECK(leaf->prefix.eob == 87U);
            CHECK(leaf->transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(leaf->transform.before_primary_bits == 23U);
            CHECK(leaf->transform.after_primary_bits == 23U);
            CHECK(leaf->transform.after_secondary_bits == 23U);
        }
    }
}

static size_t load_artifact(const char *name) {
    char path[256U];
    FILE *source;
    size_t size;
    int written = snprintf(path, sizeof(path), "images/artifacts/%s", name);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        fprintf(stderr, "artifact path is too long: %s\n", name);
        ++failures;
        return 0U;
    }
    source = fopen(path, "rb");
    if (source == 0) {
        fprintf(stderr, "failed to open artifact: %s\n", path);
        ++failures;
        return 0U;
    }
    size = fread(stream, 1U, sizeof(stream), source);
    if (ferror(source) || !feof(source)) {
        fprintf(stderr, "failed to read artifact: %s\n", path);
        ++failures;
        size = 0U;
    }
    fclose(source);
    return size;
}

static const Av2hObu *find_obu(const Av2hObuLedger *ledger, uint8_t type) {
    size_t index;
    for (index = 0U; index < ledger->count; ++index) {
        if (ledger->entries[index].type == type) {
            return &ledger->entries[index];
        }
    }
    return 0;
}

static void check_emerald_q208_full_tile(
    const Av2hObu *frame_obu,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame) {
    Av2hTileGroup group;
    uint8_t luma_partition_above[MAX_MI_COLUMNS];
    uint8_t luma_partition_left[MAX_MI_ROWS];
    uint8_t chroma_partition_above[MAX_MI_COLUMNS];
    uint8_t chroma_partition_left[MAX_MI_ROWS];
    Av2hLumaModeContext luma_mode_above[MAX_MI_COLUMNS];
    Av2hLumaModeContext luma_mode_left[MAX_MI_ROWS];
    uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * MAX_MI_COLUMNS];
    uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * MAX_MI_ROWS];
    Av2hTileLumaModeCell colocated_luma[MAX_MI_COLUMNS * MAX_MI_ROWS];
    uint8_t gdf_block_flags[MAX_GDF_BLOCKS];
    uint8_t cdef_unit_strengths[MAX_CDEF_UNITS];
    uint8_t ccso_unit_controls[AV2H_FRAME_PLANE_COUNT][MAX_CDEF_UNITS];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        MAX_MI_COLUMNS, MAX_MI_ROWS, MAX_MI_COLUMNS * MAX_MI_ROWS,
        gdf_block_flags, MAX_GDF_BLOCKS,
        cdef_unit_strengths, MAX_CDEF_UNITS,
        { ccso_unit_controls[0], ccso_unit_controls[1],
          ccso_unit_controls[2] },
        MAX_CDEF_UNITS
    };
    Av2hReconstructionPlane y_plane = {
        luma_pixels, 192U, 192U, 96U, 192U, 96U,
        luma_reconstructed, 192U / 4U, sizeof(luma_reconstructed)
    };
    Av2hReconstructionPlane u_plane = {
        emerald_u_pixels, 96U, 96U, 48U, 96U, 48U,
        emerald_u_reconstructed, 96U / 4U,
        sizeof(emerald_u_reconstructed)
    };
    Av2hReconstructionPlane v_plane = {
        emerald_v_pixels, 96U, 96U, 48U, 96U, 48U,
        emerald_v_reconstructed, 96U / 4U,
        sizeof(emerald_v_reconstructed)
    };
    Av2hReconstructionPlane u_predictor_plane = {
        emerald_u_predictor, 96U, 96U, 48U, 96U, 48U,
        emerald_u_predictor_reconstructed, 96U / 4U,
        sizeof(emerald_u_predictor_reconstructed)
    };
    Av2hReconstructionPlane v_predictor_plane = {
        emerald_v_predictor, 96U, 96U, 48U, 96U, 48U,
        emerald_v_predictor_reconstructed, 96U / 4U,
        sizeof(emerald_v_predictor_reconstructed)
    };
    Av2hTileDecoder decoder;
    Av2hReconstructionScratch scratch;
    size_t luma_index = 0U;
    size_t chroma_index = 0U;
    size_t handoff_index = 0U;

    REQUIRE(av2h_tile_group_single_parse(
                (Av2hSpan){ stream + frame_obu->payload_offset,
                             frame_obu->payload_size },
                frame->uncompressed_header_bits, &group) == AV2H_OK);
    REQUIRE(av2h_tile_decoder_init(
                &decoder, group.entropy, sequence, frame, &storage) ==
            AV2H_OK);
    av2h_memory_fill(luma_pixels, 0U, sizeof(luma_pixels));
    av2h_memory_fill(luma_reconstructed, 0U, sizeof(luma_reconstructed));
    av2h_memory_fill(emerald_u_pixels, 0U, sizeof(emerald_u_pixels));
    av2h_memory_fill(emerald_v_pixels, 0U, sizeof(emerald_v_pixels));
    av2h_memory_fill(emerald_u_predictor, 0U, sizeof(emerald_u_predictor));
    av2h_memory_fill(emerald_v_predictor, 0U, sizeof(emerald_v_predictor));
    av2h_memory_fill(
        emerald_u_reconstructed, 0U, sizeof(emerald_u_reconstructed));
    av2h_memory_fill(
        emerald_v_reconstructed, 0U, sizeof(emerald_v_reconstructed));
    av2h_memory_fill(
        emerald_u_predictor_reconstructed, 0U,
        sizeof(emerald_u_predictor_reconstructed));
    av2h_memory_fill(
        emerald_v_predictor_reconstructed, 0U,
        sizeof(emerald_v_predictor_reconstructed));

    while (decoder.phase != AV2H_TILE_DECODE_DONE &&
           decoder.status == AV2H_OK) {
        uint8_t has_leaf = 0U;
        Av2hStatus status;
        if (decoder.phase == AV2H_TILE_DECODE_LUMA) {
            Av2hTileLumaLeaf leaf;
            Av2hFirstCoeffBlock block;
            const EmeraldQ208LumaLeaf *expected;
            size_t coefficient_index;
            uint16_t block_x;
            uint16_t block_y;
            av2h_memory_fill(&leaf, 0U, sizeof(leaf));
            av2h_memory_fill(&block, 0U, sizeof(block));
            status = av2h_tile_decoder_next_luma(
                &decoder, &leaf, &block, &has_leaf);
            if (status != AV2H_OK) {
                fprintf(stderr,
                        "Emerald luma %zu failed at tell %zu: %d "
                        "node=%u,%u/%u mode=%u set/index=%u/%u "
                        "tx=%u eob=%u type=%u "
                        "mode_tells=%zu/%zu tx_tells=%zu/%zu "
                        "diag=%u/%u/%u\n",
                        luma_index, av2h_range_tell(&decoder.range), status,
                        leaf.node.row, leaf.node.column, leaf.node.block_size,
                        leaf.mode.mode, leaf.mode.mode_set,
                        leaf.mode.mode_index, leaf.prefix.tx_size,
                        leaf.prefix.eob,
                        leaf.transform.primary_type,
                        leaf.mode.before_mode_bits,
                        leaf.mode.after_mode_index_bits,
                        leaf.prefix.before_all_zero_bits,
                        leaf.prefix.after_eob_bits,
                        block.diagnostic_position, block.diagnostic_context,
                        block.diagnostic_kind);
                ++failures;
                return;
            }
            if (!has_leaf) {
                REQUIRE(handoff_index <
                        sizeof(emerald_q208_handoffs) /
                            sizeof(emerald_q208_handoffs[0]));
                CHECK(av2h_range_tell(&decoder.range) ==
                      emerald_q208_handoffs[handoff_index].luma_end.tell);
                if (decoder.range.range !=
                    emerald_q208_handoffs[handoff_index].luma_end.range) {
                    fprintf(stderr,
                            "Emerald handoff %zu luma range=%u expected=%u\n",
                            handoff_index, decoder.range.range,
                            emerald_q208_handoffs[
                                handoff_index].luma_end.range);
                }
                CHECK(decoder.range.range ==
                      emerald_q208_handoffs[handoff_index].luma_end.range);
                CHECK(decoder.event == AV2H_TILE_EVENT_ROOT_HANDOFF);
                continue;
            }
            REQUIRE(luma_index <
                    sizeof(emerald_q208_luma_leaves) /
                        sizeof(emerald_q208_luma_leaves[0]));
            expected = &emerald_q208_luma_leaves[luma_index];
            CHECK(leaf.node.row == expected->row);
            CHECK(leaf.node.column == expected->column);
            CHECK(leaf.node.block_size == expected->block_size);
            CHECK(leaf.mode.mode == expected->mode);
            CHECK(leaf.mode.angle_delta == expected->angle_delta);
            CHECK(leaf.mode.before_mode_bits == expected->syntax_start_tell);
            CHECK(leaf.prefix.all_zero == expected->all_zero);
            CHECK(leaf.prefix.eob == expected->eob);
            CHECK(leaf.transform.primary_type == expected->tx_type);
            CHECK(block.width == (expected->tx_width > 32U ?
                                  32U : expected->tx_width));
            CHECK(block.height == (expected->tx_height > 32U ?
                                   32U : expected->tx_height));
            CHECK(block.coefficient_count == expected->coefficient_count);
            if (block.nonzero_count != expected->nonzero_count ||
                av2h_range_tell(&decoder.range) !=
                    expected->syntax_end_tell) {
                fprintf(stderr,
                        "Emerald luma actual %zu tell=%zu expected=%zu "
                        "nz=%u expected=%u diag=%u/%u/%u\n",
                        luma_index, av2h_range_tell(&decoder.range),
                        expected->syntax_end_tell, block.nonzero_count,
                        expected->nonzero_count, block.diagnostic_position,
                        block.diagnostic_context, block.diagnostic_kind);
            }
            CHECK(block.nonzero_count == expected->nonzero_count);
            for (coefficient_index = 0U;
                 coefficient_index < expected->nonzero_count;
                 ++coefficient_index) {
                const EmeraldQ208Coefficient *coefficient =
                    &emerald_q208_luma_coefficients[
                        expected->coefficient_offset + coefficient_index];
                CHECK(block.quantized[coefficient->position] ==
                      coefficient->quantized);
                CHECK(block.dequantized[coefficient->position] ==
                      coefficient->dequantized);
            }
            CHECK(av2h_range_tell(&decoder.range) ==
                  expected->syntax_end_tell);
            status = av2h_luma_reconstruct_to_plane(
                &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                &block, &y_plane, &scratch);
            if (status != AV2H_OK) {
                fprintf(stderr, "Emerald luma reconstruction %zu: %d\n",
                        luma_index, status);
                ++failures;
                return;
            }
            block_x = (uint16_t)(leaf.node.column * 4U);
            block_y = (uint16_t)(leaf.node.row * 4U);
            {
                uint32_t predictor_djb2 = trace_sample_hash(
                    scratch.predictor, expected->tx_width,
                    expected->tx_width, expected->tx_height);
                uint64_t predictor_fnv = block_pixel_hash(
                    scratch.predictor, expected->tx_width,
                    expected->tx_width, expected->tx_height);
                uint32_t recon_djb2 = trace_sample_hash(
                    luma_pixels + (size_t)block_y * 192U + block_x,
                    192U, expected->tx_width, expected->tx_height);
                uint64_t recon_fnv = block_pixel_hash(
                    luma_pixels + (size_t)block_y * 192U + block_x,
                    192U, expected->tx_width, expected->tx_height);
                if (predictor_djb2 != expected->predictor_djb2 ||
                    predictor_fnv != expected->predictor_fnv1a64 ||
                    recon_djb2 != expected->prefilter_djb2 ||
                    recon_fnv != expected->prefilter_fnv1a64) {
                    fprintf(stderr,
                            "Emerald luma hash %zu pred=%08x/%016llx "
                            "exp=%08x/%016llx recon=%08x/%016llx "
                            "exp=%08x/%016llx\n",
                            luma_index, predictor_djb2,
                            (unsigned long long)predictor_fnv,
                            expected->predictor_djb2,
                            (unsigned long long)expected->predictor_fnv1a64,
                            recon_djb2, (unsigned long long)recon_fnv,
                            expected->prefilter_djb2,
                            (unsigned long long)expected->prefilter_fnv1a64);
                }
            }
            CHECK(trace_sample_hash(
                      scratch.predictor, expected->tx_width,
                      expected->tx_width, expected->tx_height) ==
                  expected->predictor_djb2);
            CHECK(block_pixel_hash(
                      scratch.predictor, expected->tx_width,
                      expected->tx_width, expected->tx_height) ==
                  expected->predictor_fnv1a64);
            CHECK(trace_sample_hash(
                      luma_pixels + (size_t)block_y * 192U + block_x,
                      192U, expected->tx_width, expected->tx_height) ==
                  expected->prefilter_djb2);
            CHECK(block_pixel_hash(
                      luma_pixels + (size_t)block_y * 192U + block_x,
                      192U, expected->tx_width, expected->tx_height) ==
                  expected->prefilter_fnv1a64);
            REQUIRE(av2h_tile_decoder_commit_leaf(
                        &decoder, leaf.node) == AV2H_OK);
            ++luma_index;
        } else {
            Av2hTileChromaLeaf leaf;
            Av2hFirstCoeffBlock blocks[2U];
            const EmeraldQ208ChromaLeaf *expected;
            const EmeraldQ208ChromaPlane *expected_planes[2U];
            Av2hReconstructionPlane *planes[2U] = { &u_plane, &v_plane };
            Av2hReconstructionPlane *predictor_planes[2U] = {
                &u_predictor_plane, &v_predictor_plane
            };
            size_t plane_index;
            av2h_memory_fill(&leaf, 0U, sizeof(leaf));
            av2h_memory_fill(blocks, 0U, sizeof(blocks));
            status = av2h_tile_decoder_next_chroma(
                &decoder, &leaf, &blocks[0], &blocks[1], &has_leaf);
            if (status != AV2H_OK) {
                fprintf(stderr,
                        "Emerald chroma %zu failed at tell %zu: %d "
                        "node=%u,%u/%u mode=%u "
                        "u=%u/%u/%u/%u/%u v=%u/%u/%u/%u/%u\n",
                        chroma_index, av2h_range_tell(&decoder.range), status,
                        leaf.node.row, leaf.node.column, leaf.node.block_size,
                        leaf.mode.uv_mode,
                        leaf.prefix[0].tx_size,
                        leaf.prefix[0].txb_skip_context,
                        leaf.prefix[0].all_zero, leaf.prefix[0].eob,
                        blocks[0].diagnostic_kind,
                        leaf.prefix[1].tx_size,
                        leaf.prefix[1].txb_skip_context,
                        leaf.prefix[1].all_zero, leaf.prefix[1].eob,
                        blocks[1].diagnostic_kind);
                ++failures;
                return;
            }
            if (!has_leaf) {
                REQUIRE(handoff_index <
                        sizeof(emerald_q208_handoffs) /
                            sizeof(emerald_q208_handoffs[0]));
                CHECK(av2h_range_tell(&decoder.range) ==
                      emerald_q208_handoffs[handoff_index].chroma_end.tell);
                if (decoder.range.range !=
                    emerald_q208_handoffs[handoff_index].chroma_end.range) {
                    fprintf(stderr,
                            "Emerald handoff %zu chroma range=%u expected=%u\n",
                            handoff_index, decoder.range.range,
                            emerald_q208_handoffs[
                                handoff_index].chroma_end.range);
                }
                CHECK(decoder.range.range ==
                      emerald_q208_handoffs[handoff_index].chroma_end.range);
                CHECK(decoder.event ==
                      (handoff_index + 1U ==
                               sizeof(emerald_q208_handoffs) /
                                   sizeof(emerald_q208_handoffs[0]) ?
                           AV2H_TILE_EVENT_DONE :
                           AV2H_TILE_EVENT_ROOT_HANDOFF));
                ++handoff_index;
                continue;
            }
            REQUIRE(chroma_index <
                    sizeof(emerald_q208_chroma_leaves) /
                        sizeof(emerald_q208_chroma_leaves[0]));
            expected = &emerald_q208_chroma_leaves[chroma_index];
            expected_planes[0] = &expected->u;
            expected_planes[1] = &expected->v;
            CHECK(leaf.node.row == expected->row);
            CHECK(leaf.node.column == expected->column);
            CHECK(leaf.mode.uv_mode == expected->mode);
            CHECK(leaf.mode.cfl_index == expected->cfl_index);
            CHECK(leaf.mode.after_mode_bits == expected->mode_end_tell);
            for (plane_index = 0U; plane_index < 2U; ++plane_index) {
                const EmeraldQ208ChromaPlane *expected_plane =
                    expected_planes[plane_index];
                const Av2hFirstCoeffBlock *block = &blocks[plane_index];
                size_t coefficient_index;
                uint16_t block_x = (uint16_t)(leaf.node.column * 2U);
                uint16_t block_y = (uint16_t)(leaf.node.row * 2U);
                CHECK(leaf.prefix[plane_index].before_all_zero_bits ==
                      expected_plane->syntax_start_tell);
                CHECK(leaf.prefix[plane_index].all_zero ==
                      expected_plane->all_zero);
                CHECK(leaf.prefix[plane_index].eob == expected_plane->eob);
                CHECK(leaf.transform[plane_index].primary_type ==
                      expected_plane->tx_type);
                CHECK(block->width == expected_plane->tx_width);
                CHECK(block->height == expected_plane->tx_height);
                CHECK(block->nonzero_count ==
                      expected_plane->nonzero_count);
                for (coefficient_index = 0U;
                     coefficient_index < expected_plane->nonzero_count;
                     ++coefficient_index) {
                    const EmeraldQ208Coefficient *coefficient =
                        &emerald_q208_chroma_coefficients[
                            expected_plane->coefficient_offset +
                            coefficient_index];
                    CHECK(block->quantized[coefficient->position] ==
                          coefficient->quantized);
                    CHECK(block->dequantized[coefficient->position] ==
                          coefficient->dequantized);
                }
                status = av2h_chroma_reconstruct_to_plane_ex(
                    &leaf.node, &leaf.mode, &leaf.prefix[plane_index],
                    &leaf.transform[plane_index], block,
                    planes[plane_index], &y_plane,
                    predictor_planes[plane_index], &scratch);
                if (status != AV2H_OK) {
                    fprintf(stderr,
                            "Emerald chroma reconstruction %zu/%zu: %d "
                            "mode=%u tx=%u zero=%u eob=%u block=%ux%u "
                            "nz=%u\n",
                            chroma_index, plane_index, status,
                            leaf.mode.mode, leaf.prefix[plane_index].tx_size,
                            leaf.prefix[plane_index].all_zero,
                            leaf.prefix[plane_index].eob,
                            block->width, block->height,
                            block->nonzero_count);
                    ++failures;
                    return;
                }
                {
                    uint32_t actual_djb2 = trace_sample_hash(
                        planes[plane_index]->data +
                            (size_t)block_y * 96U + block_x,
                        96U, block->width, block->height);
                    uint64_t actual_fnv = block_pixel_hash(
                        planes[plane_index]->data +
                            (size_t)block_y * 96U + block_x,
                        96U, block->width, block->height);
                    if (actual_djb2 != expected_plane->prefilter_djb2 ||
                        actual_fnv != expected_plane->prefilter_fnv1a64) {
                        fprintf(stderr,
                                "Emerald chroma hash %zu/%zu actual="
                                "%08x/%016llx expected=%08x/%016llx\n",
                                chroma_index, plane_index, actual_djb2,
                                (unsigned long long)actual_fnv,
                                expected_plane->prefilter_djb2,
                                (unsigned long long)
                                    expected_plane->prefilter_fnv1a64);
                    }
                }
                CHECK(trace_sample_hash(
                          planes[plane_index]->data +
                              (size_t)block_y * 96U + block_x,
                          96U, block->width, block->height) ==
                      expected_plane->prefilter_djb2);
                CHECK(block_pixel_hash(
                          planes[plane_index]->data +
                              (size_t)block_y * 96U + block_x,
                          96U, block->width, block->height) ==
                      expected_plane->prefilter_fnv1a64);
                CHECK((leaf.prefix[plane_index].all_zero ?
                           leaf.prefix[plane_index].after_all_zero_bits :
                           block->after_coefficients_bits) ==
                      expected_plane->syntax_end_tell);
            }
            REQUIRE(av2h_tile_decoder_commit_leaf(
                        &decoder, leaf.node) == AV2H_OK);
            ++chroma_index;
        }
    }
    CHECK(decoder.status == AV2H_OK);
    CHECK(decoder.phase == AV2H_TILE_DECODE_DONE);
    CHECK(luma_index == 20U);
    CHECK(chroma_index == 11U);
    CHECK(handoff_index == 6U);
    CHECK(av2h_range_tell(&decoder.range) == emerald_q208_tile_done.tell);
    CHECK(decoder.range.range == emerald_q208_tile_done.range);
    CHECK(trace_sample_hash(luma_pixels, 192U, 192U, 96U) ==
          emerald_q208_prefilter_hashes[0].sample_djb2);
    CHECK(block_pixel_hash(luma_pixels, 192U, 192U, 96U) ==
          emerald_q208_prefilter_hashes[0].sample_fnv1a64);
    CHECK(trace_sample_hash(emerald_u_pixels, 96U, 96U, 48U) ==
          emerald_q208_prefilter_hashes[1].sample_djb2);
    CHECK(block_pixel_hash(emerald_u_pixels, 96U, 96U, 48U) ==
          emerald_q208_prefilter_hashes[1].sample_fnv1a64);
    CHECK(trace_sample_hash(emerald_v_pixels, 96U, 96U, 48U) ==
          emerald_q208_prefilter_hashes[2].sample_djb2);
    CHECK(block_pixel_hash(emerald_v_pixels, 96U, 96U, 48U) ==
          emerald_q208_prefilter_hashes[2].sample_fnv1a64);
}

static void check_first_luma(const ArtifactExpectation *expected,
                             const Av2hObu *frame_obu,
                             const Av2hSequenceState *sequence,
                             const Av2hFrameState *frame) {
    Av2hTileGroup group;
    uint8_t luma_partition_above[MAX_MI_COLUMNS];
    uint8_t luma_partition_left[MAX_MI_ROWS];
    uint8_t chroma_partition_above[MAX_MI_COLUMNS];
    uint8_t chroma_partition_left[MAX_MI_ROWS];
    Av2hLumaModeContext luma_mode_above[MAX_MI_COLUMNS];
    Av2hLumaModeContext luma_mode_left[MAX_MI_ROWS];
    uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * MAX_MI_COLUMNS];
    uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * MAX_MI_ROWS];
    Av2hTileLumaModeCell colocated_luma[MAX_MI_COLUMNS * MAX_MI_ROWS];
    uint8_t gdf_block_flags[MAX_GDF_BLOCKS];
    uint8_t cdef_unit_strengths[MAX_CDEF_UNITS];
    uint8_t ccso_unit_controls[AV2H_FRAME_PLANE_COUNT][MAX_CDEF_UNITS];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        MAX_MI_COLUMNS, MAX_MI_ROWS, MAX_MI_COLUMNS * MAX_MI_ROWS,
        gdf_block_flags, MAX_GDF_BLOCKS,
        cdef_unit_strengths, MAX_CDEF_UNITS,
        { ccso_unit_controls[0], ccso_unit_controls[1],
          ccso_unit_controls[2] },
        MAX_CDEF_UNITS
    };
    Av2hTileDecoder decoder;
    Av2hTileLumaLeaf leaf;
    Av2hFirstCoeffBlock block;
    Av2hStatus status;
    uint8_t has_leaf = 0U;

    REQUIRE(av2h_tile_group_single_parse(
                (Av2hSpan){ stream + frame_obu->payload_offset,
                             frame_obu->payload_size },
                frame->uncompressed_header_bits, &group) == AV2H_OK);
    status = av2h_tile_decoder_init(
        &decoder, group.entropy, sequence, frame, &storage);
    REQUIRE(status == AV2H_OK);
    av2h_memory_fill(&leaf, 0U, sizeof(leaf));
    av2h_memory_fill(&block, 0U, sizeof(block));
    status = av2h_tile_decoder_next_luma(
        &decoder, &leaf, &block, &has_leaf);
    CHECK(status == expected->first_status);
    CHECK(has_leaf == 1U);
    CHECK(leaf.node.row == 0U);
    CHECK(leaf.node.column == 0U);
    CHECK(leaf.node.block_size == expected->first_block);
    CHECK(av2h_range_tell(&decoder.range) == expected->first_tell);
    check_advanced_frontier(expected, &leaf);
    if ((expected->base_q_index == 224U && expected->width == 333U) ||
        (expected->base_q_index == 80U && expected->width == 384U) ||
        (expected->base_q_index == 176U &&
         expected->first_block == AV2H_BLOCK_64X64 &&
         expected->gdf_mode == 2U)) {
        CHECK(decoder.leaf_cdfs.palette_y_mode[2] == 1U);
    }
    if (expected->gdf_mode == 2U) {
        uint8_t expected_cdef_count;
        uint8_t expected_cdef_index;
        uint8_t expected_ccso_mask;
        uint8_t expected_ccso_controls[AV2H_FRAME_PLANE_COUNT] = { 0U };
        uint8_t expected_flag =
            expected->base_q_index == 48U ||
            expected->base_q_index == 64U ? 0U : 1U;
        size_t expected_cdef_tell;
        size_t expected_ccso_tell;
        CHECK(leaf.mode.gdf_flags_read == 1U);
        CHECK(leaf.mode.before_gdf_bits + 1U ==
              leaf.mode.after_gdf_bits);
        CHECK(decoder.gdf_entropy.decoded_count == 1U);
        CHECK(gdf_block_flags[0] == expected_flag);
        switch (expected->base_q_index) {
            case 48U:
                expected_cdef_count = 8U;
                expected_cdef_index = 6U;
                expected_cdef_tell = 11U;
                expected_ccso_mask = 6U;
                expected_ccso_tell = 12U;
                break;
            case 64U:
                expected_cdef_count = 8U;
                expected_cdef_index = 3U;
                expected_cdef_tell = 21U;
                expected_ccso_mask = 6U;
                expected_ccso_controls[1] = 1U;
                expected_ccso_controls[2] = 1U;
                expected_ccso_tell = 24U;
                break;
            case 96U:
                expected_cdef_count = 3U;
                expected_cdef_index = 0U;
                expected_cdef_tell = 11U;
                expected_ccso_mask = 2U;
                expected_ccso_controls[1] = 1U;
                expected_ccso_tell = 13U;
                break;
            case 112U:
                expected_cdef_count = 6U;
                expected_cdef_index = 5U;
                expected_cdef_tell = 16U;
                expected_ccso_mask = 7U;
                expected_ccso_controls[0] = 1U;
                expected_ccso_controls[1] = 1U;
                expected_ccso_tell = 20U;
                break;
            case 128U:
                expected_cdef_count = 3U;
                expected_cdef_index = 1U;
                expected_cdef_tell = 4U;
                expected_ccso_mask = 7U;
                expected_ccso_controls[0] = 1U;
                expected_ccso_controls[1] = 1U;
                expected_ccso_controls[2] = 1U;
                expected_ccso_tell = 9U;
                break;
            default:
                expected_cdef_count = 1U;
                expected_cdef_index = 0U;
                expected_cdef_tell = 3U;
                expected_ccso_mask = 7U;
                expected_ccso_controls[0] = 1U;
                expected_ccso_controls[1] = 1U;
                expected_ccso_controls[2] = 1U;
                expected_ccso_tell = 8U;
                break;
        }
        CHECK(frame->cdef_strength_count == expected_cdef_count);
        CHECK(leaf.mode.before_cdef_bits == leaf.mode.after_gdf_bits);
        CHECK(leaf.mode.cdef_strength_index == expected_cdef_index);
        CHECK(leaf.mode.cdef_strengths_read ==
              (expected_cdef_count > 1U ? 1U : 0U));
        CHECK(leaf.mode.after_cdef_bits == expected_cdef_tell);
        if (expected_cdef_count > 1U) {
            CHECK(cdef_unit_strengths[0] == expected_cdef_index);
        }
        CHECK(leaf.mode.before_ccso_bits == leaf.mode.after_cdef_bits);
        CHECK(leaf.mode.after_ccso_bits == expected_ccso_tell);
        CHECK(leaf.mode.ccso_present_mask == expected_ccso_mask);
        CHECK(leaf.mode.ccso_controls_read ==
              (uint8_t)((expected_ccso_mask & 1U) +
                        ((expected_ccso_mask >> 1U) & 1U) +
                        ((expected_ccso_mask >> 2U) & 1U)));
        CHECK(decoder.ccso_entropy.unit_size == 256U);
        for (size_t plane = 0U;
             plane < AV2H_FRAME_PLANE_COUNT;
             ++plane) {
            CHECK(leaf.mode.ccso_block_control[plane] ==
                  expected_ccso_controls[plane]);
            if ((expected_ccso_mask & (uint8_t)(1U << plane)) != 0U) {
                CHECK(ccso_unit_controls[plane][0] ==
                      expected_ccso_controls[plane]);
            }
        }
    }
    if (expected->decoded != 0) {
        CHECK(status == AV2H_OK);
        check_decoded_block(expected->decoded, &leaf, &block);
        if (expected->base_q_index == 160U &&
            expected->first_block == AV2H_BLOCK_16X32) {
            static const uint8_t cuckoo_first_row[16U] = {
                117U, 117U, 116U, 116U, 115U, 114U, 113U, 112U,
                111U, 110U, 109U, 108U, 107U, 107U, 106U, 106U
            };
            static const uint8_t cuckoo_last_row[16U] = {
                114U, 114U, 114U, 115U, 116U, 117U, 118U, 119U,
                120U, 121U, 122U, 123U, 123U, 124U, 124U, 125U
            };
            static const uint8_t pastell_first_row[16U] = {
                21U, 29U, 43U, 62U, 82U, 99U, 112U, 122U,
                126U, 130U, 131U, 131U, 132U, 131U, 130U, 130U
            };
            static const uint8_t pastell_last_row[16U] = {
                95U, 97U, 101U, 105U, 107U, 108U, 107U, 105U,
                103U, 102U, 103U, 106U, 111U, 116U, 120U, 125U
            };
            const uint8_t *first_row =
                expected->width == 157U ? cuckoo_first_row :
                pastell_first_row;
            const uint8_t *last_row =
                expected->width == 157U ? cuckoo_last_row :
                pastell_last_row;
            uint64_t expected_hash = expected->width == 157U ?
                UINT64_C(0xfb97d9776de897f5) :
                UINT64_C(0x73f9aa733e29f1c7);
            size_t reconstructed_stride =
                ((size_t)expected->width + 3U) / 4U;
            Av2hReconstructionPlane plane = {
                luma_pixels, expected->width, expected->width,
                expected->height, expected->width, expected->height,
                luma_reconstructed, reconstructed_stride,
                sizeof(luma_reconstructed)
            };
            Av2hReconstructionScratch scratch;
            size_t index;

            av2h_memory_fill(luma_pixels, 0U, sizeof(luma_pixels));
            av2h_memory_fill(
                luma_reconstructed, 0U, sizeof(luma_reconstructed));
            REQUIRE(av2h_luma_reconstruct_to_plane(
                        &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                        &block, &plane, &scratch) == AV2H_OK);
            CHECK(block_pixel_hash(
                      luma_pixels, expected->width, 16U, 32U) ==
                  expected_hash);
            for (index = 0U; index < 16U; ++index) {
                CHECK(luma_pixels[index] == first_row[index]);
                CHECK(luma_pixels[31U * expected->width + index] ==
                      last_row[index]);
            }
            REQUIRE(av2h_tile_decoder_commit_leaf(&decoder, leaf.node) ==
                    AV2H_OK);
            av2h_memory_fill(&leaf, 0U, sizeof(leaf));
            av2h_memory_fill(&block, 0U, sizeof(block));
            status = av2h_tile_decoder_next_luma(
                &decoder, &leaf, &block, &has_leaf);
            CHECK(status == AV2H_UNSUPPORTED);
            CHECK(has_leaf == 1U);
            CHECK(leaf.node.row == 0U);
            CHECK(leaf.node.column == 4U);
            if (expected->width == 157U) {
                CHECK(leaf.node.block_size == AV2H_BLOCK_8X16);
                CHECK(av2h_range_tell(&decoder.range) == 41U);
                CHECK(leaf.mode.before_mode_bits == 0U);
                CHECK(leaf.prefix.before_all_zero_bits == 0U);
            } else {
                CHECK(leaf.node.block_size == AV2H_BLOCK_16X32);
                CHECK(leaf.mode.before_mode_bits == 66U);
                CHECK(leaf.mode.after_mode_set_bits == 66U);
                CHECK(leaf.mode.after_mode_index_bits == 69U);
                CHECK(leaf.mode.after_fsc_bits == 69U);
                CHECK(leaf.mode.after_mrl_bits == 69U);
                CHECK(leaf.mode.mode == AV2H_INTRA_SMOOTH);
                CHECK(leaf.prefix.before_all_zero_bits == 69U);
                CHECK(leaf.prefix.after_all_zero_bits == 69U);
                CHECK(leaf.prefix.eob_point == 3U);
                CHECK(leaf.prefix.after_eob_point_bits == 73U);
                CHECK(leaf.prefix.after_eob_extra_bit == 73U);
                CHECK(leaf.prefix.after_eob_bits == 73U);
                CHECK(leaf.prefix.eob == 3U);
                CHECK(leaf.transform.primary_type == AV2H_TX_DCT_DCT);
                CHECK(leaf.transform.primary_signaled == 1U);
                CHECK(leaf.transform.primary_long_side_symbol == 1U);
                CHECK(leaf.transform.primary_symbol == 0U);
                CHECK(leaf.transform.before_primary_bits == 73U);
                CHECK(leaf.transform.after_primary_bits == 74U);
                CHECK(leaf.transform.secondary_signaled == 1U);
                CHECK(leaf.transform.secondary_symbol == 0U);
                CHECK(leaf.transform.after_secondary_bits == 78U);
                CHECK(block.before_levels_bits == 0U);
                CHECK(av2h_range_tell(&decoder.range) == 78U);
            }
        } else if (expected->base_q_index == 96U &&
                   expected->first_block == AV2H_BLOCK_32X32) {
            static const uint8_t first_row[32U] = {
                138U, 144U, 152U, 156U, 156U, 152U, 147U, 147U,
                151U, 155U, 158U, 158U, 154U, 151U, 148U, 146U,
                144U, 139U, 131U, 121U, 110U, 103U, 100U, 103U,
                109U, 113U, 113U, 109U, 102U, 95U, 89U, 87U
            };
            static const uint8_t last_row[32U] = {
                159U, 158U, 157U, 158U, 162U, 167U, 172U, 174U,
                173U, 171U, 166U, 161U, 156U, 151U, 145U, 139U,
                133U, 126U, 119U, 114U, 114U, 117U, 124U, 130U,
                134U, 134U, 132U, 129U, 127U, 126U, 126U, 127U
            };
            size_t reconstructed_stride =
                ((size_t)expected->width + 3U) / 4U;
            Av2hReconstructionPlane plane = {
                luma_pixels, expected->width, expected->width,
                expected->height, expected->width, expected->height,
                luma_reconstructed, reconstructed_stride,
                sizeof(luma_reconstructed)
            };
            Av2hReconstructionScratch scratch;
            size_t index;

            av2h_memory_fill(luma_pixels, 0U, sizeof(luma_pixels));
            av2h_memory_fill(
                luma_reconstructed, 0U, sizeof(luma_reconstructed));
            REQUIRE(av2h_luma_reconstruct_to_plane(
                        &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                        &block, &plane, &scratch) == AV2H_OK);
            CHECK(block_pixel_hash(scratch.predictor, 32U, 32U, 32U) ==
                  UINT64_C(0x75290844cee19be7));
            CHECK(block_pixel_hash(
                      luma_pixels, expected->width, 32U, 32U) ==
                  UINT64_C(0x51b573f06c63948a));
            for (index = 0U; index < 32U; ++index) {
                CHECK(luma_pixels[index] == first_row[index]);
                CHECK(luma_pixels[31U * expected->width + index] ==
                      last_row[index]);
            }
            REQUIRE(av2h_tile_decoder_commit_leaf(&decoder, leaf.node) ==
                    AV2H_OK);
            av2h_memory_fill(&leaf, 0U, sizeof(leaf));
            av2h_memory_fill(&block, 0U, sizeof(block));
            status = av2h_tile_decoder_next_luma(
                &decoder, &leaf, &block, &has_leaf);
            CHECK(status == AV2H_UNSUPPORTED);
            CHECK(has_leaf == 1U);
            CHECK(leaf.node.row == 0U);
            CHECK(leaf.node.column == 8U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_16X16);
            CHECK(av2h_range_tell(&decoder.range) == 476U);
        }
    } else if (expected->base_q_index == 208U &&
               expected->first_block == AV2H_BLOCK_64X16) {
        size_t reconstructed_stride =
            ((size_t)expected->width + 3U) / 4U;
        Av2hReconstructionPlane plane = {
            luma_pixels, expected->width, expected->width,
            expected->height, expected->width, expected->height,
            luma_reconstructed, reconstructed_stride,
            sizeof(luma_reconstructed)
        };
        Av2hReconstructionScratch scratch;
        CHECK(status == AV2H_OK);
        CHECK(leaf.transform.primary_type == AV2H_TX_DCT_DCT);
        CHECK(leaf.transform.before_primary_bits == 17U);
        CHECK(leaf.transform.after_primary_bits == 17U);
        CHECK(leaf.transform.after_secondary_bits == 17U);
        CHECK(block.width == 32U);
        CHECK(block.height == 16U);
        CHECK(block.coefficient_count == 512U);
        CHECK(block.nonzero_count == 0U);
        CHECK(block.before_levels_bits == 17U);
        CHECK(block.after_levels_bits == 17U);
        CHECK(block.after_coefficients_bits == 17U);
        av2h_memory_fill(luma_pixels, 0U, sizeof(luma_pixels));
        av2h_memory_fill(
            luma_reconstructed, 0U, sizeof(luma_reconstructed));
        REQUIRE(av2h_luma_reconstruct_to_plane(
                    &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                    &block, &plane, &scratch) == AV2H_OK);
        CHECK(block_pixel_hash(scratch.predictor, 64U, 64U, 16U) ==
              UINT64_C(0xd495a039fba13383));
        CHECK(block_pixel_hash(
                  luma_pixels, expected->width, 64U, 16U) ==
              UINT64_C(0xd495a039fba13383));
        REQUIRE(av2h_tile_decoder_commit_leaf(&decoder, leaf.node) ==
                AV2H_OK);
        av2h_memory_fill(&leaf, 0U, sizeof(leaf));
        av2h_memory_fill(&block, 0U, sizeof(block));
        status = av2h_tile_decoder_next_luma(
            &decoder, &leaf, &block, &has_leaf);
        CHECK(status == AV2H_UNSUPPORTED);
        CHECK(has_leaf == 1U);
        CHECK(leaf.node.row == 4U);
        CHECK(leaf.node.column == 0U);
        CHECK(leaf.node.block_size == AV2H_BLOCK_32X16);
        CHECK(av2h_range_tell(&decoder.range) == 19U);
    }
    if (expected->width == 192U && expected->height == 96U &&
        expected->base_q_index == 208U) {
        static const uint8_t expected_left[32U] = {
            75U, 75U, 76U, 76U, 77U, 77U, 78U, 79U,
            80U, 82U, 83U, 84U, 86U, 87U, 88U, 90U,
            91U, 93U, 94U, 96U, 97U, 99U, 100U, 101U,
            102U, 103U, 104U, 105U, 105U, 106U, 106U, 106U
        };
        static const uint8_t expected_ibp_edge[32U * 8U] = {
            84U, 85U, 86U, 87U, 88U, 88U, 89U, 90U,
            84U, 85U, 86U, 87U, 88U, 88U, 89U, 90U,
            84U, 85U, 86U, 87U, 88U, 89U, 89U, 90U,
            84U, 85U, 86U, 87U, 88U, 89U, 89U, 90U,
            85U, 86U, 86U, 87U, 88U, 89U, 89U, 90U,
            85U, 86U, 86U, 87U, 88U, 89U, 89U, 90U,
            85U, 86U, 87U, 87U, 88U, 89U, 90U, 90U,
            86U, 86U, 87U, 88U, 88U, 89U, 90U, 90U,
            86U, 87U, 87U, 88U, 89U, 89U, 90U, 90U,
            87U, 87U, 88U, 88U, 89U, 90U, 90U, 91U,
            87U, 88U, 88U, 89U, 89U, 90U, 90U, 91U,
            88U, 88U, 89U, 89U, 89U, 90U, 90U, 91U,
            89U, 89U, 89U, 90U, 90U, 90U, 90U, 91U,
            89U, 89U, 90U, 90U, 90U, 90U, 91U, 91U,
            90U, 90U, 90U, 90U, 90U, 91U, 91U, 91U,
            91U, 91U, 91U, 91U, 91U, 91U, 91U, 91U,
            91U, 91U, 91U, 91U, 91U, 91U, 91U, 91U,
            92U, 92U, 92U, 92U, 91U, 91U, 91U, 91U,
            92U, 92U, 92U, 92U, 92U, 91U, 91U, 91U,
            93U, 93U, 93U, 92U, 92U, 92U, 92U, 91U,
            94U, 93U, 93U, 93U, 92U, 92U, 92U, 91U,
            95U, 94U, 94U, 93U, 93U, 92U, 92U, 91U,
            95U, 95U, 94U, 94U, 93U, 92U, 92U, 91U,
            95U, 95U, 94U, 94U, 93U, 93U, 92U, 92U,
            96U, 95U, 95U, 94U, 93U, 93U, 92U, 92U,
            96U, 96U, 95U, 94U, 94U, 93U, 92U, 92U,
            97U, 96U, 95U, 95U, 94U, 93U, 92U, 92U,
            97U, 96U, 96U, 95U, 94U, 93U, 93U, 92U,
            97U, 96U, 96U, 95U, 94U, 93U, 93U, 92U,
            98U, 97U, 96U, 95U, 94U, 93U, 93U, 92U,
            98U, 97U, 96U, 95U, 94U, 93U, 93U, 92U,
            98U, 97U, 96U, 95U, 94U, 93U, 93U, 92U
        };
        static const uint8_t expected_32x8_row0[32U] = {
            115U, 115U, 115U, 115U, 115U, 115U, 115U, 114U,
            114U, 114U, 114U, 114U, 114U, 113U, 113U, 113U,
            113U, 113U, 112U, 112U, 112U, 112U, 112U, 111U,
            111U, 111U, 111U, 111U, 111U, 111U, 111U, 111U
        };
        static const uint8_t expected_32x8_row1[32U] = {
            114U, 114U, 114U, 114U, 114U, 114U, 114U, 114U,
            114U, 113U, 113U, 113U, 113U, 113U, 113U, 113U,
            113U, 113U, 113U, 113U, 113U, 112U, 112U, 112U,
            112U, 112U, 112U, 112U, 112U, 112U, 112U, 112U
        };
        uint8_t pixels[192U * 96U];
        uint8_t reconstructed[48U * 24U];
        Av2hReconstructionPlane plane = {
            pixels, 192U, 192U, 96U, 192U, 96U,
            reconstructed, 48U, sizeof(reconstructed)
        };
        Av2hReconstructionScratch scratch;
        size_t row;
        size_t column;

        av2h_memory_fill(pixels, 0U, sizeof(pixels));
        av2h_memory_fill(reconstructed, 0U, sizeof(reconstructed));
        CHECK(av2h_luma_reconstruct_to_plane(
                  &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                  &block, &plane, &scratch) == AV2H_OK);
        for (row = 0U; row < 32U; ++row) {
            CHECK(pixels[row * 192U + 31U] == expected_left[row]);
        }
        REQUIRE(av2h_tile_decoder_commit_leaf(&decoder, leaf.node) ==
                AV2H_OK);

        av2h_memory_fill(&leaf, 0U, sizeof(leaf));
        av2h_memory_fill(&block, 0U, sizeof(block));
        status = av2h_tile_decoder_next_luma(
            &decoder, &leaf, &block, &has_leaf);
        REQUIRE(status == AV2H_OK);
        CHECK(has_leaf == 1U);
        CHECK(leaf.node.row == 0U);
        CHECK(leaf.node.column == 8U);
        CHECK(leaf.node.block_size == AV2H_BLOCK_32X32);
        CHECK(leaf.mode.mode == AV2H_INTRA_DC);
        CHECK(leaf.mode.enable_ibp == 1U);
        CHECK(leaf.prefix.tx_size == AV2H_TX_32X32);
        CHECK(leaf.prefix.eob == 1U);
        CHECK(block.nonzero_count == 1U);
        CHECK(block.dc_value == -1);
        CHECK(block.dequantized[0U] == -992);
        CHECK(av2h_range_tell(&decoder.range) == 32U);
        REQUIRE(av2h_luma_reconstruct_to_plane(
                    &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                    &block, &plane, &scratch) == AV2H_OK);
        for (row = 0U; row < 32U; ++row) {
            for (column = 0U; column < 32U; ++column) {
                uint8_t prediction = column < 8U ?
                    expected_ibp_edge[row * 8U + column] : 91U;
                CHECK(pixels[row * 192U + 32U + column] ==
                      (uint8_t)(prediction - 8U));
            }
        }
        REQUIRE(av2h_tile_decoder_commit_leaf(&decoder, leaf.node) ==
                AV2H_OK);

        av2h_memory_fill(&leaf, 0U, sizeof(leaf));
        av2h_memory_fill(&block, 0U, sizeof(block));
        status = av2h_tile_decoder_next_luma(
            &decoder, &leaf, &block, &has_leaf);
        REQUIRE(status == AV2H_OK);
        CHECK(has_leaf == 1U);
        CHECK(leaf.node.row == 8U);
        CHECK(leaf.node.column == 0U);
        CHECK(leaf.node.block_size == AV2H_BLOCK_32X8);
        CHECK(leaf.mode.mode == AV2H_INTRA_DC);
        CHECK(leaf.mode.enable_ibp == 1U);
        CHECK(leaf.prefix.tx_size == AV2H_TX_32X8);
        CHECK(leaf.prefix.before_all_zero_bits == 42U);
        CHECK(leaf.prefix.after_all_zero_bits == 42U);
        CHECK(leaf.prefix.after_eob_point_bits == 45U);
        CHECK(leaf.prefix.after_eob_bits == 45U);
        CHECK(leaf.prefix.eob_point == 1U);
        CHECK(leaf.prefix.eob == 1U);
        CHECK(leaf.transform.primary_type == AV2H_TX_DCT_DCT);
        CHECK(leaf.transform.before_primary_bits == 45U);
        CHECK(leaf.transform.after_primary_bits == 45U);
        CHECK(leaf.transform.after_secondary_bits == 45U);
        CHECK(block.width == 32U);
        CHECK(block.height == 8U);
        CHECK(block.coefficient_count == 256U);
        CHECK(block.nonzero_count == 1U);
        CHECK(block.quantized[0U] == -1);
        CHECK(block.dequantized[0U] == -1984);
        CHECK(block.dc_value == -1);
        CHECK(block.cul_level == 9U);
        CHECK(block.before_levels_bits == 45U);
        CHECK(block.after_levels_bits == 46U);
        CHECK(block.after_coefficients_bits == 46U);
        CHECK(av2h_range_tell(&decoder.range) == 46U);
        {
            Av2hLumaReferences references = {
                0, 0U, pixels + 31U * 192U, 32U, 0U, 0U
            };
            REQUIRE(av2h_luma_predict(
                        &leaf.mode, 32U, 8U, &references,
                        scratch.predictor, 32U) == AV2H_OK);
            for (row = 0U; row < 8U; ++row) {
                for (column = 0U; column < 32U; ++column) {
                    uint8_t expected_prediction =
                        row == 0U ? expected_32x8_row0[column] :
                        row == 1U ? expected_32x8_row1[column] : 113U;
                    CHECK(scratch.predictor[row * 32U + column] ==
                          expected_prediction);
                }
            }
        }
        REQUIRE(av2h_luma_reconstruct_to_plane(
                    &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                    &block, &plane, &scratch) == AV2H_OK);
        for (row = 0U; row < 8U; ++row) {
            for (column = 0U; column < 32U; ++column) {
                uint8_t expected_prediction =
                    row == 0U ? expected_32x8_row0[column] :
                    row == 1U ? expected_32x8_row1[column] : 113U;
                CHECK(pixels[(32U + row) * 192U + column] ==
                      (uint8_t)(expected_prediction - 15U));
            }
        }
        REQUIRE(av2h_tile_decoder_commit_leaf(&decoder, leaf.node) ==
                AV2H_OK);

        av2h_memory_fill(&leaf, 0U, sizeof(leaf));
        av2h_memory_fill(&block, 0U, sizeof(block));
        status = av2h_tile_decoder_next_luma(
            &decoder, &leaf, &block, &has_leaf);
        CHECK(status == AV2H_OK);
        CHECK(has_leaf == 1U);
        CHECK(leaf.node.row == 10U);
        CHECK(leaf.node.column == 0U);
        CHECK(leaf.node.block_size == AV2H_BLOCK_16X16);
        CHECK(leaf.mode.before_mode_bits == 47U);
        CHECK(leaf.mode.after_mode_set_bits == 47U);
        CHECK(leaf.mode.after_mode_index_bits == 48U);
        CHECK(leaf.mode.after_fsc_bits == 48U);
        CHECK(leaf.mode.after_mrl_bits == 48U);
        CHECK(leaf.mode.mode == AV2H_INTRA_DC);
        CHECK(leaf.mode.angle_delta == 0);
        CHECK(leaf.mode.mrl_index == 0U);
        CHECK(leaf.mode.enable_ibp == 1U);
        CHECK(leaf.mode.fsc_mode == 0U);
        CHECK(leaf.prefix.tx_size == AV2H_TX_16X16);
        CHECK(leaf.prefix.tx_size_context == 2U);
        CHECK(leaf.prefix.txb_skip_context == 0U);
        CHECK(leaf.prefix.before_all_zero_bits == 48U);
        CHECK(leaf.prefix.after_all_zero_bits == 52U);
        CHECK(leaf.prefix.after_eob_point_bits == 52U);
        CHECK(leaf.prefix.after_eob_extra_bit == 52U);
        CHECK(leaf.prefix.after_eob_bits == 52U);
        CHECK(leaf.prefix.all_zero == 1U);
        CHECK(leaf.prefix.eob == 0U);
        CHECK(leaf.transform.primary_type == AV2H_TX_DCT_DCT);
        CHECK(leaf.transform.primary_signaled == 0U);
        CHECK(leaf.transform.secondary_signaled == 0U);
        CHECK(leaf.transform.before_primary_bits == 52U);
        CHECK(leaf.transform.after_primary_bits == 52U);
        CHECK(leaf.transform.after_secondary_bits == 52U);
        CHECK(block.width == 16U);
        CHECK(block.height == 16U);
        CHECK(block.coefficient_count == 256U);
        CHECK(block.nonzero_count == 0U);
        CHECK(block.before_levels_bits == 52U);
        CHECK(block.after_levels_bits == 52U);
        CHECK(block.after_coefficients_bits == 52U);
        CHECK(av2h_range_tell(&decoder.range) == 52U);
        {
            Av2hLumaReferences references = {
                0, 0U, pixels + 39U * 192U, 16U, 0U, 0U
            };
            REQUIRE(av2h_luma_predict(
                        &leaf.mode, 16U, 16U, &references,
                        scratch.predictor, 16U) == AV2H_OK);
            for (row = 0U; row < 16U; ++row) {
                for (column = 0U; column < 16U; ++column) {
                    CHECK(scratch.predictor[row * 16U + column] == 98U);
                }
            }
        }
        for (row = 0U; row < block.coefficient_count; ++row) {
            CHECK(block.quantized[row] == 0);
            CHECK(block.dequantized[row] == 0);
        }
        REQUIRE(av2h_luma_reconstruct_to_plane(
                    &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                    &block, &plane, &scratch) == AV2H_OK);
        for (row = 0U; row < 16U; ++row) {
            for (column = 0U; column < 16U; ++column) {
                CHECK(pixels[(40U + row) * 192U + column] == 98U);
            }
        }
        REQUIRE(av2h_tile_decoder_commit_leaf(&decoder, leaf.node) ==
                AV2H_OK);

        av2h_memory_fill(&leaf, 0U, sizeof(leaf));
        av2h_memory_fill(&block, 0U, sizeof(block));
        status = av2h_tile_decoder_next_luma(
            &decoder, &leaf, &block, &has_leaf);
        CHECK(status == AV2H_OK);
        CHECK(has_leaf == 1U);
        CHECK(leaf.node.row == 10U);
        CHECK(leaf.node.column == 4U);
        CHECK(leaf.node.block_size == AV2H_BLOCK_16X16);
        CHECK(leaf.mode.before_mode_bits == 52U);
        CHECK(leaf.mode.after_mode_set_bits == 52U);
        CHECK(leaf.mode.after_mode_index_bits == 55U);
        CHECK(leaf.mode.after_fsc_bits == 55U);
        CHECK(leaf.mode.after_mrl_bits == 55U);
        CHECK(leaf.mode.mode == AV2H_INTRA_SMOOTH);
        CHECK(leaf.mode.angle_delta == 0);
        CHECK(leaf.mode.mrl_index == 0U);
        CHECK(leaf.mode.fsc_mode == 0U);
        CHECK(leaf.prefix.tx_size == AV2H_TX_16X16);
        CHECK(leaf.prefix.before_all_zero_bits == 55U);
        CHECK(leaf.prefix.after_all_zero_bits == 55U);
        CHECK(leaf.prefix.all_zero == 0U);
        CHECK(leaf.prefix.eob_point == 3U);
        CHECK(leaf.prefix.eob_offset_bits == 1U);
        CHECK(leaf.prefix.after_eob_point_bits == 58U);
        CHECK(leaf.prefix.after_eob_extra_bit == 59U);
        CHECK(leaf.prefix.after_eob_bits == 59U);
        CHECK(leaf.prefix.eob == 4U);
        CHECK(leaf.transform.primary_type == AV2H_TX_ADST_ADST);
        CHECK(leaf.transform.primary_signaled == 1U);
        CHECK(leaf.transform.primary_symbol == 1U);
        CHECK(leaf.transform.before_primary_bits == 59U);
        CHECK(leaf.transform.after_primary_bits == 61U);
        CHECK(leaf.transform.secondary_signaled == 1U);
        CHECK(leaf.transform.secondary_symbol == 0U);
        CHECK(leaf.transform.after_secondary_bits == 65U);
        CHECK(block.width == 16U);
        CHECK(block.height == 16U);
        CHECK(block.coefficient_count == 256U);
        CHECK(block.nonzero_count == 3U);
        CHECK(block.quantized[0U] == 2);
        CHECK(block.dequantized[0U] == 3968);
        CHECK(block.quantized[16U] == 1);
        CHECK(block.dequantized[16U] == 1984);
        CHECK(block.quantized[1U] == 0);
        CHECK(block.dequantized[1U] == 0);
        CHECK(block.quantized[32U] == -1);
        CHECK(block.dequantized[32U] == -1984);
        CHECK(block.cul_level == 20U);
        CHECK(block.dc_value == 2);
        CHECK(block.before_levels_bits == 65U);
        CHECK(block.after_levels_bits == 69U);
        CHECK(block.after_coefficients_bits == 72U);
        CHECK(av2h_range_tell(&decoder.range) == 72U);
        REQUIRE(av2h_luma_reconstruct_to_plane(
                    &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                    &block, &plane, &scratch) == AV2H_OK);
        for (row = 0U; row < 256U; ++row) {
            CHECK(scratch.predictor[row] == 98U);
        }
        CHECK(trace_sample_hash(
                  pixels + 40U * 192U + 16U, 192U, 16U, 16U) ==
              0x7959e335U);
        {
            static const uint8_t row1[16U] = {
                98U, 99U, 99U, 99U, 99U, 100U, 100U, 100U,
                100U, 100U, 101U, 101U, 101U, 101U, 101U, 101U
            };
            static const uint8_t row8[16U] = {
                106U, 115U, 123U, 131U, 139U, 146U, 153U, 160U,
                165U, 171U, 175U, 179U, 182U, 185U, 186U, 187U
            };
            for (column = 0U; column < 16U; ++column) {
                CHECK(pixels[41U * 192U + 16U + column] == row1[column]);
                CHECK(pixels[48U * 192U + 16U + column] == row8[column]);
            }
        }
        REQUIRE(av2h_tile_decoder_commit_leaf(&decoder, leaf.node) ==
                AV2H_OK);
        av2h_memory_fill(&leaf, 0U, sizeof(leaf));
        av2h_memory_fill(&block, 0U, sizeof(block));
        status = av2h_tile_decoder_next_luma(
            &decoder, &leaf, &block, &has_leaf);
        CHECK(status == AV2H_OK);
        CHECK(has_leaf == 1U);
        CHECK(leaf.node.row == 14U);
        CHECK(leaf.node.column == 0U);
        CHECK(leaf.node.block_size == AV2H_BLOCK_32X8);
        CHECK(leaf.mode.mode == AV2H_INTRA_DC);
        CHECK(leaf.prefix.eob == 2U);
        CHECK(leaf.transform.primary_type == AV2H_TX_ADST_DCT);
        CHECK(av2h_range_tell(&decoder.range) == 89U);
    }
}

static void check_artifact(const ArtifactExpectation *expected) {
    Av2hObu entries[4U];
    Av2hObuLedger ledger = { entries, 4U, 0U };
    Av2hError error;
    const Av2hObu *sequence_obu;
    const Av2hObu *frame_obu;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hStatus status;
    size_t stream_size = load_artifact(expected->name);

    REQUIRE(stream_size != 0U);
    REQUIRE(av2h_obu_parse(
                (Av2hSpan){ stream, stream_size }, &ledger, &error) ==
            AV2H_OK);
    sequence_obu = find_obu(&ledger, AV2H_OBU_SEQUENCE_HEADER);
    frame_obu = find_obu(&ledger, AV2H_OBU_CLOSED_LOOP_KEY);
    REQUIRE(sequence_obu != 0);
    REQUIRE(frame_obu != 0);
    REQUIRE(av2h_sequence_state_parse(
                (Av2hSpan){ stream, stream_size }, sequence_obu,
                &sequence, &error) == AV2H_OK);

    CHECK(sequence.info.seq_profile_idc == 0U);
    CHECK(sequence.info.bit_depth_idc == 1U);
    CHECK(sequence.info.chroma_format_idc == 0U);
    CHECK(sequence.info.still_picture == 1U);
    CHECK(sequence.superblock_size == 64U);
    CHECK(sequence.enable_ext_partitions == 1U);
    CHECK(sequence.enable_fsc == 1U);
    CHECK(sequence.enable_ist == 1U);
    CHECK(sequence.enable_cfl_intra == 1U);
    CHECK(sequence.enable_mrls == 1U);
    CHECK(sequence.enable_cctx == 1U);
    CHECK(sequence.enable_gdf == expected->enable_gdf);
    CHECK(sequence.enable_cdef == expected->enable_cdef);
    CHECK(sequence.enable_ccso == expected->enable_ccso);

    status = av2h_frame_header_closed_key_parse(
        (Av2hSpan){ stream + frame_obu->payload_offset,
                     frame_obu->payload_size },
        &sequence, &frame);
    CHECK(status == AV2H_OK);
    CHECK(frame.width == expected->width);
    CHECK(frame.height == expected->height);
    CHECK(frame.base_q_index == expected->base_q_index);
    CHECK(frame.allow_intrabc == expected->allow_intrabc);
    CHECK(frame.uncompressed_header_bits == expected->header_bits);
    CHECK(frame.gdf_mode == expected->gdf_mode);
    CHECK(frame.gdf_qp_index == expected->gdf_qp_index);
    CHECK(frame.gdf_scale_index == expected->gdf_scale_index);
    CHECK(frame.ccso_unit_size == 256U);
    if (sequence.enable_gdf) {
        uint16_t block_size =
            sequence.gdf_unit_matches_superblock_size ?
                sequence.superblock_size : 128U;
        uint16_t columns = (uint16_t)(
            ((uint32_t)frame.width + block_size - 1U) / block_size);
        uint16_t rows = (uint16_t)(
            ((uint32_t)frame.height + block_size - 1U) / block_size);
        CHECK(frame.gdf_block_size == block_size);
        CHECK(frame.gdf_block_columns == columns);
        CHECK(frame.gdf_block_rows == rows);
        CHECK(frame.gdf_block_count == (uint32_t)columns * rows);
    }
    if (status == AV2H_OK) {
        check_first_luma(expected, frame_obu, &sequence, &frame);
        if (expected->base_q_index == 208U &&
            expected->width == 192U && expected->height == 96U) {
            check_emerald_q208_full_tile(frame_obu, &sequence, &frame);
        }
    }
}

static const ArtifactCoefficient cuckoo_q160_coefficients[] = {
    { 0U, -6, -1488 }, { 16U, -1, -248 },
    { 17U, 1, 248 }, { 32U, 1, 248 }
};
static const ArtifactCoefficient emerald_q208_coefficients[] = {
    { 0U, -2, -1984 }, { 1U, 2, 1984 }, { 33U, 1, 992 }
};
static const ArtifactCoefficient laarim_q96_coefficients[] = {
    { 0U, 67, 2613 }, { 1U, 6, 234 }, { 2U, -15, -585 },
    { 3U, 3, 117 }, { 5U, 5, 195 }, { 6U, -2, -78 },
    { 7U, 1, 39 }, { 8U, 2, 78 }, { 9U, -2, -78 },
    { 32U, -24, -936 }, { 33U, 5, 195 }, { 34U, -3, -117 },
    { 35U, -7, -273 }, { 36U, 5, 195 }, { 37U, 3, 117 },
    { 38U, -3, -117 }, { 40U, -4, -156 }, { 41U, -2, -78 },
    { 43U, -1, -39 }, { 64U, -40, -1560 }, { 65U, 18, 702 },
    { 66U, -8, -312 }, { 67U, -6, -234 }, { 68U, -1, -39 },
    { 69U, 2, 78 }, { 73U, -2, -78 }, { 96U, 3, 117 },
    { 97U, 4, 156 }, { 98U, -8, -312 }, { 99U, -1, -39 },
    { 100U, 2, 78 }, { 101U, 1, 39 }, { 102U, 4, 156 },
    { 103U, 2, 78 }, { 104U, -1, -39 }, { 105U, -1, -39 },
    { 106U, -1, -39 }, { 128U, 11, 429 }, { 129U, 18, 702 },
    { 130U, 4, 156 }, { 131U, -6, -234 }, { 132U, -7, -273 },
    { 133U, -1, -39 }, { 134U, -1, -39 }, { 135U, 1, 39 },
    { 136U, -1, -39 }, { 160U, 5, 195 }, { 161U, -9, -351 },
    { 162U, 2, 78 }, { 163U, 11, 429 }, { 164U, -4, -156 },
    { 165U, -2, -78 }, { 166U, -1, -39 }, { 192U, 4, 156 },
    { 193U, 3, 117 }, { 194U, 9, 351 }, { 195U, -2, -78 },
    { 196U, 1, 39 }, { 198U, -2, -78 }, { 224U, 1, 39 },
    { 226U, -2, -78 }, { 227U, 4, 156 }, { 228U, 3, 117 },
    { 229U, -2, -78 }, { 230U, -4, -156 }, { 256U, -1, -39 },
    { 258U, 1, 39 }, { 259U, -1, -39 }, { 260U, 3, 117 },
    { 261U, 1, 39 }, { 262U, -2, -78 }, { 288U, 2, 78 },
    { 289U, 4, 156 }, { 290U, -1, -39 }, { 291U, -2, -78 },
    { 292U, 1, 39 }, { 323U, -1, -39 }, { 324U, -1, -39 },
    { 325U, 1, 39 }, { 353U, 2, 78 }, { 355U, -1, -39 },
    { 356U, -1, -39 }
};
static const ArtifactCoefficient louis_q224_coefficients[] = {
    { 0U, 6, 9408 }
};
static const ArtifactCoefficient pastell_q160_coefficients[] = {
    { 0U, -8, -1984 }, { 1U, -3, -744 }, { 2U, -1, -248 },
    { 3U, -1, -248 }, { 16U, -2, -496 }, { 17U, -4, -992 },
    { 18U, -1, -248 }, { 32U, -2, -496 }, { 33U, -1, -248 },
    { 34U, -1, -248 }
};

static const ArtifactDecodedExpectation cuckoo_q160_decoded = {
    9U, 10U, 11U, 11U, 11U,
    AV2H_TX_16X32, 5U, 11U, 13U, 15U,
    AV2H_TX_DCT_DCT, 0U, 20U,
    512U, 4U, -6, 20U, 31U, 35U,
    cuckoo_q160_coefficients,
    sizeof(cuckoo_q160_coefficients) / sizeof(cuckoo_q160_coefficients[0])
};
static const ArtifactDecodedExpectation emerald_q208_decoded = {
    7U, 7U, 8U, 8U, 8U,
    AV2H_TX_32X32, 5U, 8U, 10U, 12U,
    AV2H_TX_DCT_DCT, 0U, 16U,
    1024U, 3U, -2, 16U, 24U, 27U,
    emerald_q208_coefficients,
    sizeof(emerald_q208_coefficients) / sizeof(emerald_q208_coefficients[0])
};
static const ArtifactDecodedExpectation laarim_q96_decoded = {
    13U, 13U, 17U, 17U, 17U,
    AV2H_TX_32X32, 126U, 18U, 22U, 28U,
    AV2H_TX_DCT_DCT, 0U, 28U,
    1024U, 82U, 67, 28U, 314U, 471U,
    laarim_q96_coefficients,
    sizeof(laarim_q96_coefficients) / sizeof(laarim_q96_coefficients[0])
};
static const ArtifactDecodedExpectation louis_q224_decoded = {
    10U, 10U, 11U, 11U, 11U,
    AV2H_TX_32X32, 1U, 11U, 13U, 13U,
    AV2H_TX_DCT_DCT, 0U, 13U,
    1024U, 1U, 6, 13U, 26U, 27U,
    louis_q224_coefficients,
    sizeof(louis_q224_coefficients) / sizeof(louis_q224_coefficients[0])
};
static const ArtifactDecodedExpectation pastell_q160_decoded = {
    13U, 13U, 14U, 14U, 14U,
    AV2H_TX_16X32, 13U, 14U, 16U, 19U,
    AV2H_TX_DCT_FLIPADST, 0U, 26U,
    512U, 10U, -8, 26U, 53U, 65U,
    pastell_q160_coefficients,
    sizeof(pastell_q160_coefficients) / sizeof(pastell_q160_coefficients[0])
};

static void test_artifact_profiles(void) {
    static const ArtifactExpectation expected[] = {
        { "chalk-320x240.obu", 320U, 240U, 128U, 1U, 1U, 1U, 1U,
          219U, 1U, 2U, 3U, AV2H_BLOCK_8X16, 32U,
          AV2H_UNSUPPORTED, 0 },
        { "chimp-240x287-qp192.obu", 240U, 287U, 192U, 0U, 1U, 1U, 1U,
          217U, 1U, 1U, 3U, AV2H_BLOCK_8X16, 15U,
          AV2H_UNSUPPORTED, 0 },
        { "chimp-93x111-qp064.obu", 93U, 111U, 64U, 1U, 0U, 1U, 1U,
          179U, 0U, 0U, 0U, AV2H_BLOCK_8X8, 16U,
          AV2H_UNSUPPORTED, 0 },
        { "chinnathurai-128x128-qp112.obu", 128U, 128U, 112U, 1U,
          1U, 1U, 1U, 134U, 1U, 2U, 0U, AV2H_BLOCK_32X64, 19U,
          AV2H_UNSUPPORTED, 0 },
        { "chinnathurai-333x333-qp224.obu", 333U, 333U, 224U, 0U,
          1U, 0U, 1U, 98U, 1U, 3U, 2U, AV2H_BLOCK_64X64, 9U,
          AV2H_UNSUPPORTED, 0 },
        { "cuckoo-157x105-qp160.obu", 157U, 105U, 160U, 1U, 1U, 1U, 0U,
          69U, 1U, 1U, 3U, AV2H_BLOCK_16X32, 35U,
          AV2H_OK, &cuckoo_q160_decoded },
        { "cuckoo-431x287-qp080.obu", 431U, 287U, 80U, 1U, 0U, 1U, 1U,
          174U, 0U, 0U, 0U, AV2H_BLOCK_64X64, 23U,
          AV2H_UNSUPPORTED, 0 },
        { "emerald-192x96-qp208.obu", 192U, 96U, 208U, 0U, 0U, 0U, 0U,
          36U, 0U, 0U, 0U, AV2H_BLOCK_32X32, 27U,
          AV2H_OK, &emerald_q208_decoded },
        { "emerald-480x240-qp128.obu", 480U, 240U, 128U, 1U, 1U, 1U, 1U,
          344U, 2U, 1U, 2U, AV2H_BLOCK_64X64, 23U,
          AV2H_UNSUPPORTED, 0 },
        { "flycatcher-223x149-qp176.obu", 223U, 149U, 176U, 0U,
          1U, 1U, 1U, 125U, 1U, 0U, 3U, AV2H_BLOCK_32X64, 20U,
          AV2H_UNSUPPORTED, 0 },
        { "flycatcher-397x265-qp048.obu", 397U, 265U, 48U, 1U,
          1U, 1U, 1U, 256U, 2U, 3U, 3U, AV2H_BLOCK_32X32, 25U,
          AV2H_UNSUPPORTED, 0 },
        { "laarim-256x171-qp096.obu", 256U, 171U, 96U, 1U, 1U, 1U, 1U,
          120U, 2U, 1U, 3U, AV2H_BLOCK_32X32, 471U,
          AV2H_OK, &laarim_q96_decoded },
        { "laarim-448x299-qp192.obu", 448U, 299U, 192U, 0U, 1U, 1U, 1U,
          98U, 1U, 2U, 2U, AV2H_BLOCK_64X64, 18U,
          AV2H_UNSUPPORTED, 0 },
        { "louis-301x222-qp144.obu", 301U, 222U, 144U, 1U, 1U, 1U, 1U,
          192U, 1U, 2U, 3U, AV2H_BLOCK_64X16, 26U,
          AV2H_UNSUPPORTED, 0 },
        { "louis-463x342-qp224.obu", 463U, 342U, 224U, 0U, 1U, 1U, 1U,
          131U, 1U, 3U, 3U, AV2H_BLOCK_32X32, 27U,
          AV2H_OK, &louis_q224_decoded },
        { "pastell-320x213-qp064.obu", 320U, 213U, 64U, 1U, 1U, 1U, 1U,
          252U, 2U, 2U, 1U, AV2H_BLOCK_8X8, 25U,
          AV2H_UNSUPPORTED, 0 },
        { "pastell-512x341-qp160.obu", 512U, 341U, 160U, 1U, 1U, 1U, 1U,
          332U, 1U, 2U, 3U, AV2H_BLOCK_16X32, 65U,
          AV2H_OK, &pastell_q160_decoded },
        { "schwaebisch-353x234-qp112.obu", 353U, 234U, 112U, 1U,
          1U, 1U, 1U, 200U, 2U, 1U, 3U, AV2H_BLOCK_32X8, 32U,
          AV2H_UNSUPPORTED, 0 },
        { "schwaebisch-417x277-qp208.obu", 417U, 277U, 208U, 1U,
          1U, 1U, 1U, 111U, 1U, 3U, 1U, AV2H_BLOCK_64X16, 17U,
          AV2H_OK, 0 },
        { "silene-384x307-qp080.obu", 384U, 307U, 80U, 1U, 0U, 1U, 1U,
          131U, 0U, 0U, 0U, AV2H_BLOCK_64X64, 20U,
          AV2H_UNSUPPORTED, 0 },
        { "silene-479x383-qp176.obu", 479U, 383U, 176U, 1U, 1U, 1U, 1U,
          175U, 2U, 1U, 3U, AV2H_BLOCK_64X64, 13U,
          AV2H_UNSUPPORTED, 0 }
    };
    size_t index;
    CHECK(sizeof(expected) / sizeof(expected[0]) == 21U);
    for (index = 0U; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        check_artifact(&expected[index]);
    }
}

int main(void) {
    check_emerald_q208_vector_fixture();
    test_artifact_profiles();
    if (failures != 0) {
        fprintf(stderr, "%d artifact profile checks failed\n", failures);
        return 1;
    }
    return 0;
}

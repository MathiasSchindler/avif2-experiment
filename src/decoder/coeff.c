#include "coeff.h"

#include "internal/coeff_modules.h"
#include "internal/decode_diagnostics.h"
#include "tx_geometry.h"

#include <stddef.h>

#include "coeff_prefix_cdf_all.inc"

const Av2hCoefficientDiagnostics *av2h_coefficient_diagnostics_get(
    const Av2hDecodeDiagnostics *diagnostics) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    return diagnostics != NULL ? &diagnostics->coefficient : NULL;
#else
    (void)diagnostics;
    return NULL;
#endif
}

void av2h_coefficient_diagnostics_set_eob(
    Av2hDecodeDiagnostics *diagnostics, uint32_t range,
    uint8_t tx_size, uint8_t eob_class) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL) {
        diagnostics->coefficient.eob_range = range;
        diagnostics->coefficient.eob_tx = tx_size;
        diagnostics->coefficient.eob_class = eob_class;
    }
#else
    (void)diagnostics;
    (void)range;
    (void)tx_size;
    (void)eob_class;
#endif
}

void av2h_coefficient_diagnostics_set_skip_range(
    Av2hDecodeDiagnostics *diagnostics, uint32_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL) diagnostics->coefficient.skip_range = value;
#else
    (void)diagnostics;
    (void)value;
#endif
}

void av2h_coefficient_diagnostics_set_luma_prefix_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL) {
        diagnostics->coefficient.luma_prefix_stage = value;
    }
#else
    (void)diagnostics;
    (void)value;
#endif
}

Av2hStatus av2h_coeff_cdfs_init_for_q(
    Av2hCoeffCdfs *cdfs,
    Av2hQIndex base_q_index) {
    static const Av2hBinaryCdfSpec txb_skip[4U][5U] = {
        {
            { 25759U, 31U, 0U }, { 24799U, 56U, 0U },
            { 18946U, 62U, 0U }, { 7809U, 62U, 0U },
            { 2800U, 62U, 0U }
        },
        {
            { 30296U, 5U, 0U }, { 31530U, 30U, 0U },
            { 31362U, 25U, 0U }, { 27230U, 62U, 0U },
            { 23062U, 62U, 0U }
        },
        {
            { 28021U, 3U, 0U }, { 30300U, 0U, 0U },
            { 31298U, 25U, 0U }, { 31807U, 56U, 0U },
            { 32278U, 61U, 0U }
        },
        {
            { 21443U, 16U, 0U }, { 27522U, 0U, 0U },
            { 30197U, 6U, 0U }, { 32558U, 56U, 0U },
            { 32683U, 93U, 0U }
        }
    };
    static const uint16_t eob_pt_128[4U][7U] = {
        { 336U, 988U, 2984U, 5382U, 9800U, 16995U, 24488U },
        { 654U, 1068U, 2257U, 5580U, 12362U, 21455U, 26067U },
        { 1672U, 2600U, 5666U, 11986U, 21596U, 28733U, 30851U },
        { 4122U, 6840U, 13278U, 20906U, 28131U, 31450U, 32305U }
    };
    static const uint8_t eob_pt_128_parameter[4U] = { 32U, 32U, 31U, 31U };
    static const uint16_t eob_pt_512[4U][7U] = {
        { 139U, 5954U, 13032U, 16244U, 20068U, 24954U, 25897U },
        { 1311U, 2019U, 3578U, 6876U, 13781U, 23541U, 24922U },
        { 1654U, 2720U, 5317U, 10565U, 19306U, 29790U, 30875U },
        { 3753U, 5634U, 10147U, 17539U, 26473U, 32201U, 32398U }
    };
    static const uint8_t eob_pt_512_parameter[4U] = { 32U, 62U, 32U, 6U };
    static const uint16_t eob_pt_1024[4U][7U] = {
        { 576U, 1953U, 8086U, 14995U, 20006U, 24682U, 25870U },
        { 634U, 1777U, 3873U, 6293U, 12653U, 23825U, 24712U },
        { 2101U, 3304U, 6486U, 12539U, 21781U, 30341U, 30738U },
        { 5838U, 8921U, 14454U, 21162U, 27888U, 32275U, 32424U }
    };
    static const uint8_t eob_pt_1024_parameter[4U] = {
        62U, 57U, 32U, 31U
    };
    static const Av2hBinaryCdfSpec eob_extra[4U] = {
        { 16391U, 0U, 0U }, { 18326U, 0U, 0U },
        { 19673U, 78U, 0U }, { 20364U, 3U, 0U }
    };
    static const uint16_t eob_pt_16[4U][4U] = {
        { 1946U, 3059U, 6834U, 15123U },
        { 1839U, 2995U, 6796U, 15126U },
        { 3946U, 6406U, 13325U, 26004U },
        { 5029U, 12046U, 26017U, 32453U }
    };
    static const uint8_t eob_pt_16_parameter[4U] = {
        6U, 30U, 30U, 31U
    };
    static const uint16_t eob_pt_16_inter[] = {
        1354U, 1787U, 4813U, 9327U
    };
    static const uint16_t eob_pt_32[4U][5U] = {
        { 1203U, 1920U, 4502U, 10281U, 17078U },
        { 1143U, 1778U, 3854U, 8166U, 13337U },
        { 3277U, 4780U, 9255U, 16544U, 21853U },
        { 7189U, 11732U, 21773U, 29775U, 32060U }
    };
    static const uint8_t eob_pt_32_parameter[4U] = {
        6U, 31U, 31U, 31U
    };
    static const uint16_t eob_pt_64[4U][6U] = {
        { 1848U, 2357U, 4392U, 7629U, 13328U, 21530U },
        { 939U, 1381U, 3142U, 6536U, 13617U, 21799U },
        { 2024U, 3239U, 6969U, 13073U, 21617U, 27643U },
        { 3765U, 6333U, 13340U, 20831U, 28830U, 32289U }
    };
    static const uint8_t eob_pt_64_parameter[4U] = {
        32U, 31U, 31U, 31U
    };
    static const uint16_t eob_pt_256[4U][7U] = {
        { 321U, 2376U, 5427U, 7836U, 12726U, 18552U, 21084U },
        { 790U, 1184U, 2218U, 5022U, 11432U, 20289U, 23689U },
        { 1556U, 2520U, 5142U, 10585U, 19507U, 28407U, 30571U },
        { 3574U, 5683U, 11604U, 19611U, 27344U, 30852U, 31552U }
    };
    static const uint8_t eob_pt_256_parameter[4U] = {
        32U, 57U, 32U, 31U
    };
    uint8_t q_context;
    Av2hStatus status;
    if (cdfs == NULL) return AV2H_INVALID_ARGUMENT;
    q_context = av2h_coeff_q_context(base_q_index);
    cdfs->q_context = q_context;
    av2h_memory_copy(cdfs->full_luma_txb_skip,
                     av2h_luma_txb_skip[q_context],
                     sizeof(cdfs->full_luma_txb_skip));
    av2h_memory_copy(cdfs->full_v_txb_skip,
                     av2h_v_txb_skip[q_context],
                     sizeof(cdfs->full_v_txb_skip));
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs0, txb_skip[q_context][0U]);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_fsc_txs0_ctx9,
        (Av2hBinaryCdfSpec){ 32128U, 78U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_fsc_txs1_ctx9,
        (Av2hBinaryCdfSpec){ 31738U, 75U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_fsc_txs2_ctx9,
        (Av2hBinaryCdfSpec){ 29599U, 7U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs1, txb_skip[q_context][1U]);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs2, txb_skip[q_context][2U]);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs3, txb_skip[q_context][3U]);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs4, txb_skip[q_context][4U]);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->eob_pt_16, 5U + 4U, eob_pt_16[q_context],
        5U, eob_pt_16_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_16_inter, 5U + 4U,
                           eob_pt_16_inter, 5U, 30U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_32, 6U + 4U,
                           eob_pt_32[q_context], 6U,
                           eob_pt_32_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_64, 7U + 4U,
                           eob_pt_64[q_context],
                           7U, eob_pt_64_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->eob_pt_128, 8U + 4U, eob_pt_128[q_context],
        8U, eob_pt_128_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->eob_pt_256, 8U + 4U, eob_pt_256[q_context],
        8U, eob_pt_256_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->eob_pt_512, 8U + 4U, eob_pt_512[q_context],
        8U, eob_pt_512_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->eob_pt_1024, 8U + 4U, eob_pt_1024[q_context],
        8U, eob_pt_1024_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    av2h_memory_fill(cdfs->chroma_eob_pt, 0U,
                     sizeof(cdfs->chroma_eob_pt));
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_16],
                           5U + 4U, av2h_chroma_eob_pt_16[q_context],
                           5U, av2h_chroma_eob_pt_16[q_context][4U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_32],
                           6U + 4U, av2h_chroma_eob_pt_32[q_context],
                           6U, av2h_chroma_eob_pt_32[q_context][5U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_64],
                           7U + 4U, av2h_chroma_eob_pt_64[q_context],
                           7U, av2h_chroma_eob_pt_64[q_context][6U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_128],
                           8U + 4U, av2h_chroma_eob_pt_128[q_context],
                           8U, av2h_chroma_eob_pt_128[q_context][7U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_256],
                           8U + 4U, av2h_chroma_eob_pt_256[q_context],
                           8U, av2h_chroma_eob_pt_256[q_context][7U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_512],
                           8U + 4U, av2h_chroma_eob_pt_512[q_context],
                           8U, av2h_chroma_eob_pt_512[q_context][7U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_1024],
                           8U + 4U, av2h_chroma_eob_pt_1024[q_context],
                           8U, av2h_chroma_eob_pt_1024[q_context][7U], 0U);
    if (status != AV2H_OK) return status;
    {
        const uint16_t *const inter_sources[7U] = {
            av2h_inter_eob_pt_16[q_context],
            av2h_inter_eob_pt_32[q_context],
            av2h_inter_eob_pt_64[q_context],
            av2h_inter_eob_pt_128[q_context],
            av2h_inter_eob_pt_256[q_context],
            av2h_inter_eob_pt_512[q_context],
            av2h_inter_eob_pt_1024[q_context]
        };
        static const uint8_t symbols[7U] = { 5U, 6U, 7U, 8U, 8U, 8U, 8U };
        size_t eob_class;
        av2h_memory_fill(cdfs->inter_eob_pt, 0U,
                         sizeof(cdfs->inter_eob_pt));
        for (eob_class = 0U; eob_class < 7U; ++eob_class) {
            status = av2h_cdf_init(cdfs->inter_eob_pt[eob_class],
                (size_t)symbols[eob_class] + 4U,
                inter_sources[eob_class], symbols[eob_class],
                inter_sources[eob_class][symbols[eob_class] - 1U], 0U);
            if (status != AV2H_OK) return status;
        }
    }
    return av2h_binary_cdf_init(cdfs->eob_extra, eob_extra[q_context]);
}

Av2hStatus av2h_coeff_cdfs_init(Av2hCoeffCdfs *cdfs) {
    return av2h_coeff_cdfs_init_for_q(cdfs, 128U);
}

static Av2hStatus derive_tx_size(Av2hBlockSize block_size,
                                 Av2hTxSize *tx_size,
                                 uint8_t *tx_size_context) {
    const Av2hTxDescriptor *descriptor;
    Av2hStatus status;
    if (tx_size == NULL || tx_size_context == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_luma_tx_descriptor_get(block_size, &descriptor);
    if (status != AV2H_OK) return status;
    switch (descriptor->tx_size) {
        case AV2H_TX_4X4:
        case AV2H_TX_4X8:
        case AV2H_TX_8X4:
        case AV2H_TX_8X8:
        case AV2H_TX_8X16:
        case AV2H_TX_16X8:
        case AV2H_TX_16X16:
        case AV2H_TX_16X32:
        case AV2H_TX_32X16:
        case AV2H_TX_32X32:
        case AV2H_TX_32X64:
        case AV2H_TX_64X32:
        case AV2H_TX_64X64:
        case AV2H_TX_4X16:
        case AV2H_TX_16X4:
        case AV2H_TX_8X32:
        case AV2H_TX_32X8:
        case AV2H_TX_16X64:
        case AV2H_TX_64X16:
        case AV2H_TX_4X32:
        case AV2H_TX_32X4:
        case AV2H_TX_8X64:
        case AV2H_TX_64X8:
        case AV2H_TX_4X64:
        case AV2H_TX_64X4:
            break;
        default:
            return AV2H_UNSUPPORTED;
    }
    *tx_size = descriptor->tx_size;
    *tx_size_context = descriptor->tx_size_context;
    return AV2H_OK;
}

static uint8_t *coefficient_plane(uint8_t *contexts, size_t count,
                                  uint8_t plane) {
    return contexts + (size_t)plane * count;
}

Av2hStatus av2h_coeff_context_state_init(
    Av2hCoeffContextState *state,
    uint8_t *above,
    size_t above_count,
    uint8_t *left,
    size_t left_count) {
    size_t index;
    if (state == NULL || above == NULL || left == NULL || above_count == 0U ||
        left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    state->above = above;
    state->left = left;
    state->above_count = above_count;
    state->left_count = left_count;
    state->mi_columns = above_count;
    state->mi_rows = left_count;
    for (index = 0U; index < AV2H_FRAME_PLANE_COUNT * above_count; ++index) {
        above[index] = 0U;
    }
    for (index = 0U; index < AV2H_FRAME_PLANE_COUNT * left_count; ++index) {
        left[index] = 0U;
    }
    return AV2H_OK;
}

Av2hStatus av2h_coeff_contexts_from_state(
    const Av2hCoeffContextState *state,
    uint8_t plane,
    const Av2hPartitionNode *leaf,
    uint8_t fsc_mode,
    uint8_t *txb_skip_context,
    uint8_t *dc_sign_context) {
    const uint8_t *above;
    const uint8_t *left;
    const uint8_t *u_above;
    const uint8_t *u_left;
    size_t width;
    size_t height;
    size_t left_index;
    size_t index;
    int dc_sign = 0;
    unsigned int above_nonzero = 0U;
    unsigned int left_nonzero = 0U;
    Av2hStatus status;
    if (state == NULL || state->above == NULL || state->left == NULL ||
        state->above_count == 0U || state->left_count == 0U || leaf == NULL ||
        plane >= AV2H_FRAME_PLANE_COUNT || fsc_mode > 1U ||
        txb_skip_context == NULL || dc_sign_context == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_block_dimensions_mi(leaf->block_size, &width, &height);
    if (status != AV2H_OK) return status;
    left_index = leaf->row % state->left_count;
    if (leaf->column >= state->above_count ||
        leaf->column >= state->mi_columns || leaf->row >= state->mi_rows ||
        left_index >= state->left_count) {
        return AV2H_OVERFLOW;
    }
    if (width > state->above_count - leaf->column) {
        width = state->above_count - leaf->column;
    }
    if (height > state->left_count - left_index) {
        height = state->left_count - left_index;
    }
    above = coefficient_plane(state->above, state->above_count, plane);
    left = coefficient_plane(state->left, state->left_count, plane);
    u_above = coefficient_plane(state->above, state->above_count, 1U);
    u_left = coefficient_plane(state->left, state->left_count, 1U);
    for (index = 0U; index < width; ++index) {
        uint8_t context = above[leaf->column + index];
        above_nonzero |= context & 7U;
        dc_sign += (context & 16U) != 0U ? 1 :
                   (context & 8U) != 0U ? -1 : 0;
    }
    for (index = 0U; index < height; ++index) {
        uint8_t context = left[left_index + index];
        left_nonzero |= context & 7U;
        dc_sign += (context & 16U) != 0U ? 1 :
                   (context & 8U) != 0U ? -1 : 0;
    }
    if (plane == 0U) {
        *txb_skip_context = fsc_mode ? 9U : 0U;
    } else if (plane == 1U) {
        *txb_skip_context = (uint8_t)(6U + (above_nonzero != 0U) +
                                     (left_nonzero != 0U));
    } else {
        unsigned int u_nonzero =
            (u_above[leaf->column] & 7U) | (u_left[left_index] & 7U);
        unsigned int neighbor_count =
            (above_nonzero != 0U) + (left_nonzero != 0U);
        *txb_skip_context =
            (uint8_t)(neighbor_count + (u_nonzero != 0U ? 6U : 0U));
    }
    *dc_sign_context = dc_sign < 0 ? 1U : dc_sign > 0 ? 2U : 0U;
    return AV2H_OK;
}

Av2hStatus av2h_coeff_contexts_from_state_for_coding_block(
    const Av2hCoeffContextState *state,
    uint8_t plane,
    const Av2hPartitionNode *leaf,
    Av2hBlockSize coding_block_size,
    uint8_t fsc_mode,
    uint8_t *txb_skip_context,
    uint8_t *dc_sign_context) {
    const uint8_t *above;
    const uint8_t *left;
    size_t width;
    size_t height;
    size_t left_index;
    size_t index;
    int dc_sign = 0;
    unsigned int above_nonzero = 0U;
    unsigned int left_nonzero = 0U;
    size_t tx_width;
    size_t tx_height;
    size_t coding_width;
    size_t coding_height;
    Av2hStatus status;
    if (state == NULL || state->above == NULL || state->left == NULL ||
        state->above_count == 0U || state->left_count == 0U || leaf == NULL ||
        plane >= AV2H_FRAME_PLANE_COUNT ||
        coding_block_size >= AV2H_BLOCK_SIZE_COUNT || fsc_mode > 1U ||
        txb_skip_context == NULL || dc_sign_context == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_block_dimensions_mi(leaf->block_size, &width, &height);
    if (status != AV2H_OK) return status;
    tx_width = width;
    tx_height = height;
    status = av2h_block_dimensions_mi(
        coding_block_size, &coding_width, &coding_height);
    if (status != AV2H_OK) return status;
    left_index = leaf->row % state->left_count;
    if (leaf->column >= state->above_count ||
        leaf->column >= state->mi_columns || leaf->row >= state->mi_rows ||
        left_index >= state->left_count) {
        return AV2H_OVERFLOW;
    }
    if (width > state->above_count - leaf->column) {
        width = state->above_count - leaf->column;
    }
    if (height > state->left_count - left_index) {
        height = state->left_count - left_index;
    }
    above = coefficient_plane(state->above, state->above_count, plane);
    left = coefficient_plane(state->left, state->left_count, plane);
    for (index = 0U; index < width; ++index) {
        uint8_t context = above[leaf->column + index];
        above_nonzero |= context & 7U;
        dc_sign += (context & 16U) != 0U ? 1 :
                   (context & 8U) != 0U ? -1 : 0;
    }
    for (index = 0U; index < height; ++index) {
        uint8_t context = left[left_index + index];
        left_nonzero |= context & 7U;
        dc_sign += (context & 16U) != 0U ? 1 :
                   (context & 8U) != 0U ? -1 : 0;
    }
    if (plane == 0U) {
        static const uint8_t skip_contexts[5U][5U] = {
            { 1U, 2U, 2U, 3U, 3U }, { 2U, 2U, 3U, 3U, 4U },
            { 2U, 3U, 3U, 4U, 4U }, { 3U, 3U, 4U, 4U, 5U },
            { 3U, 4U, 4U, 5U, 5U }
        };
        if (fsc_mode) {
            *txb_skip_context = 9U;
        } else if (coding_width == width && coding_height == height) {
            *txb_skip_context = 0U;
        } else {
            unsigned int top = above_nonzero > 4U ? 4U : above_nonzero;
            unsigned int left = left_nonzero > 4U ? 4U : left_nonzero;
            *txb_skip_context = skip_contexts[top][left];
        }
    } else if (plane == 1U) {
        *txb_skip_context = (uint8_t)(6U + (above_nonzero != 0U) +
                                     (left_nonzero != 0U));
    } else {
        unsigned int neighbor_count =
            (above_nonzero != 0U) + (left_nonzero != 0U);
        unsigned int coding_offset =
            coding_width * coding_height > tx_width * tx_height ? 3U : 0U;
        *txb_skip_context = (uint8_t)(neighbor_count + coding_offset);
    }
    *dc_sign_context = dc_sign < 0 ? 1U : dc_sign > 0 ? 2U : 0U;
    return AV2H_OK;
}

Av2hStatus av2h_coeff_context_state_update(
    Av2hCoeffContextState *state,
    uint8_t plane,
    const Av2hPartitionNode *leaf,
    uint8_t cul_level) {
    uint8_t *above;
    uint8_t *left;
    size_t width;
    size_t height;
    size_t left_index;
    size_t index;
    Av2hStatus status;
    if (state == NULL || state->above == NULL || state->left == NULL ||
        state->above_count == 0U || state->left_count == 0U || leaf == NULL ||
        plane >= AV2H_FRAME_PLANE_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_block_dimensions_mi(leaf->block_size, &width, &height);
    if (status != AV2H_OK) return status;
    left_index = leaf->row % state->left_count;
    if (leaf->column >= state->above_count ||
        leaf->column >= state->mi_columns || leaf->row >= state->mi_rows ||
        left_index >= state->left_count) {
        return AV2H_OVERFLOW;
    }
    if (width > state->above_count - leaf->column) {
        width = state->above_count - leaf->column;
    }
    if (height > state->left_count - left_index) {
        height = state->left_count - left_index;
    }
    above = coefficient_plane(state->above, state->above_count, plane);
    left = coefficient_plane(state->left, state->left_count, plane);
    for (index = 0U; index < width; ++index) {
        above[leaf->column + index] = cul_level;
    }
    for (index = 0U; index < height; ++index) {
        left[left_index + index] = cul_level;
    }
    return AV2H_OK;
}

static Av2hStatus eob_parameters(
    Av2hCoeffCdfs *cdfs,
    Av2hTxSize tx_size,
    uint16_t **point_cdf,
    size_t *point_symbols,
    uint16_t *capacity,
    Av2hTxEobClass *eob_class) {
    const Av2hTxDescriptor *descriptor;
    Av2hStatus status;
    if (cdfs == NULL || point_cdf == NULL || point_symbols == NULL ||
        capacity == NULL || eob_class == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    if (descriptor->eob_class == AV2H_TX_EOB_16) {
        *point_cdf = cdfs->eob_pt_16;
        *point_symbols = 5U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_32) {
        *point_cdf = cdfs->eob_pt_32;
        *point_symbols = 6U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_64) {
        *point_cdf = cdfs->eob_pt_64;
        *point_symbols = 7U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_128) {
        *point_cdf = cdfs->eob_pt_128;
        *point_symbols = 8U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_256) {
        *point_cdf = cdfs->eob_pt_256;
        *point_symbols = 8U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_512) {
        *point_cdf = cdfs->eob_pt_512;
        *point_symbols = 8U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_1024) {
        *point_cdf = cdfs->eob_pt_1024;
        *point_symbols = 8U;
    } else {
        return AV2H_UNSUPPORTED;
    }
    *capacity = descriptor->eob_capacity;
    *eob_class = descriptor->eob_class;
    return AV2H_OK;
}

static Av2hStatus decode_eob(Av2hRangeDecoder *decoder,
                             Av2hCoeffCdfs *cdfs,
                             uint16_t *point_override,
                             size_t point_override_symbols,
                             Av2hCoeffPrefix *prefix) {
    static const uint16_t group_start[] = {
        0U, 1U, 2U, 3U, 5U, 9U, 17U, 33U, 65U, 129U, 257U, 513U
    };
    static const uint8_t offset_bits[] = {
        0U, 0U, 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U
    };
    uint16_t *point_cdf;
    uint16_t capacity;
    size_t point_symbols;
    uint32_t value;
    Av2hTxEobClass eob_class;
    Av2hStatus status = eob_parameters(cdfs, prefix->tx_size, &point_cdf,
                                       &point_symbols, &capacity, &eob_class);
    if (status != AV2H_OK) return status;
    if (point_override != NULL) {
        point_cdf = point_override;
        point_symbols = point_override_symbols;
    }
    av2h_coefficient_diagnostics_set_eob(
        decoder->diagnostics, decoder->range,
        (uint8_t)prefix->tx_size, (uint8_t)eob_class);
    value = av2h_range_read_adaptive(decoder, point_cdf, point_symbols);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    if (eob_class == AV2H_TX_EOB_256 && value == 7U) {
        value += av2h_range_read_bypass(decoder, 1U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    if ((eob_class == AV2H_TX_EOB_512 ||
         eob_class == AV2H_TX_EOB_1024) && value == 7U) {
        value += av2h_range_read_bypass(decoder, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    prefix->eob_point = (uint8_t)(value + 1U);
    prefix->after_eob_point_bits = av2h_range_tell(decoder);
    if (prefix->eob_point >= sizeof(offset_bits) / sizeof(offset_bits[0])) {
        return AV2H_UNSUPPORTED;
    }
    prefix->eob_offset_bits = offset_bits[prefix->eob_point];
    prefix->eob_extra = 0U;
    prefix->after_eob_extra_bit = prefix->after_eob_point_bits;
    if (prefix->eob_offset_bits != 0U) {
        value = av2h_range_read_adaptive(decoder, cdfs->eob_extra, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        prefix->eob_extra =
            (uint16_t)(value << (prefix->eob_offset_bits - 1U));
        prefix->after_eob_extra_bit = av2h_range_tell(decoder);
        if (prefix->eob_offset_bits > 1U) {
            value = av2h_range_read_bypass(
                decoder, (unsigned int)(prefix->eob_offset_bits - 1U));
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            prefix->eob_extra = (uint16_t)(prefix->eob_extra + value);
        }
    }
    prefix->eob =
        (uint16_t)(group_start[prefix->eob_point] + prefix->eob_extra);
    prefix->after_eob_bits = av2h_range_tell(decoder);
    if (prefix->eob == 0U || prefix->eob > capacity) {
        return AV2H_UNSUPPORTED;
    }
    return AV2H_OK;
}

Av2hStatus av2h_coeff_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLeafMode *mode,
    uint8_t txb_skip_context,
    Av2hCoeffPrefix *prefix) {
    uint16_t *cdf;
    uint32_t value;
    Av2hStatus status;
    av2h_coefficient_diagnostics_set_luma_prefix_stage(
        decoder != NULL ? decoder->diagnostics : NULL, 2U);
    if (decoder == NULL || cdfs == NULL || frame == NULL || leaf == NULL ||
        mode == NULL || prefix == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (cdfs->q_context != av2h_coeff_q_context(frame->base_q_index)) {
        return AV2H_UNSUPPORTED;
    }
    av2h_coefficient_diagnostics_set_luma_prefix_stage(
        decoder->diagnostics, 3U);
    status = derive_tx_size(leaf->block_size, &prefix->tx_size,
                            &prefix->tx_size_context);
    if (status != AV2H_OK) return status;
    av2h_coefficient_diagnostics_set_luma_prefix_stage(
        decoder->diagnostics, 4U);
    prefix->txb_skip_context = txb_skip_context;
    if ((mode->fsc_mode ? txb_skip_context != 9U :
         txb_skip_context >= 10U) ||
        (frame->tx_mode != 1U && frame->tx_mode != 2U) ||
        frame->disable_cdf_update) {
        return AV2H_UNSUPPORTED;
    }
    av2h_coefficient_diagnostics_set_luma_prefix_stage(
        decoder->diagnostics, 5U);
    av2h_coefficient_diagnostics_set_luma_prefix_stage(
        decoder->diagnostics, 6U);
    prefix->before_all_zero_bits = av2h_range_tell(decoder);
    cdf = cdfs->full_luma_txb_skip[
        mode->use_intrabc || mode->fsc_mode][prefix->tx_size_context]
                                             [txb_skip_context];
    av2h_coefficient_diagnostics_set_skip_range(
        decoder->diagnostics, decoder->range);
    av2h_coefficient_diagnostics_set_luma_prefix_stage(
        decoder->diagnostics, 7U);
    value = av2h_range_read_adaptive(decoder, cdf, 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->all_zero = (uint8_t)value;
    prefix->after_all_zero_bits = av2h_range_tell(decoder);
    av2h_coefficient_diagnostics_set_luma_prefix_stage(
        decoder->diagnostics, 8U);
    if (value != 0U) {
        prefix->eob_point = 0U;
        prefix->eob_offset_bits = 0U;
        prefix->eob_extra = 0U;
        prefix->eob = 0U;
        prefix->after_eob_point_bits = prefix->after_all_zero_bits;
        prefix->after_eob_extra_bit = prefix->after_all_zero_bits;
        prefix->after_eob_bits = prefix->after_all_zero_bits;
        return AV2H_OK;
    }
    if (mode->use_intrabc) {
        static const uint8_t symbols[7U] = {
            5U, 6U, 7U, 8U, 8U, 8U, 8U
        };
        const Av2hTxDescriptor *descriptor;
        av2h_coefficient_diagnostics_set_luma_prefix_stage(
            decoder->diagnostics, 9U);
        status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
        if (status != AV2H_OK || descriptor->eob_class >= 7U) {
            return AV2H_UNSUPPORTED;
        }
        av2h_coefficient_diagnostics_set_luma_prefix_stage(
            decoder->diagnostics, 10U);
        return decode_eob(decoder, cdfs,
            cdfs->inter_eob_pt[descriptor->eob_class],
            symbols[descriptor->eob_class], prefix);
    }
    return decode_eob(decoder, cdfs, NULL, 0U, prefix);
}

Av2hStatus av2h_first_coeff_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLeafMode *mode,
    Av2hCoeffPrefix *prefix) {
    if (leaf == NULL) return AV2H_INVALID_ARGUMENT;
    if (leaf->row != 0U || leaf->column != 0U) return AV2H_UNSUPPORTED;
    return av2h_coeff_prefix_read(decoder, cdfs, frame, leaf, mode, 0U,
                                  prefix);
}

Av2hStatus av2h_coeff_prefix_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLeafMode *mode,
    const Av2hCoeffContextState *state,
    Av2hCoeffPrefix *prefix,
    uint8_t *dc_sign_context) {
    uint8_t txb_skip_context;
    Av2hStatus status = av2h_coeff_contexts_from_state(
        state, 0U, leaf, mode != NULL ? mode->fsc_mode : 0U,
        &txb_skip_context, dc_sign_context);
    av2h_coefficient_diagnostics_set_luma_prefix_stage(
        decoder != NULL ? decoder->diagnostics : NULL, 1U);
    if (status != AV2H_OK) return status;
    return av2h_coeff_prefix_read(decoder, cdfs, frame, leaf, mode,
                                  txb_skip_context, prefix);
}

Av2hStatus av2h_chroma_coeff_prefix_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hBlockSize coding_block_size,
    uint8_t plane,
    uint16_t u_eob,
    const Av2hCoeffContextState *state,
    Av2hCoeffPrefix *prefix) {
    uint8_t txb_skip_context;
    uint8_t dc_sign_context;
    Av2hStatus status = av2h_coeff_contexts_from_state_for_coding_block(
        state, plane, leaf, coding_block_size, 0U,
        &txb_skip_context, &dc_sign_context);
    if (status != AV2H_OK) return status;
    if (plane == 2U && u_eob != 0U) {
        txb_skip_context = (uint8_t)(txb_skip_context + 6U);
    }
    return av2h_chroma_coeff_prefix_read(
        decoder, cdfs, frame, leaf, plane, txb_skip_context, prefix);
}

Av2hStatus av2h_chroma_coeff_prefix_read_default_context(
    Av2hRangeDecoder *decoder,
    Av2hCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    uint8_t plane,
    Av2hCoeffPrefix *prefix) {
    return av2h_chroma_coeff_prefix_read(
        decoder, cdfs, frame, leaf, plane, plane == 1U ? 6U : 9U, prefix);
}

Av2hStatus av2h_chroma_coeff_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    uint8_t plane,
    uint8_t txb_skip_context,
    Av2hCoeffPrefix *prefix) {
    const Av2hTxDescriptor *descriptor;
    uint16_t *eob_cdf = NULL;
    size_t eob_symbols = 0U;
    uint16_t *skip_cdf;
    uint32_t value;
    Av2hStatus status;
    if (decoder == NULL || cdfs == NULL || frame == NULL || leaf == NULL || prefix == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((plane != 1U && plane != 2U) ||
        (frame->tx_mode != 1U && frame->tx_mode != 2U) ||
        frame->disable_cdf_update ||
        cdfs->q_context != av2h_coeff_q_context(frame->base_q_index)) {
        return AV2H_UNSUPPORTED;
    }
    status = av2h_chroma_tx_descriptor_get(
        leaf->block_size, frame->chroma_format_idc, &descriptor);
    if (status == AV2H_OK && descriptor->tx_size_context < 5U &&
        ((plane == 1U && txb_skip_context < 10U) ||
         (plane == 2U && txb_skip_context < 12U))) {
        static const uint8_t chroma_eob_symbols[7U] = {
            5U, 6U, 7U, 8U, 8U, 8U, 8U
        };
        prefix->tx_size = descriptor->tx_size;
        prefix->tx_size_context = descriptor->tx_size_context;
        skip_cdf = plane == 1U ?
            cdfs->full_luma_txb_skip[0U][
                prefix->tx_size_context][txb_skip_context] :
            cdfs->full_v_txb_skip[txb_skip_context];
        eob_cdf = cdfs->chroma_eob_pt[descriptor->eob_class];
        eob_symbols = chroma_eob_symbols[descriptor->eob_class];
    } else {
        return AV2H_UNSUPPORTED;
    }
    prefix->txb_skip_context = txb_skip_context;
    prefix->before_all_zero_bits = av2h_range_tell(decoder);
    value = av2h_range_read_adaptive(decoder, skip_cdf, 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->all_zero = (uint8_t)value;
    prefix->after_all_zero_bits = av2h_range_tell(decoder);
    if (value != 0U) {
        prefix->eob_point = 0U;
        prefix->eob_offset_bits = 0U;
        prefix->eob_extra = 0U;
        prefix->eob = 0U;
        prefix->after_eob_point_bits = prefix->after_all_zero_bits;
        prefix->after_eob_extra_bit = prefix->after_all_zero_bits;
        prefix->after_eob_bits = prefix->after_all_zero_bits;
        return AV2H_OK;
    }
    if (eob_cdf == NULL || eob_symbols == 0U) {
        return decode_eob(decoder, cdfs, NULL, 0U, prefix);
    }
    return decode_eob(
        decoder, cdfs, eob_cdf, eob_symbols, prefix);
}
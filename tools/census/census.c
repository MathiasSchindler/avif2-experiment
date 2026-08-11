#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/avm_config.h"

#include "avm/avm_decoder.h"
#include "avm/avmdx.h"
#include "av2/common/av2_common_int.h"
#include "av2/common/blockd.h"
#include "av2/common/common_data.h"
#include "av2/decoder/decoder.h"
#include "av2/decoder/inspection.h"

#if !CONFIG_INSPECTION
#error "av2-census requires AVM built with CONFIG_INSPECTION=1"
#endif

#define AVM_PINNED_REVISION "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d"

typedef struct {
    int block_size;
    int partition;
    int tree_type;
    int mode;
    int uv_mode;
    int angle_delta;
    int uv_angle_delta;
    int mrl_index;
    int multi_line_mrl;
    int fsc_y;
    int fsc_uv;
    int enable_ibp;
    int use_intrabc;
    int intrabc_mode;
    int skip_transform;
    int tx_size;
    int primary_tx_type;
    int secondary_tx_type;
    int secondary_tx_set;
    int current_qindex;
    int cfl_index;
    int cfl_alpha_index;
    int cfl_alpha_signs;
    int y_mode_index;
    int joint_mode;
    int palette_y;
    int palette_uv;
} FeatureKey;

typedef struct {
    FeatureKey key;
    uint64_t count;
    int first_frame;
    int first_mi_row;
    int first_mi_column;
} FeatureEntry;

typedef struct {
    int mi_row;
    int mi_column;
    int chroma_mi_row;
    int chroma_mi_column;
    int luma_block_size;
    int chroma_block_size;
    int partition;
    int tree_type;
    int region_type;
    int use_intrabc;
    int intrabc_drl_index;
    int mv_row;
    int mv_column;
    int reference_mv_row;
    int reference_mv_column;
    int mv_precision;
} BlockTraceEntry;

#if CONFIG_ACCOUNTING
typedef struct {
    AccountingSymbolInfo info;
    uint64_t count;
    uint64_t nonzero_count;
    uint64_t tree_counts[3U];
    int minimum_value;
    int maximum_value;
    AccountingSymbolContext first_context;
    int has_first_context;
} AccountingSummary;

typedef struct {
    AccountingSymbolContext context;
    uint32_t id;
    uint64_t bits;
    int value;
    int coding_mode;
    int coding_type;
} AccountingTraceEntry;
#endif

typedef struct {
    insp_frame_data frame_data;
#if CONFIG_ACCOUNTING
    AccountingSummary accounting[MAX_SYMBOL_TYPES];
    int accounting_count;
    AccountingTraceEntry *accounting_trace;
    size_t accounting_trace_count;
    size_t accounting_trace_capacity;
    int trace_accounting;
#endif
    FeatureEntry *features;
    size_t feature_count;
    size_t feature_capacity;
    BlockTraceEntry *block_trace;
    size_t block_trace_count;
    size_t block_trace_capacity;
    int trace_blocks;
    uint64_t block_count;
    uint64_t feature_occurrence_count;
    int frame_count;
    int width;
    int height;
    int bit_depth;
    int base_qindex;
    int superblock_size;
    uint32_t frame_header_bits;
    uint32_t tile_entropy_tell;
    int deblock_enabled[4];
    int cdef_enabled;
    int cdef_strength_count;
    int cdef_y_strength;
    int cdef_uv_strength;
    int ccso_frame_enabled;
    int ccso_enabled[MAX_MB_PLANE];
    int gdf_mode;
    int failed;
} Census;

static int census_trace_block(Census *census, const MB_MODE_INFO *mode) {
    BlockTraceEntry *entry;
    if (!census->trace_blocks) return 1;
    if (census->block_trace_count == census->block_trace_capacity) {
        size_t capacity = census->block_trace_capacity == 0U ?
            256U : census->block_trace_capacity * 2U;
        BlockTraceEntry *grown = (BlockTraceEntry *)realloc(
            census->block_trace, capacity * sizeof(*grown));
        if (grown == NULL) return 0;
        census->block_trace = grown;
        census->block_trace_capacity = capacity;
    }
    entry = &census->block_trace[census->block_trace_count++];
    entry->mi_row = mode->mi_row_start;
    entry->mi_column = mode->mi_col_start;
    entry->chroma_mi_row = mode->chroma_mi_row_start;
    entry->chroma_mi_column = mode->chroma_mi_col_start;
    entry->luma_block_size = mode->sb_type[PLANE_TYPE_Y];
    entry->chroma_block_size = mode->sb_type[PLANE_TYPE_UV];
    entry->partition = mode->partition;
    entry->tree_type = mode->tree_type;
    entry->region_type = mode->region_type;
    entry->use_intrabc = mode->use_intrabc[0];
    entry->intrabc_drl_index = mode->intrabc_drl_idx;
    entry->mv_row = mode->mv[0].as_mv.row;
    entry->mv_column = mode->mv[0].as_mv.col;
    entry->reference_mv_row = mode->ref_bv.as_mv.row;
    entry->reference_mv_column = mode->ref_bv.as_mv.col;
    entry->mv_precision = mode->pb_mv_precision;
    return 1;
}

#if CONFIG_ACCOUNTING
static int census_trace_accounting(Census *census,
                                   const AccountingSymbol *symbol) {
    AccountingTraceEntry *entry;
    if (!census->trace_accounting) return 1;
    if (census->accounting_trace_count == census->accounting_trace_capacity) {
        size_t capacity = census->accounting_trace_capacity == 0U ?
            1024U : census->accounting_trace_capacity * 2U;
        AccountingTraceEntry *grown = (AccountingTraceEntry *)realloc(
            census->accounting_trace, capacity * sizeof(*grown));
        if (grown == NULL) return 0;
        census->accounting_trace = grown;
        census->accounting_trace_capacity = capacity;
    }
    entry = &census->accounting_trace[census->accounting_trace_count++];
    entry->context = symbol->context;
    entry->id = symbol->id;
    entry->bits = symbol->bits;
    entry->value = symbol->value;
    entry->coding_mode = symbol->coding_mode;
    entry->coding_type = symbol->coding_type;
    return 1;
}
#endif

static int feature_key_equal(const FeatureKey *left,
                             const FeatureKey *right) {
#define FIELD_EQUAL(field) (left->field == right->field)
    return FIELD_EQUAL(block_size) && FIELD_EQUAL(partition) &&
           FIELD_EQUAL(tree_type) && FIELD_EQUAL(mode) &&
           FIELD_EQUAL(uv_mode) && FIELD_EQUAL(angle_delta) &&
           FIELD_EQUAL(uv_angle_delta) && FIELD_EQUAL(mrl_index) &&
           FIELD_EQUAL(multi_line_mrl) && FIELD_EQUAL(fsc_y) &&
           FIELD_EQUAL(fsc_uv) && FIELD_EQUAL(enable_ibp) &&
           FIELD_EQUAL(use_intrabc) && FIELD_EQUAL(intrabc_mode) &&
           FIELD_EQUAL(skip_transform) && FIELD_EQUAL(tx_size) &&
           FIELD_EQUAL(primary_tx_type) && FIELD_EQUAL(secondary_tx_type) &&
           FIELD_EQUAL(secondary_tx_set) && FIELD_EQUAL(current_qindex) &&
           FIELD_EQUAL(cfl_index) && FIELD_EQUAL(cfl_alpha_index) &&
           FIELD_EQUAL(cfl_alpha_signs) && FIELD_EQUAL(y_mode_index) &&
           FIELD_EQUAL(joint_mode) && FIELD_EQUAL(palette_y) &&
           FIELD_EQUAL(palette_uv);
#undef FIELD_EQUAL
}

static int census_add(Census *census, const FeatureKey *key, int frame,
                      int mi_row, int mi_column) {
    size_t index;
    for (index = 0U; index < census->feature_count; ++index) {
        if (feature_key_equal(&census->features[index].key, key)) {
            ++census->features[index].count;
            ++census->feature_occurrence_count;
            return 1;
        }
    }
    if (census->feature_count == census->feature_capacity) {
        size_t capacity = census->feature_capacity == 0U ?
            64U : census->feature_capacity * 2U;
        FeatureEntry *features = (FeatureEntry *)realloc(
            census->features, capacity * sizeof(*features));
        if (features == NULL) return 0;
        census->features = features;
        census->feature_capacity = capacity;
    }
    census->features[census->feature_count].key = *key;
    census->features[census->feature_count].count = 1U;
    census->features[census->feature_count].first_frame = frame;
    census->features[census->feature_count].first_mi_row = mi_row;
    census->features[census->feature_count].first_mi_column = mi_column;
    ++census->feature_count;
    ++census->feature_occurrence_count;
    return 1;
}

static FeatureKey make_feature_key(const AV2_COMMON *common,
                                   const MB_MODE_INFO *mode,
                                   TX_TYPE packed_tx_type) {
    FeatureKey key = { 0 };
    key.block_size = mode->sb_type[PLANE_TYPE_Y];
    key.partition = mode->partition;
    key.tree_type = mode->tree_type;
    key.mode = mode->mode;
    key.uv_mode = mode->mode < INTRA_MODES ? mode->uv_mode : UV_MODE_INVALID;
    key.angle_delta = mode->angle_delta[PLANE_TYPE_Y];
    key.uv_angle_delta = mode->angle_delta[PLANE_TYPE_UV];
    key.mrl_index = mode->mrl_index;
    key.multi_line_mrl = mode->multi_line_mrl;
    key.fsc_y = mode->fsc_mode[PLANE_TYPE_Y];
    key.fsc_uv = mode->fsc_mode[PLANE_TYPE_UV];
    key.enable_ibp = common->seq_params.enable_ibp;
    key.use_intrabc = mode->use_intrabc[0];
    key.intrabc_mode = mode->intrabc_mode;
    key.skip_transform = mode->skip_txfm[0];
    key.tx_size = mode->tx_size;
    key.primary_tx_type = get_primary_tx_type(packed_tx_type);
    key.secondary_tx_type = get_secondary_tx_type(packed_tx_type);
    key.secondary_tx_set = get_secondary_tx_set(packed_tx_type);
    key.current_qindex = mode->current_qindex;
    key.cfl_index = mode->cfl_idx;
    key.cfl_alpha_index = mode->cfl_alpha_idx;
    key.cfl_alpha_signs = mode->cfl_alpha_signs;
    key.y_mode_index = mode->y_mode_idx;
    key.joint_mode = mode->joint_y_mode_delta_angle;
    key.palette_y = mode->palette_mode_info.palette_size[0];
    key.palette_uv = mode->palette_mode_info.palette_size[1];
    return key;
}

static void inspect_frame(void *decoder, void *context) {
    Census *census = (Census *)context;
    struct AV2Decoder *pbi = (struct AV2Decoder *)decoder;
    AV2_COMMON *common = &pbi->common;
    const CommonModeInfoParams *mi_params = &common->mi_params;
    int row;
    int column;
    if (!ifd_inspect(&census->frame_data, decoder, 0)) {
        census->failed = 1;
        return;
    }
#if CONFIG_ACCOUNTING
    if (census->frame_data.accounting != NULL) {
        const Accounting *accounting = census->frame_data.accounting;
        int dictionary_index;
        int symbol_index;
        census->accounting_count = accounting->syms.dictionary.num_strs;
        for (dictionary_index = 0;
             dictionary_index < census->accounting_count;
             ++dictionary_index) {
            census->accounting[dictionary_index].info =
                accounting->syms.dictionary.acct_infos[dictionary_index];
        }
        for (symbol_index = 0; symbol_index < accounting->syms.num_syms;
             ++symbol_index) {
            const AccountingSymbol *symbol =
                &accounting->syms.syms[symbol_index];
            AccountingSummary *summary;
            if (!census_trace_accounting(census, symbol)) {
                census->failed = 1;
                return;
            }
            if (symbol->id >= (uint32_t)census->accounting_count) continue;
            summary = &census->accounting[symbol->id];
            if (!summary->has_first_context) {
                summary->first_context = symbol->context;
                summary->minimum_value = symbol->value;
                summary->maximum_value = symbol->value;
                summary->has_first_context = 1;
            }
            if (symbol->value != 0) ++summary->nonzero_count;
            if (symbol->value < summary->minimum_value) {
                summary->minimum_value = symbol->value;
            }
            if (symbol->value > summary->maximum_value) {
                summary->maximum_value = symbol->value;
            }
            if ((unsigned int)symbol->context.tree_type < 3U) {
                ++summary->tree_counts[symbol->context.tree_type];
            }
            ++summary->count;
        }
    }
#endif
    census->width = census->frame_data.width;
    census->height = census->frame_data.height;
    census->bit_depth = census->frame_data.bit_depth;
    census->base_qindex = census->frame_data.base_qindex;
    census->superblock_size = census->frame_data.superblock_size;
    census->frame_header_bits = pbi->uncomp_hdr_size_in_bits;
    if (pbi->tile_data != NULL) {
        census->tile_entropy_tell =
            avm_reader_tell(&pbi->tile_data[0].bit_reader);
    }
    census->deblock_enabled[0] = common->lf.apply_deblocking_filter[0];
    census->deblock_enabled[1] = common->lf.apply_deblocking_filter[1];
    census->deblock_enabled[2] = common->lf.apply_deblocking_filter_u;
    census->deblock_enabled[3] = common->lf.apply_deblocking_filter_v;
    census->cdef_enabled = common->cdef_info.cdef_frame_enable;
    census->cdef_strength_count = common->cdef_info.nb_cdef_strengths;
    if (census->cdef_strength_count > 0) {
        census->cdef_y_strength = common->cdef_info.cdef_strengths[0];
        census->cdef_uv_strength = common->cdef_info.cdef_uv_strengths[0];
    }
    census->ccso_frame_enabled = common->ccso_info.ccso_frame_flag;
    for (int plane = 0; plane < MAX_MB_PLANE; ++plane) {
        census->ccso_enabled[plane] = common->ccso_info.ccso_enable[plane];
    }
    census->gdf_mode = common->gdf_info.gdf_mode;
    ++census->frame_count;
    for (row = 0; row < mi_params->mi_rows; ++row) {
        for (column = 0; column < mi_params->mi_cols; ++column) {
            const MB_MODE_INFO *mode = mi_params->mi_grid_base[
                row * mi_params->mi_stride + column];
            BLOCK_SIZE block_size;
            int block_bottom;
            int block_right;
            int tx_row;
            int tx_column;
            if (mode == NULL || mode->mi_row_start != row ||
                mode->mi_col_start != column) {
                continue;
            }
            if (!census_trace_block(census, mode)) {
                census->failed = 1;
                return;
            }
            ++census->block_count;
            block_size = mode->sb_type[PLANE_TYPE_Y];
            block_bottom = row + mi_size_high[block_size];
            block_right = column + mi_size_wide[block_size];
            if (block_bottom > mi_params->mi_rows) {
                block_bottom = mi_params->mi_rows;
            }
            if (block_right > mi_params->mi_cols) {
                block_right = mi_params->mi_cols;
            }
            if (mode->skip_txfm[0]) {
                FeatureKey key = make_feature_key(
                    common, mode, mi_params->tx_type_map[
                        row * mi_params->mi_stride + column]);
                if (!census_add(census, &key,
                                census->frame_data.frame_number, row,
                                column)) {
                    census->failed = 1;
                    return;
                }
                continue;
            }
            for (tx_row = row; tx_row < block_bottom;
                 tx_row += tx_size_high_unit[mode->tx_size]) {
                for (tx_column = column; tx_column < block_right;
                     tx_column += tx_size_wide_unit[mode->tx_size]) {
                    FeatureKey key = make_feature_key(
                        common, mode, mi_params->tx_type_map[
                            tx_row * mi_params->mi_stride + tx_column]);
                    if (!census_add(census, &key,
                                    census->frame_data.frame_number, tx_row,
                                    tx_column)) {
                        census->failed = 1;
                        return;
                    }
                }
            }
        }
    }
}

static const char *block_size_name(int value) {
    static const char *const names[BLOCK_SIZES_ALL] = {
        "4x4", "4x8", "8x4", "8x8", "8x16", "16x8", "16x16",
        "16x32", "32x16", "32x32", "32x64", "64x32", "64x64",
        "64x128", "128x64", "128x128", "128x256", "256x128",
        "256x256", "4x16", "16x4", "8x32", "32x8", "16x64",
        "64x16", "4x32", "32x4", "8x64", "64x8"
    };
    return value >= 0 && value < BLOCK_SIZES_ALL ? names[value] : "unknown";
}

static const char *tx_size_name(int value) {
    static const char *const names[TX_SIZES_ALL] = {
        "4x4", "8x8", "16x16", "32x32", "64x64", "4x8", "8x4",
        "8x16", "16x8", "16x32", "32x16", "32x64", "64x32",
        "4x16", "16x4", "8x32", "32x8", "16x64", "64x16",
        "4x32", "32x4", "8x64", "64x8", "4x64", "64x4"
    };
    return value >= 0 && value < TX_SIZES_ALL ? names[value] : "unknown";
}

static const char *tx_type_name(int value) {
    static const char *const names[TX_TYPES] = {
        "DCT_DCT", "ADST_DCT", "DCT_ADST", "ADST_ADST",
        "FLIPADST_DCT", "DCT_FLIPADST", "FLIPADST_FLIPADST",
        "ADST_FLIPADST", "FLIPADST_ADST", "IDTX", "V_DCT", "H_DCT",
        "V_ADST", "H_ADST", "V_FLIPADST", "H_FLIPADST"
    };
    return value >= 0 && value < TX_TYPES ? names[value] : "unknown";
}

static const char *mode_name(int value) {
    static const char *const names[MB_MODE_COUNT] = {
        "DC", "VERTICAL", "HORIZONTAL", "D45", "D135", "D113",
        "D157", "D203", "D67", "SMOOTH", "SMOOTH_VERTICAL",
        "SMOOTH_HORIZONTAL", "PAETH", "NEARMV", "GLOBALMV", "NEWMV",
        "WARPMV", "WARP_NEWMV", "NEAR_NEARMV", "NEAR_NEWMV",
        "NEW_NEARMV", "GLOBAL_GLOBALMV", "NEW_NEWMV", "JOINT_NEWMV",
        "NEAR_NEARMV_OPTFLOW", "NEAR_NEWMV_OPTFLOW",
        "NEW_NEARMV_OPTFLOW", "NEW_NEWMV_OPTFLOW", "JOINT_NEWMV_OPTFLOW"
    };
    return value >= 0 && value < MB_MODE_COUNT ? names[value] : "unknown";
}

static const char *uv_mode_name(int value) {
    static const char *const names[UV_INTRA_MODES] = {
        "DC", "VERTICAL", "HORIZONTAL", "D45", "D135", "D113",
        "D157", "D203", "D67", "SMOOTH", "SMOOTH_VERTICAL",
        "SMOOTH_HORIZONTAL", "PAETH", "CFL"
    };
    if (value == UV_MODE_INVALID) return "INVALID";
    return value >= 0 && value < UV_INTRA_MODES ? names[value] : "unknown";
}

static void json_string(FILE *output, const char *value) {
    const unsigned char *cursor = (const unsigned char *)value;
    fputc('"', output);
    while (*cursor != 0U) {
        unsigned char character = *cursor++;
        if (character == '"' || character == '\\') {
            fputc('\\', output);
            fputc(character, output);
        } else if (character == '\b') {
            fputs("\\b", output);
        } else if (character == '\f') {
            fputs("\\f", output);
        } else if (character == '\n') {
            fputs("\\n", output);
        } else if (character == '\r') {
            fputs("\\r", output);
        } else if (character == '\t') {
            fputs("\\t", output);
        } else if (character < 0x20U) {
            fprintf(output, "\\u%04x", character);
        } else {
            fputc(character, output);
        }
    }
    fputc('"', output);
}

static unsigned char *read_entire_file(const char *path, size_t *size) {
    FILE *input = fopen(path, "rb");
    unsigned char *bytes;
    long length;
    if (input == NULL) return NULL;
    if (fseek(input, 0L, SEEK_END) != 0) {
        fclose(input);
        return NULL;
    }
    length = ftell(input);
    if (length <= 0L || fseek(input, 0L, SEEK_SET) != 0) {
        fclose(input);
        return NULL;
    }
    bytes = (unsigned char *)malloc((size_t)length);
    if (bytes == NULL || fread(bytes, 1U, (size_t)length, input) !=
                             (size_t)length) {
        free(bytes);
        fclose(input);
        return NULL;
    }
    fclose(input);
    *size = (size_t)length;
    return bytes;
}

static int decode_file(const char *path, Census *census, size_t *byte_count) {
    avm_codec_ctx_t codec;
    avm_inspect_init inspection = { inspect_frame, NULL, NULL, census };
    unsigned char *bytes;
    size_t size = 0U;
    avm_codec_err_t status;
    int trace_blocks = census->trace_blocks;
#if CONFIG_ACCOUNTING
    int trace_accounting = census->trace_accounting;
#endif
    memset(census, 0, sizeof(*census));
    census->trace_blocks = trace_blocks;
#if CONFIG_ACCOUNTING
    census->trace_accounting = trace_accounting;
#endif
    memset(&codec, 0, sizeof(codec));
    bytes = read_entire_file(path, &size);
    if (bytes == NULL) {
        fprintf(stderr, "av2-census: cannot read %s: %s\n", path,
                strerror(errno));
        return 0;
    }
    status = avm_codec_dec_init(&codec, avm_codec_av2_dx(), NULL, 0);
    if (status != AVM_CODEC_OK) {
        fprintf(stderr, "av2-census: decoder init failed for %s\n", path);
        free(bytes);
        return 0;
    }
    status = avm_codec_control(&codec, AV2_SET_INSPECTION_CALLBACK,
                               &inspection);
    if (status == AVM_CODEC_OK) {
        status = avm_codec_decode(&codec, bytes, size, NULL);
    }
    if (status != AVM_CODEC_OK || census->failed || census->frame_count == 0) {
        const char *detail = avm_codec_error_detail(&codec);
        fprintf(stderr, "av2-census: decode failed for %s: %s%s%s\n", path,
                avm_codec_error(&codec), detail == NULL ? "" : ": ",
                detail == NULL ? "" : detail);
        ifd_clear(&census->frame_data);
        avm_codec_destroy(&codec);
        free(bytes);
        free(census->features);
        free(census->block_trace);
    #if CONFIG_ACCOUNTING
        free(census->accounting_trace);
    #endif
        memset(census, 0, sizeof(*census));
        return 0;
    }
    ifd_clear(&census->frame_data);
    avm_codec_destroy(&codec);
    free(bytes);
    *byte_count = size;
    return 1;
}

static void write_feature(FILE *output, const FeatureEntry *entry) {
    const FeatureKey *key = &entry->key;
    fprintf(output,
            "      {\"count\":%llu,\"first\":{\"frame\":%d,"
            "\"mi_row\":%d,\"mi_column\":%d},",
            (unsigned long long)entry->count, entry->first_frame,
            entry->first_mi_row, entry->first_mi_column);
    fprintf(output, "\"block_size\":{\"id\":%d,\"name\":",
            key->block_size);
    json_string(output, block_size_name(key->block_size));
    fprintf(output, "},\"partition\":%d,\"tree_type\":%d,",
            key->partition, key->tree_type);
    fprintf(output, "\"mode\":{\"id\":%d,\"name\":", key->mode);
    json_string(output, mode_name(key->mode));
    fprintf(output, "},\"uv_mode\":{\"id\":%d,\"name\":", key->uv_mode);
    json_string(output, uv_mode_name(key->uv_mode));
    fprintf(output,
            "},\"angle_delta\":%d,\"uv_angle_delta\":%d,"
            "\"mrl_index\":%d,\"multi_line_mrl\":%d,"
            "\"fsc_y\":%d,\"fsc_uv\":%d,\"enable_ibp\":%d,"
            "\"use_intrabc\":%d,\"intrabc_mode\":%d,"
            "\"skip_transform\":%d,",
            key->angle_delta, key->uv_angle_delta, key->mrl_index,
            key->multi_line_mrl, key->fsc_y, key->fsc_uv, key->enable_ibp,
            key->use_intrabc, key->intrabc_mode, key->skip_transform);
    fprintf(output, "\"tx_size\":{\"id\":%d,\"name\":", key->tx_size);
    json_string(output, tx_size_name(key->tx_size));
    fprintf(output, "},\"primary_tx_type\":{\"id\":%d,\"name\":",
            key->primary_tx_type);
    json_string(output, tx_type_name(key->primary_tx_type));
    fprintf(output,
            "},\"secondary_tx_type\":%d,\"secondary_tx_set\":%d,"
            "\"qindex\":%d,\"cfl_index\":%d,"
            "\"cfl_alpha_index\":%d,\"cfl_alpha_signs\":%d,"
            "\"y_mode_index\":%d,\"joint_mode\":%d,"
            "\"palette_y\":%d,\"palette_uv\":%d}",
            key->secondary_tx_type, key->secondary_tx_set,
            key->current_qindex, key->cfl_index, key->cfl_alpha_index,
            key->cfl_alpha_signs, key->y_mode_index, key->joint_mode,
            key->palette_y, key->palette_uv);
}

static void write_block_trace(FILE *output, const Census *census) {
    size_t index;
    fputs(",\"block_trace\":[", output);
    for (index = 0U; index < census->block_trace_count; ++index) {
        const BlockTraceEntry *entry = &census->block_trace[index];
        if (index != 0U) fputc(',', output);
        fprintf(output,
                "{\"mi_row\":%d,\"mi_column\":%d,"
                "\"chroma_mi_row\":%d,\"chroma_mi_column\":%d,"
                "\"luma_block_size\":%d,\"chroma_block_size\":%d,"
                "\"partition\":%d,\"tree_type\":%d,"
                "\"region_type\":%d,\"use_intrabc\":%d,"
                "\"intrabc_drl_index\":%d,"
                "\"mv_row\":%d,\"mv_column\":%d,"
                "\"reference_mv_row\":%d,\"reference_mv_column\":%d,"
                "\"mv_precision\":%d}",
                entry->mi_row, entry->mi_column,
                entry->chroma_mi_row, entry->chroma_mi_column,
                entry->luma_block_size, entry->chroma_block_size,
                entry->partition, entry->tree_type, entry->region_type,
                entry->use_intrabc, entry->intrabc_drl_index,
                entry->mv_row, entry->mv_column,
                entry->reference_mv_row, entry->reference_mv_column,
                entry->mv_precision);
    }
    fputc(']', output);
}

#if CONFIG_ACCOUNTING
static void write_accounting(FILE *output, const Census *census) {
    int dictionary_index;
    fputs(",\"accounting_symbols\":[", output);
    for (dictionary_index = 0;
         dictionary_index < census->accounting_count;
         ++dictionary_index) {
        const AccountingSummary *summary =
            &census->accounting[dictionary_index];
        const AccountingSymbolInfo *info = &summary->info;
        int tag_index;
        if (dictionary_index != 0) fputc(',', output);
        fputs("{\"function\":", output);
        json_string(output, info->c_func);
        fputs(",\"file\":", output);
        json_string(output, info->c_file);
        fprintf(output, ",\"line\":%d,\"tags\":[", info->c_line);
        for (tag_index = 0; tag_index < AVM_ACCOUNTING_MAX_TAGS &&
                            info->tags[tag_index] != NULL;
             ++tag_index) {
            if (tag_index != 0) fputc(',', output);
            json_string(output, info->tags[tag_index]);
        }
        fprintf(output,
            "],\"count\":%llu,\"nonzero_count\":%llu,"
                "\"tree_counts\":{\"shared\":%llu,\"luma\":%llu,"
                "\"chroma\":%llu},\"minimum_value\":%d,"
                "\"maximum_value\":%d",
            (unsigned long long)summary->count,
            (unsigned long long)summary->nonzero_count,
                (unsigned long long)summary->tree_counts[0U],
                (unsigned long long)summary->tree_counts[1U],
                (unsigned long long)summary->tree_counts[2U],
            summary->minimum_value, summary->maximum_value);
        if (summary->has_first_context) {
            fprintf(output,
                    ",\"first\":{\"mi_row\":%d,\"mi_column\":%d,"
                    "\"tree_type\":%d}",
                summary->first_context.y, summary->first_context.x,
                summary->first_context.tree_type);
        }
        fputc('}', output);
    }
    fputc(']', output);
}

static void write_accounting_trace(FILE *output, const Census *census) {
    size_t index;
    fputs(",\"accounting_trace\":[", output);
    for (index = 0U; index < census->accounting_trace_count; ++index) {
        const AccountingTraceEntry *entry =
            &census->accounting_trace[index];
        if (index != 0U) fputc(',', output);
        fprintf(output,
                "{\"id\":%u,\"value\":%d,\"bits\":%llu,"
                "\"mi_row\":%d,\"mi_column\":%d,"
                "\"tree_type\":%d,\"coding_mode\":%d,"
                "\"coding_type\":%d}",
                entry->id, entry->value,
                (unsigned long long)entry->bits,
                entry->context.y, entry->context.x,
                entry->context.tree_type, entry->coding_mode,
                entry->coding_type);
    }
    fputc(']', output);
}
#endif

static void write_census(FILE *output, const char *path, size_t byte_count,
                         const Census *census) {
    size_t index;
    fputs("    {\"path\":", output);
    json_string(output, path);
    fprintf(output,
            ",\"bytes\":%llu,\"frames\":%d,\"width\":%d,"
            "\"height\":%d,\"bit_depth\":%d,\"base_qindex\":%d,"
            "\"frame_header_bits\":%u,\"tile_entropy_tell\":%u,"
            "\"filters\":{\"deblock_enabled\":[%d,%d,%d,%d],"
            "\"cdef\":{\"enabled\":%d,\"strength_count\":%d,"
            "\"y_strength_0\":%d,\"uv_strength_0\":%d},"
            "\"ccso\":{\"frame_enabled\":%d,"
            "\"plane_enabled\":[%d,%d,%d]},\"gdf_mode\":%d},"
            "\"superblock_size\":{\"id\":%d,\"name\":",
            (unsigned long long)byte_count, census->frame_count,
            census->width, census->height, census->bit_depth,
            census->base_qindex, census->frame_header_bits,
            census->tile_entropy_tell,
            census->deblock_enabled[0], census->deblock_enabled[1],
            census->deblock_enabled[2], census->deblock_enabled[3],
            census->cdef_enabled, census->cdef_strength_count,
            census->cdef_y_strength, census->cdef_uv_strength,
            census->ccso_frame_enabled, census->ccso_enabled[0],
            census->ccso_enabled[1], census->ccso_enabled[2],
            census->gdf_mode, census->superblock_size);
    json_string(output, block_size_name(census->superblock_size));
    fprintf(output,
            "},\"coding_block_count\":%llu,"
            "\"feature_occurrence_count\":%llu,"
            "\"unique_feature_count\":%llu,"
            "\"features\":[\n",
            (unsigned long long)census->block_count,
            (unsigned long long)census->feature_occurrence_count,
            (unsigned long long)census->feature_count);
    for (index = 0U; index < census->feature_count; ++index) {
        write_feature(output, &census->features[index]);
        fputs(index + 1U == census->feature_count ? "\n" : ",\n", output);
    }
    fputs("    ]", output);
#if CONFIG_ACCOUNTING
    if (census->accounting_count != 0) {
        write_accounting(output, census);
    }
    if (census->trace_accounting) write_accounting_trace(output, census);
#endif
    if (census->trace_blocks) write_block_trace(output, census);
    fputc('}', output);
}

static void usage(const char *program) {
    fprintf(stderr,
            "Usage: %s [--trace-accounting] [--trace-blocks] "
            "[-o output.json] "
            "input.obu [input.obu ...]\n", program);
}

int main(int argc, char **argv) {
    const char *output_path = NULL;
    FILE *output = stdout;
    int first_input = 1;
    int input_count = 0;
    int trace_blocks = 0;
    int trace_accounting = 0;
    int index;
    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "-o") == 0) {
            if (++index >= argc || output_path != NULL) {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
            output_path = argv[index];
        } else if (strcmp(argv[index], "--trace-blocks") == 0) {
            trace_blocks = 1;
        } else if (strcmp(argv[index], "--trace-accounting") == 0) {
            trace_accounting = 1;
        } else {
            ++input_count;
        }
    }
    if (input_count == 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (output_path != NULL) {
        output = fopen(output_path, "wb");
        if (output == NULL) {
            fprintf(stderr, "av2-census: cannot create %s: %s\n", output_path,
                    strerror(errno));
            return EXIT_FAILURE;
        }
    }
    fputs("{\n  \"schema\":\"av2-feature-census/v1\",\n"
          "  \"avm_revision\":\"" AVM_PINNED_REVISION "\",\n"
          "  \"files\":[\n", output);
    for (index = 1; index < argc; ++index) {
        Census census;
        size_t byte_count;
        if (strcmp(argv[index], "-o") == 0) {
            ++index;
            continue;
        }
        if (strcmp(argv[index], "--trace-blocks") == 0) continue;
        if (strcmp(argv[index], "--trace-accounting") == 0) continue;
        memset(&census, 0, sizeof(census));
        census.trace_blocks = trace_blocks;
    #if CONFIG_ACCOUNTING
        census.trace_accounting = trace_accounting;
    #else
        (void)trace_accounting;
    #endif
        if (!decode_file(argv[index], &census, &byte_count)) {
            if (output != stdout) fclose(output);
            return EXIT_FAILURE;
        }
        if (!first_input) fputs(",\n", output);
        write_census(output, argv[index], byte_count, &census);
        first_input = 0;
        free(census.features);
        free(census.block_trace);
    #if CONFIG_ACCOUNTING
        free(census.accounting_trace);
    #endif
    }
    fputs("\n  ]\n}\n", output);
    if (output != stdout && fclose(output) != 0) {
        fprintf(stderr, "av2-census: failed to close %s\n", output_path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

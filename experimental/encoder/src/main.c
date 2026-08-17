#include "encoder/ac8x8.h"
#include "encoder/cli_input.h"
#include "encoder/image_input.h"
#include "encoder/input.h"
#include "encoder/m1.h"
#include "encoder/m3.h"
#include "encoder/m4.h"
#include "encoder/platform.h"

#define ENC_EXIT_OK 0
#define ENC_EXIT_USAGE 2
#define ENC_EXIT_INPUT_METADATA 3
#define ENC_EXIT_INPUT_IO 4
#define ENC_EXIT_UNSUPPORTED 5
#define ENC_EXIT_ENCODER 6
#define ENC_EXIT_OUTPUT_IO 7

static EncAc8x8Search quality_search = ENC_AC8X8_SEARCH_BOUNDED;

static int strings_equal(const char *left, const char *right) {
    if (left == 0 || right == 0) return 0;
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) return 0;
        ++left;
        ++right;
    }
    return *left == *right;
}

static size_t string_size(const char *text) {
    size_t size = 0U;
    if (text == 0) return 0U;
    while (text[size] != '\0') ++size;
    return size;
}

static void report_error(const char *message) {
    (void)enc_standard_error_write(message, string_size(message));
}

static void report_u64(uint64_t value) {
    char digits[24U];
    size_t used = 0U;
    do {
        digits[used++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);
    while (used != 0U) {
        --used;
        (void)enc_standard_error_write(&digits[used], 1U);
    }
}

static void report_i64(int64_t value) {
    if (value < 0) {
        report_error("-");
        report_u64((uint64_t)(-value));
    } else {
        report_u64((uint64_t)value);
    }
}

static uint64_t log2_q20(uint64_t value) {
    unsigned int integer = 0U;
    unsigned int bit;
    uint64_t probe = value;
    uint64_t normalized;
    uint64_t result;
    while (probe > 1U) {
        probe >>= 1U;
        ++integer;
    }
    normalized = integer <= 31U
        ? value << (31U - integer)
        : value >> (integer - 31U);
    result = (uint64_t)integer << 20U;
    for (bit = 0U; bit < 20U; ++bit) {
        normalized = (normalized * normalized) >> 31U;
        if (normalized >= (UINT64_C(2) << 31U)) {
            normalized >>= 1U;
            result |= UINT64_C(1) << (19U - bit);
        }
    }
    return result;
}

static void report_rd_winners(
    const size_t mode_counts[7U], const size_t residual_counts[3U],
    size_t candidate_trials, size_t qdelta_trials,
    int qdelta_y_preserved, int report_qdelta) {
    static const char *const mode_names[7U] = {
        "dc", "vertical", "horizontal", "smooth", "smooth_v", "smooth_h",
        "paeth"
    };
    static const char *const residual_names[3U] = {
        "skip", "dc", "ac"
    };
    size_t index;
    for (index = 0U; index < 7U; ++index) {
        report_error(" rd_mode_");
        report_error(mode_names[index]);
        report_error("=");
        report_u64(mode_counts[index]);
    }
    for (index = 0U; index < 3U; ++index) {
        report_error(" rd_residual_");
        report_error(residual_names[index]);
        report_error("=");
        report_u64(residual_counts[index]);
    }
    report_error(" rd_trials=");
    report_u64(candidate_trials);
    if (report_qdelta) {
        report_error(" rd_qdelta_trials=");
        report_u64(qdelta_trials);
        report_error(" rd_qdelta_y_preserved=");
        report_u64(qdelta_y_preserved != 0 ? 1U : 0U);
    }
}

static void report_m3_metrics(const EncM3Result *result) {
    report_error("encoder: bytes=");
    report_u64(result->stream_size);
    report_error(" sse_y=");
    report_u64(result->squared_error);
    report_error(" psnr_y=");
    if (result->squared_error == 0U) {
        report_error("inf");
    } else {
        uint64_t log_ratio = log2_q20(
                                 UINT64_C(65025) * result->pixel_count) -
                             log2_q20(result->squared_error);
        uint64_t centibels =
            (log_ratio * UINT64_C(30103) + UINT64_C(52428800)) /
            UINT64_C(104857600);
        report_u64(centibels / 100U);
        report_error(".");
        if ((centibels % 100U) < 10U) report_error("0");
        report_u64(centibels % 100U);
    }

    report_error("dB workspace_bytes=");
    report_u64(result->workspace_size);
    if (result->qindex != 0U) {
        report_error(" qindex=");
        report_u64(result->qindex);
    }
    if (quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL ||
        quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_EXACT ||
        quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_FULL ||
        quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_FULL_EXACT ||
        quality_search == ENC_AC8X8_SEARCH_INTRA) {
        report_error(" intra_mode=");
        report_u64(result->intra_mode);
    }
    report_error("\n");
}

static void report_m4_metrics(const EncM4Result *result) {
    EncM3Result metrics;
    metrics.stream_size = result->stream_size;
    metrics.squared_error = result->squared_error;
    metrics.pixel_count = result->pixel_count;
    metrics.workspace_size = result->pixel_count;
    metrics.qindex = 0U;
    report_m3_metrics(&metrics);
}

static void report_chroma_metrics(const EncM3ChromaResult *result) {
    report_error("encoder: bytes=");
    report_u64(result->stream_size);
    report_error(" sse_y=");
    report_u64(result->squared_error[0]);
    report_error(" sse_u=");
    report_u64(result->squared_error[1]);
    report_error(" sse_v=");
    report_u64(result->squared_error[2]);
    report_error(" workspace_bytes=");
    report_u64(result->workspace_size);
    report_error(" qindex=");
    report_u64(result->qindex);
    report_error(" qdelta_u=");
    report_i64(result->u_qdelta);
    report_error(" qdelta_v=");
    report_i64(result->v_qdelta);
    if (quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL ||
        quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_EXACT ||
        quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_FULL ||
        quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_FULL_EXACT ||
        quality_search == ENC_AC8X8_SEARCH_INTRA) {
        report_error(" luma_mode=");
        report_u64(result->luma_mode);
        report_error(" chroma_mode=");
        report_u64(result->chroma_mode);
    }
    report_error("\n");
}

static int parse_size(const char *text, size_t *value) {
    size_t parsed = 0U;
    size_t index = 0U;
    if (text == 0 || value == 0 || text[0] == '\0') return 0;
    while (text[index] != '\0') {
        size_t digit;
        if (text[index] < '0' || text[index] > '9') return 0;
        digit = (size_t)(text[index] - '0');
        if (parsed > (SIZE_MAX - digit) / 10U) return 0;
        parsed = parsed * 10U + digit;
        ++index;
    }

    *value = parsed;
    return 1;
}

static int parse_search(const char *text, EncAc8x8Search *search) {
    if (strings_equal(text, "off")) {
        *search = ENC_AC8X8_SEARCH_OFF;
        return 1;
    }
    if (strings_equal(text, "bounded")) {
        *search = ENC_AC8X8_SEARCH_BOUNDED;
        return 1;
    }
    if (strings_equal(text, "directional")) {
        *search = ENC_AC8X8_SEARCH_DIRECTIONAL;
        return 1;
    }
    if (strings_equal(text, "directional-exact")) {
        *search = ENC_AC8X8_SEARCH_DIRECTIONAL_EXACT;
        return 1;
    }
    if (strings_equal(text, "directional-full")) {
        *search = ENC_AC8X8_SEARCH_DIRECTIONAL_FULL;
        return 1;
    }
    if (strings_equal(text, "directional-full-exact")) {
        *search = ENC_AC8X8_SEARCH_DIRECTIONAL_FULL_EXACT;
        return 1;
    }
    if (strings_equal(text, "intra")) {
        *search = ENC_AC8X8_SEARCH_INTRA;
        return 1;
    }
    if (strings_equal(text, "plane-qdelta")) {
        *search = ENC_AC8X8_SEARCH_PLANE_QDELTA;
        return 1;
    }
    if (strings_equal(text, "rd")) {
        *search = ENC_AC8X8_SEARCH_RD;
        return 1;
    }
    return 0;
}

static int uses_ac8x8_search(EncAc8x8Search search) {
    return search == ENC_AC8X8_SEARCH_BOUNDED ||
           search == ENC_AC8X8_SEARCH_RD;
}

static EncStatus release_mapping(EncMemoryMapping *mapping) {
    if (mapping->data == 0) return ENC_OK;
    return enc_page_free(mapping);
}

static EncStatus release_i400_memory(
    EncMemoryMapping *workspace, EncMemoryMapping *stream,
    EncMemoryMapping *reconstruction) {
    EncStatus status = ENC_OK;
    if (release_mapping(reconstruction) != ENC_OK) status = ENC_IO_ERROR;
    if (release_mapping(stream) != ENC_OK) status = ENC_IO_ERROR;
    if (release_mapping(workspace) != ENC_OK) status = ENC_IO_ERROR;
    return status;
}

static int run_constant_mode(const char *output_path) {
    uint8_t buffer[ENC_M1_STREAM_SIZE];
    size_t output_size = 0U;
    EncStatus status;

    status = enc_m1_constant_128_8x8(
        buffer, sizeof(buffer), &output_size);
    if (status != ENC_OK) {
        report_error("encoder: constant mode failed\n");
        return ENC_EXIT_ENCODER;
    }
    status = enc_file_write_atomic(output_path, buffer, output_size);
    if (status != ENC_OK) {
        report_error("encoder: output write failed\n");
        return ENC_EXIT_OUTPUT_IO;
    }
    return ENC_EXIT_OK;
}

static int paths_equal(const char *left, const char *right) {
    return strings_equal(left, right);
}

static int validate_output_path(const char *input_path,
                                const char *output_path) {
    int same = 0;
    EncStatus status;
    if (output_path == 0 || output_path[0] == '\0') {
        report_error("encoder: invalid output path\n");
        return ENC_EXIT_USAGE;
    }
    if (paths_equal(input_path, output_path)) {
        report_error("encoder: output path must not be the input path\n");
        return ENC_EXIT_INPUT_METADATA;
    }
    status = enc_paths_same_file(input_path, output_path, &same);
    if (status != ENC_OK) {
        report_error("encoder: cannot inspect input/output collision\n");
        return ENC_EXIT_INPUT_IO;
    }
    if (same) {
        report_error("encoder: output resolves to the input file\n");
        return ENC_EXIT_INPUT_METADATA;
    }
    return ENC_EXIT_OK;
}

static int run_i400_encode(const char *input_path, size_t width,
                           size_t height, size_t stride, size_t qindex,
                           const char *output_path) {
    EncI400Input input;
    EncM3Result result;
    EncM3WorkspaceLayout layout;
    EncAc8x8WorkspaceLayout ac_layout;
    EncMemoryMapping workspace = {0, 0U};
    EncMemoryMapping stream = {0, 0U};
    EncMemoryMapping reconstruction = {0, 0U};
    EncStatus status;
    int collision_status =
        validate_output_path(input_path, output_path);
    if (collision_status != ENC_EXIT_OK) return collision_status;
    status = enc_i400_input_open(input_path, width, height, stride, &input);
    if (status == ENC_INVALID_ARGUMENT || status == ENC_OVERFLOW) {
        report_error("encoder: invalid I400 dimensions or stride\n");
        return ENC_EXIT_INPUT_METADATA;
    }
    if (status == ENC_SIZE_MISMATCH) {
        report_error("encoder: I400 file size does not equal stride * height\n");
        return ENC_EXIT_INPUT_IO;
    }
    if (status != ENC_OK) {
        report_error("encoder: cannot read I400 input\n");
        return ENC_EXIT_INPUT_IO;
    }
    if (width < ENC_M3_MIN_DIMENSION ||
        width > ENC_M3_PRODUCTION_MAX_DIMENSION ||
        height < ENC_M3_MIN_DIMENSION ||
        height > ENC_M3_PRODUCTION_MAX_DIMENSION) {
        (void)enc_i400_input_close(&input);
        report_error(
            "encoder: DC subset supports 1..1440 pixels per axis\n");
        return ENC_EXIT_UNSUPPORTED;
    }
    if (quality_search == ENC_AC8X8_SEARCH_PLANE_QDELTA) {
        (void)enc_i400_input_close(&input);
        report_error("encoder: plane-qdelta requires I420 or I444 input\n");
        return ENC_EXIT_UNSUPPORTED;
    }
    status = uses_ac8x8_search(quality_search)
        ? enc_ac8x8_query_workspace(input.width, input.height, &ac_layout)
        : enc_m3_query_workspace(input.width, input.height, &layout);
    if (status != ENC_OK) {
        (void)enc_i400_input_close(&input);
        report_error("encoder: I400 workspace geometry overflow\n");
        return ENC_EXIT_ENCODER;
    }
    if (enc_page_allocate(
            uses_ac8x8_search(quality_search)
                ? ac_layout.size : layout.size, &workspace) != ENC_OK ||
        enc_page_allocate(
            uses_ac8x8_search(quality_search)
                ? ac_layout.stream_capacity : layout.stream_capacity,
            &stream) != ENC_OK ||
        enc_page_allocate(
            uses_ac8x8_search(quality_search)
                ? ac_layout.pixel_count : layout.pixel_count,
            &reconstruction) != ENC_OK) {
        (void)release_i400_memory(&workspace, &stream, &reconstruction);
        (void)enc_i400_input_close(&input);
        report_error("encoder: cannot allocate I400 workspace\n");
        return ENC_EXIT_ENCODER;
    }
    if (uses_ac8x8_search(quality_search)) {
        EncAc8x8Result ac_result;
        status = enc_ac8x8_i400_q_workspace(
            input.plane, input.width, input.height, input.stride,
            (unsigned int)qindex, quality_search,
            workspace.data, workspace.size, stream.data, stream.size,
            (uint8_t *)reconstruction.data, reconstruction.size, &ac_result);
        result.stream_size = ac_result.stream_size;
        result.squared_error = ac_result.squared_error;
        result.pixel_count = ac_result.pixel_count;
        result.workspace_size = ac_result.workspace_size;
        result.qindex = ac_result.qindex;
        result.quant_level = 0U;
        result.quant_sign = 0U;
        result.reconstructed_sample =
            ac_result.pixel_count != 0U
                ? ((uint8_t *)reconstruction.data)[0] : 0U;
        result.block_count = ac_layout.block_count;
    } else {
        status = enc_m3_intra_i400_q_workspace(
            input.plane, input.width, input.height, input.stride,
            (unsigned int)qindex,
            quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL
                ? ENC_M3_INTRA_SEARCH_DIRECTIONAL
                : quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_EXACT
                    ? ENC_M3_INTRA_SEARCH_DIRECTIONAL_EXACT
                : quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_FULL
                    ? ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL
                : quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_FULL_EXACT
                    ? ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT
                : quality_search == ENC_AC8X8_SEARCH_INTRA
                    ? ENC_M3_INTRA_SEARCH_INTRA
                    : quality_search == ENC_AC8X8_SEARCH_PLANE_QDELTA
                        ? ENC_M3_INTRA_SEARCH_PLANE_QDELTA
                    : ENC_M3_INTRA_SEARCH_OFF,
            workspace.data, workspace.size,
            stream.data, stream.size,
            (uint8_t *)reconstruction.data, reconstruction.size, &result);
    }
    if (status != ENC_OK) {
        (void)release_i400_memory(&workspace, &stream, &reconstruction);
        (void)enc_i400_input_close(&input);
        report_error("encoder: I400 DC encoding failed\n");
        return ENC_EXIT_ENCODER;
    }
    status = enc_i400_input_close(&input);
    if (status != ENC_OK) {
        (void)release_i400_memory(&workspace, &stream, &reconstruction);
        report_error("encoder: cannot release I400 input\n");
        return ENC_EXIT_INPUT_IO;
    }
    status = enc_file_write_atomic(
        output_path, stream.data, result.stream_size);
    if (status != ENC_OK) {
        (void)release_i400_memory(&workspace, &stream, &reconstruction);
        report_error("encoder: output write failed\n");
        return ENC_EXIT_OUTPUT_IO;
    }
    report_m3_metrics(&result);
    if (release_i400_memory(
            &workspace, &stream, &reconstruction) != ENC_OK) {
        report_error("encoder: cannot release I400 workspace\n");
        return ENC_EXIT_ENCODER;
    }
    return ENC_EXIT_OK;
}

static int run_i400_image_encode(const char *input_path,
                                 const EncPlanarImage *image,
                                 size_t qindex, const char *output_path) {
    EncM3Result result;
    EncM3WorkspaceLayout layout;
    EncAc8x8WorkspaceLayout ac_layout;
    EncMemoryMapping workspace = {0, 0U};
    EncMemoryMapping stream = {0, 0U};
    EncMemoryMapping reconstruction = {0, 0U};
    EncAc8x8Result ac_result;
    EncStatus status;
    int result_status = ENC_EXIT_ENCODER;
    int have_ac_result = 0;
    int collision_status = validate_output_path(input_path, output_path);
    if (collision_status != ENC_EXIT_OK) return collision_status;
    if (enc_planar_image_validate(image) != ENC_OK ||
        image->format != ENC_PIXEL_FORMAT_I400_8) {
        report_error("encoder: invalid decoded I400 planes\n");
        return ENC_EXIT_INPUT_IO;
    }
    if (image->planes[0].width > ENC_M3_PRODUCTION_MAX_DIMENSION ||
        image->planes[0].height > ENC_M3_PRODUCTION_MAX_DIMENSION) {
        report_error("encoder: DC subset supports 1..1440 pixels per axis\n");
        return ENC_EXIT_UNSUPPORTED;
    }
    if (quality_search == ENC_AC8X8_SEARCH_PLANE_QDELTA) {
        report_error("encoder: plane-qdelta requires I420 or I444 input\n");
        return ENC_EXIT_UNSUPPORTED;
    }
    status = uses_ac8x8_search(quality_search)
        ? enc_ac8x8_query_workspace(
              image->planes[0].width, image->planes[0].height, &ac_layout)
        : enc_m3_query_workspace(
              image->planes[0].width, image->planes[0].height, &layout);
    if (status != ENC_OK) {
        report_error("encoder: I400 workspace geometry overflow\n");
        return ENC_EXIT_ENCODER;
    }
    if (enc_page_allocate(
            uses_ac8x8_search(quality_search)
                ? ac_layout.size : layout.size, &workspace) != ENC_OK ||
        enc_page_allocate(
            uses_ac8x8_search(quality_search)
                ? ac_layout.stream_capacity : layout.stream_capacity,
            &stream) != ENC_OK ||
        enc_page_allocate(
            uses_ac8x8_search(quality_search)
                ? ac_layout.pixel_count : layout.pixel_count,
            &reconstruction) != ENC_OK) {
        report_error("encoder: cannot allocate I400 workspace\n");
        goto cleanup;
    }
    if (uses_ac8x8_search(quality_search)) {
        status = enc_ac8x8_i400_q_workspace(
            image->planes[0].bytes.data, image->planes[0].width,
            image->planes[0].height, image->planes[0].stride,
            (unsigned int)qindex, quality_search,
            workspace.data, workspace.size, stream.data, stream.size,
            (uint8_t *)reconstruction.data, reconstruction.size, &ac_result);
        have_ac_result = 1;
        result.stream_size = ac_result.stream_size;
        result.squared_error = ac_result.squared_error;
        result.pixel_count = ac_result.pixel_count;
        result.workspace_size = ac_result.workspace_size;
        result.qindex = ac_result.qindex;
        result.quant_level = 0U;
        result.quant_sign = 0U;
        result.reconstructed_sample =
            ac_result.pixel_count != 0U
                ? ((uint8_t *)reconstruction.data)[0] : 0U;
        result.block_count = ac_layout.block_count;
    } else {
        status = enc_m3_intra_i400_q_workspace(
            image->planes[0].bytes.data, image->planes[0].width,
            image->planes[0].height, image->planes[0].stride,
            (unsigned int)qindex,
            quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL
                ? ENC_M3_INTRA_SEARCH_DIRECTIONAL
                : quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_EXACT
                    ? ENC_M3_INTRA_SEARCH_DIRECTIONAL_EXACT
                : quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_FULL
                    ? ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL
                : quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_FULL_EXACT
                    ? ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT
                : quality_search == ENC_AC8X8_SEARCH_INTRA
                    ? ENC_M3_INTRA_SEARCH_INTRA
                    : quality_search == ENC_AC8X8_SEARCH_PLANE_QDELTA
                        ? ENC_M3_INTRA_SEARCH_PLANE_QDELTA
                    : ENC_M3_INTRA_SEARCH_OFF,
            workspace.data, workspace.size,
            stream.data, stream.size, (uint8_t *)reconstruction.data,
            reconstruction.size, &result);
    }
    if (status != ENC_OK) {
        report_error("encoder: I400 DC encoding failed\n");
        goto cleanup;
    }
    if (enc_file_write_atomic(output_path, stream.data,
                              result.stream_size) != ENC_OK) {
        report_error("encoder: output write failed\n");
        result_status = ENC_EXIT_OUTPUT_IO;
        goto cleanup;
    }
    report_m3_metrics(&result);
    if (quality_search == ENC_AC8X8_SEARCH_RD && have_ac_result) {
        report_rd_winners(
            ac_result.rd_mode_counts, ac_result.rd_residual_counts,
            ac_result.candidate_trials, 0U, 0, 0);
        report_error("\n");
    }
    result_status = ENC_EXIT_OK;
cleanup:
    if (release_i400_memory(&workspace, &stream, &reconstruction) != ENC_OK &&
        result_status == ENC_EXIT_OK) {
        report_error("encoder: cannot release I400 workspace\n");
        result_status = ENC_EXIT_ENCODER;
    }
    return result_status;
}

static EncM3PixelFormat enc_m3_format(EncPixelFormat format) {
    return format == ENC_PIXEL_FORMAT_I420_8 ? ENC_M3_PIXEL_I420 :
                                               ENC_M3_PIXEL_I444;
}

static int run_chroma_encode(const char *input_path,
                             const EncPlanarImage *image, size_t qindex,
                             const char *output_path) {
    EncM3Image8 input;
    EncM3MutableImage8 reconstruction;
    EncM3ChromaWorkspaceLayout layout;
    EncAc8x8YuvWorkspaceLayout ac_layout;
    EncM3ChromaResult result;
    EncMemoryMapping workspace = {0, 0U};
    EncMemoryMapping stream = {0, 0U};
    EncMemoryMapping reconstruction_memory = {0, 0U};
    EncAc8x8YuvResult ac_result;
    EncStatus status;
    size_t offset = 0U;
    size_t plane;
    int cleanup_failed = 0;
    int result_status = ENC_EXIT_ENCODER;
    int have_ac_result = 0;
    int collision_status = validate_output_path(input_path, output_path);
    if (collision_status != ENC_EXIT_OK) return collision_status;
    if (enc_planar_image_validate(image) != ENC_OK ||
        (image->format != ENC_PIXEL_FORMAT_I420_8 &&
         image->format != ENC_PIXEL_FORMAT_I444_8)) {
        report_error("encoder: invalid decoded chroma planes\n");
        return ENC_EXIT_INPUT_IO;
    }
    input.format = enc_m3_format(image->format);
    input.width = image->planes[0].width;
    input.height = image->planes[0].height;
    reconstruction.format = input.format;
    reconstruction.width = input.width;
    reconstruction.height = input.height;
    status = uses_ac8x8_search(quality_search)
        ? enc_ac8x8_query_yuv8_workspace(
              input.format, input.width, input.height, &ac_layout)
        : enc_m3_query_chroma_workspace(
              input.format, input.width, input.height, &layout);
    if (status == ENC_UNSUPPORTED) {
        report_error("encoder: DC subset supports 1..1440 pixels per axis\n");
        return ENC_EXIT_UNSUPPORTED;
    }
    if (status != ENC_OK) {
        report_error("encoder: chroma workspace geometry overflow\n");
        return ENC_EXIT_ENCODER;
    }
    if (enc_page_allocate(
            uses_ac8x8_search(quality_search)
                ? ac_layout.size : layout.size, &workspace) != ENC_OK ||
        enc_page_allocate(
            uses_ac8x8_search(quality_search)
                ? ac_layout.stream_capacity : layout.stream_capacity,
            &stream) != ENC_OK ||
        enc_page_allocate(
            uses_ac8x8_search(quality_search)
                ? ac_layout.plane_size[0U] + ac_layout.plane_size[1U] +
                      ac_layout.plane_size[2U]
                : layout.reconstruction_size,
                          &reconstruction_memory) != ENC_OK) {
        report_error("encoder: cannot allocate chroma workspace\n");
        goto cleanup;
    }
    for (plane = 0U; plane < 3U; ++plane) {
        input.planes[plane].data = image->planes[plane].bytes.data;
        input.planes[plane].stride = image->planes[plane].stride;
        input.planes[plane].capacity = image->planes[plane].bytes.size;
        reconstruction.planes[plane].data =
            (uint8_t *)reconstruction_memory.data + offset;
        reconstruction.planes[plane].stride =
            uses_ac8x8_search(quality_search)
                ? ac_layout.plane_width[plane] : layout.plane_width[plane];
        reconstruction.planes[plane].capacity =
            uses_ac8x8_search(quality_search)
                ? ac_layout.plane_size[plane] : layout.plane_size[plane];
        offset += reconstruction.planes[plane].capacity;
    }
    if (uses_ac8x8_search(quality_search)) {
        size_t index;
        status = enc_ac8x8_yuv8_q_workspace(
            &input, (unsigned int)qindex,
            quality_search == ENC_AC8X8_SEARCH_RD
                ? ENC_AC8X8_COEFFICIENT_SEARCH_RD
                : ENC_AC8X8_COEFFICIENT_SEARCH_BOUNDED,
            workspace.data, workspace.size,
            stream.data, stream.size, &reconstruction, &ac_result);
        have_ac_result = 1;
        result.stream_size = ac_result.stream_size;
        result.workspace_size = ac_result.workspace_size;
        result.qindex = ac_result.qindex;
        result.format = ac_result.format;
        result.u_qdelta = ac_result.u_qdelta;
        result.v_qdelta = ac_result.v_qdelta;
        result.luma_mode = ac_result.luma_mode;
        result.chroma_mode = ac_result.chroma_mode;
        for (index = 0U; index < 3U; ++index) {
            result.plane_size[index] = ac_result.plane_size[index];
            result.transform_count[index] = ac_result.transform_count[index];
            result.squared_error[index] = ac_result.squared_error[index];
        }
    } else {
        status = enc_m3_intra_yuv8_q_workspace(
            &input, (unsigned int)qindex,
            quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL
                ? ENC_M3_INTRA_SEARCH_DIRECTIONAL
                : quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_EXACT
                    ? ENC_M3_INTRA_SEARCH_DIRECTIONAL_EXACT
                : quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_FULL
                    ? ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL
                : quality_search == ENC_AC8X8_SEARCH_DIRECTIONAL_FULL_EXACT
                    ? ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT
                : quality_search == ENC_AC8X8_SEARCH_INTRA
                    ? ENC_M3_INTRA_SEARCH_INTRA
                : quality_search == ENC_AC8X8_SEARCH_PLANE_QDELTA
                    ? ENC_M3_INTRA_SEARCH_PLANE_QDELTA
                : ENC_M3_INTRA_SEARCH_OFF,
            workspace.data, workspace.size,
            stream.data, stream.size, &reconstruction, &result);
    }
    if (status != ENC_OK) {
        report_error("encoder: chroma DC encoding failed\n");
        goto cleanup;
    }
    if (enc_file_write_atomic(output_path, stream.data,
                              result.stream_size) != ENC_OK) {
        report_error("encoder: output write failed\n");
        result_status = ENC_EXIT_OUTPUT_IO;
        goto cleanup;
    }
    report_chroma_metrics(&result);
    if (quality_search == ENC_AC8X8_SEARCH_RD && have_ac_result) {
        report_rd_winners(
            ac_result.rd_mode_counts, ac_result.rd_residual_counts,
            ac_result.candidate_trials, ac_result.rd_qdelta_trials,
            ac_result.rd_qdelta_y_preserved, 1);
        report_error("\n");
    }
    result_status = ENC_EXIT_OK;
cleanup:
    if (release_mapping(&reconstruction_memory) != ENC_OK) cleanup_failed = 1;
    if (release_mapping(&stream) != ENC_OK) cleanup_failed = 1;
    if (release_mapping(&workspace) != ENC_OK) cleanup_failed = 1;
    if (cleanup_failed) {
        report_error("encoder: cannot release chroma workspace\n");
        if (result_status == ENC_EXIT_OK) result_status = ENC_EXIT_ENCODER;
    }
    return result_status;
}

static int run_raw_encode(const char *input_path, EncPixelFormat format,
                          size_t width, size_t height, size_t stride,
                          size_t qindex, const char *output_path) {
    EncPlanarInput input;
    size_t strides[3];
    EncStatus status;
    int result;
    strides[0] = stride;
    strides[1] = format == ENC_PIXEL_FORMAT_I400_8 ? 0U :
                 format == ENC_PIXEL_FORMAT_I420_8 ? (width + 1U) / 2U :
                                                     width;
    strides[2] = strides[1];
    status = enc_planar_input_open(input_path, format, width, height,
                                   strides, &input);
    if (status == ENC_INVALID_ARGUMENT || status == ENC_OVERFLOW) {
        report_error("encoder: invalid raw dimensions or stride\n");
        return ENC_EXIT_INPUT_METADATA;
    }
    if (status == ENC_SIZE_MISMATCH) {
        report_error("encoder: raw file size does not match its plane layout\n");
        return ENC_EXIT_INPUT_IO;
    }
    if (status != ENC_OK) {
        report_error("encoder: cannot read raw planar input\n");
        return ENC_EXIT_INPUT_IO;
    }
    result = format == ENC_PIXEL_FORMAT_I400_8
        ? run_i400_image_encode(input_path, &input.image, qindex, output_path)
        : run_chroma_encode(input_path, &input.image, qindex, output_path);
    if (enc_planar_input_close(&input) != ENC_OK) {
        report_error("encoder: cannot release raw planar input\n");
        if (result == ENC_EXIT_OK) result = ENC_EXIT_INPUT_IO;
    }
    return result;
}

static int run_image_encode(const char *input_path, size_t qindex,
                            const char *output_path) {
    EncReadMapping source = {0, 0U};
    EncMemoryMapping decoded = {0, 0U};
    EncMemoryMapping scratch = {0, 0U};
    EncImageLayout layout;
    EncImage decoded_image;
    EncPlanarImage image;
    size_t strides[3];
    EncStatus status;
    int result = ENC_EXIT_INPUT_IO;
    status = enc_file_map_readonly(input_path, ENC_IMAGE_MAX_BYTES, &source);
    if (status == ENC_UNSUPPORTED) {
        report_error("encoder: image input exceeds 256 MiB\n");
        return ENC_EXIT_UNSUPPORTED;
    }
    if (status != ENC_OK) {
        report_error("encoder: cannot map image input\n");
        return ENC_EXIT_INPUT_IO;
    }
    status = enc_image_query(source.data, source.size, &layout);
    if (status == ENC_UNSUPPORTED) {
        report_error("encoder: unsupported BMP/PNG subset\n");
        result = ENC_EXIT_UNSUPPORTED;
        goto cleanup;
    }
    if (status != ENC_OK) {
        report_error("encoder: malformed BMP/PNG input\n");
        goto cleanup;
    }
    if (layout.width > ENC_M3_PRODUCTION_MAX_DIMENSION ||
        layout.height > ENC_M3_PRODUCTION_MAX_DIMENSION) {
        report_error("encoder: DC subset supports 1..1440 pixels per axis\n");
        result = ENC_EXIT_UNSUPPORTED;
        goto cleanup;
    }
    if (enc_page_allocate(layout.output_size, &decoded) != ENC_OK ||
        (layout.scratch_size != 0U &&
         enc_page_allocate(layout.scratch_size, &scratch) != ENC_OK)) {
        report_error("encoder: cannot allocate image decode storage\n");
        result = ENC_EXIT_ENCODER;
        goto cleanup;
    }
    status = enc_image_decode(source.data, source.size, decoded.data,
                              decoded.size, scratch.data, scratch.size,
                              &decoded_image);
    if (status == ENC_UNSUPPORTED) {
        report_error("encoder: unsupported BMP/PNG subset\n");
        result = ENC_EXIT_UNSUPPORTED;
        goto cleanup;
    }
    if (status != ENC_OK) {
        report_error("encoder: malformed BMP/PNG input\n");
        goto cleanup;
    }
    strides[0] = decoded_image.stride[0];
    strides[1] = decoded_image.stride[1];
    strides[2] = decoded_image.stride[2];
    status = enc_planar_image_layout(
        decoded_image.type == ENC_IMAGE_I400 ? ENC_PIXEL_FORMAT_I400_8 :
                                               ENC_PIXEL_FORMAT_I444_8,
        decoded_image.width, decoded_image.height, strides,
        decoded.size, (const uint8_t *)decoded.data, &image);
    if (status != ENC_OK) {
        report_error("encoder: decoded image plane layout is invalid\n");
        result = ENC_EXIT_ENCODER;
        goto cleanup;
    }
    result = image.format == ENC_PIXEL_FORMAT_I400_8
        ? run_i400_image_encode(input_path, &image, qindex, output_path)
        : run_chroma_encode(input_path, &image, qindex, output_path);
cleanup:
    if (release_mapping(&scratch) != ENC_OK && result == ENC_EXIT_OK) {
        result = ENC_EXIT_ENCODER;
    }
    if (release_mapping(&decoded) != ENC_OK && result == ENC_EXIT_OK) {
        result = ENC_EXIT_ENCODER;
    }
    if (source.data != 0 && enc_file_unmap(&source) != ENC_OK &&
        result == ENC_EXIT_OK) {
        result = ENC_EXIT_INPUT_IO;
    }
    return result;
}

static int run_i400_mode(long argument_count, char **arguments) {
    size_t width;
    size_t height;
    size_t stride;
    size_t qindex = ENC_M3_DEFAULT_QINDEX;
    long option;
    int saw_stride = 0;
    int saw_qindex = 0;
    int saw_search = 0;
    const char *output_path;
    if ((argument_count != 8 && argument_count != 10 &&
         argument_count != 12 && argument_count != 14) ||
        !strings_equal(arguments[3], "--width") ||
        !strings_equal(arguments[5], "--height")) {
        report_error("usage: encoder --i400-8 INPUT --width W --height H "
                     "[--stride S] [--qindex Q] "
                     "[--search off|bounded|directional|directional-exact|"
                     "directional-full|directional-full-exact|intra|"
                     "plane-qdelta|rd] "
                     "OUTPUT\n");
        return ENC_EXIT_USAGE;
    }

    if (!parse_size(arguments[4], &width) ||
        !parse_size(arguments[6], &height)) {
        report_error("encoder: invalid I400 dimensions\n");
        return ENC_EXIT_INPUT_METADATA;
    }
    stride = width;
    for (option = 7; option + 1 < argument_count - 1; option += 2) {
        if (strings_equal(arguments[option], "--stride") && !saw_stride) {
            if (!parse_size(arguments[option + 1], &stride)) {
                report_error("encoder: invalid I400 stride\n");
                return ENC_EXIT_INPUT_METADATA;
            }
            saw_stride = 1;
        } else if (strings_equal(arguments[option], "--qindex") &&
                   !saw_qindex) {
            if (!parse_size(arguments[option + 1], &qindex) ||
                qindex < ENC_M3_MIN_QINDEX ||
                qindex > ENC_M3_MAX_QINDEX) {
                report_error("encoder: qindex must be in 1..255\n");
                return ENC_EXIT_INPUT_METADATA;
            }
            saw_qindex = 1;
        } else if (strings_equal(arguments[option], "--search") &&
                   !saw_search) {
            if (!parse_search(arguments[option + 1], &quality_search)) {
                report_error(
                    "encoder: search must be off, bounded, directional, "
                    "directional-exact, directional-full, "
                    "directional-full-exact, intra, plane-qdelta, or rd\n");
                return ENC_EXIT_INPUT_METADATA;
            }
            saw_search = 1;
        } else {
            report_error("usage: encoder --i400-8 INPUT --width W --height H "
                         "[--stride S] [--qindex Q] "
                         "[--search off|bounded|directional|directional-exact|"
                         "directional-full|directional-full-exact|intra|"
                         "plane-qdelta|rd] "
                         "OUTPUT\n");
            return ENC_EXIT_USAGE;
        }
    }
    output_path = arguments[argument_count - 1];
    return run_i400_encode(arguments[2], width, height, stride, qindex,
                           output_path);
}

static int run_m4_mode(long argument_count, char **arguments) {
    EncI400Input input;
    EncM4Result result;
    uint8_t stream[ENC_M4_MAX_STREAM_SIZE];
    uint8_t reconstruction[ENC_M4_PIXELS];
    EncStatus status;
    if (argument_count != 4) {
        report_error(
            "usage: encoder --experimental-m4-ac INPUT OUTPUT\n");
        return ENC_EXIT_USAGE;
    }
    {
        int collision_status =
            validate_output_path(arguments[2], arguments[3]);
        if (collision_status != ENC_EXIT_OK) return collision_status;
    }
    status = enc_i400_input_open(arguments[2], 8U, 8U, 8U, &input);
    if (status == ENC_SIZE_MISMATCH) {
        report_error("encoder: M4 input must contain exactly 64 bytes\n");
        return ENC_EXIT_INPUT_IO;
    }
    if (status != ENC_OK) {
        report_error("encoder: cannot read M4 I400 input\n");
        return ENC_EXIT_INPUT_IO;
    }
    status = enc_m4_ac_i400(
        input.plane, input.stride, stream, sizeof(stream),
        reconstruction, sizeof(reconstruction), &result);
    if (enc_i400_input_close(&input) != ENC_OK) {
        report_error("encoder: cannot release M4 I400 input\n");
        return ENC_EXIT_INPUT_IO;
    }
    if (status != ENC_OK) {
        report_error("encoder: experimental M4 AC encoding failed\n");
        return ENC_EXIT_ENCODER;
    }
    status = enc_file_write_atomic(arguments[3], stream, result.stream_size);
    if (status != ENC_OK) {
        report_error("encoder: output write failed\n");
        return ENC_EXIT_OUTPUT_IO;
    }
    report_m4_metrics(&result);
    return ENC_EXIT_OK;
}

static void report_generic_usage(void) {
    report_error(
        "usage: encoder INPUT [--output OUTPUT] [--qindex Q]\n"
        "       encoder RAW [--width W --height H] [--stride S] "
        "[--output OUTPUT] [--qindex Q] "
        "[--search off|bounded|directional|directional-exact|"
        "directional-full|directional-full-exact|intra|plane-qdelta|rd]\n"
        "       encoder --i400-8 INPUT --width W --height H "
        "[--stride S] [--qindex Q] "
        "[--search off|bounded|directional|directional-exact|"
        "directional-full|directional-full-exact|intra|plane-qdelta|rd] OUTPUT\n"
        "formats: .i400, .i420, .i444, .bmp, .png; default qindex: 100; "
        "default search: off\n"
        "output: sibling INPUT with its extension replaced by .obu\n");
}

static int report_discovery_error(EncStatus status, EncCliInputIssue issue) {
    if (status == ENC_IO_ERROR) {
        report_error("encoder: cannot read input\n");
        return ENC_EXIT_INPUT_IO;
    }
    if (issue == ENC_CLI_INPUT_ISSUE_UNKNOWN_FORMAT) {
        report_error(
            "encoder: unknown input format; expected .i400, .i420, .i444, "
            ".png, or .bmp\n");
    } else if (issue == ENC_CLI_INPUT_ISSUE_CONTENT_MISMATCH) {
        report_error("encoder: input content does not match its extension\n");
    } else if (issue == ENC_CLI_INPUT_ISSUE_METADATA_AMBIGUOUS) {
        report_error(
            "encoder: raw geometry is ambiguous; use --width and --height\n");
    } else if (issue == ENC_CLI_INPUT_ISSUE_METADATA_CONFLICT) {
        report_error(
            "encoder: raw filename and sibling manifest geometry conflict\n");
    } else {
        report_error(
            "encoder: raw geometry missing; use one WIDTHxHEIGHT filename "
            "token, a matching sibling manifest.json, or --width/--height\n");
    }
    return ENC_EXIT_INPUT_METADATA;
}

static int run_inferred_mode(long argument_count, char **arguments) {
    EncCliInputContract contract;
    EncCliInputIssue issue;
    EncStatus discovery_status;
    const char *input_path = 0;
    const char *output_path = 0;
    char default_output[ENC_CLI_PATH_CAPACITY];
    size_t width = 0U;
    size_t height = 0U;
    size_t stride = 0U;
    size_t qindex = ENC_M3_DEFAULT_QINDEX;
    int saw_width = 0;
    int saw_height = 0;
    int saw_stride = 0;
    int saw_qindex = 0;
    int saw_output = 0;
    int saw_search = 0;
    long index;
    for (index = 1; index < argument_count; ++index) {
        const char *argument = arguments[index];
        if (strings_equal(argument, "--width") ||
            strings_equal(argument, "--height") ||
            strings_equal(argument, "--stride") ||
            strings_equal(argument, "--qindex") ||
            strings_equal(argument, "--search") ||
            strings_equal(argument, "--output") ||
            strings_equal(argument, "-o")) {
            const char *value;
            if (index + 1 >= argument_count) {
                report_generic_usage();
                return ENC_EXIT_USAGE;
            }
            value = arguments[++index];
            if (strings_equal(argument, "--width") && !saw_width) {
                if (!parse_size(value, &width)) {
                    report_error("encoder: invalid raw width\n");
                    return ENC_EXIT_INPUT_METADATA;
                }
                saw_width = 1;
            } else if (strings_equal(argument, "--height") && !saw_height) {
                if (!parse_size(value, &height)) {
                    report_error("encoder: invalid raw height\n");
                    return ENC_EXIT_INPUT_METADATA;
                }
                saw_height = 1;
            } else if (strings_equal(argument, "--stride") && !saw_stride) {
                if (!parse_size(value, &stride)) {
                    report_error("encoder: invalid raw stride\n");
                    return ENC_EXIT_INPUT_METADATA;
                }
                saw_stride = 1;
            } else if (strings_equal(argument, "--qindex") && !saw_qindex) {
                if (!parse_size(value, &qindex) ||
                    qindex < ENC_M3_MIN_QINDEX ||
                    qindex > ENC_M3_MAX_QINDEX) {
                    report_error("encoder: qindex must be in 1..255\n");
                    return ENC_EXIT_INPUT_METADATA;
                }
                saw_qindex = 1;
            } else if ((strings_equal(argument, "--output") ||
                        strings_equal(argument, "-o")) &&
                       !saw_output) {
                output_path = value;
                saw_output = 1;
            } else if (strings_equal(argument, "--search") && !saw_search) {
                if (!parse_search(value, &quality_search)) {
                    report_error(
                        "encoder: search must be off, bounded, directional, "
                        "directional-exact, directional-full, "
                        "directional-full-exact, intra, plane-qdelta, or rd\n");
                    return ENC_EXIT_INPUT_METADATA;
                }
                saw_search = 1;
            } else {
                report_generic_usage();
                return ENC_EXIT_USAGE;
            }
        } else if (argument[0] == '-') {
            report_error("encoder: unknown option\n");
            return ENC_EXIT_USAGE;
        } else if (input_path == 0) {
            input_path = argument;
        } else {
            report_generic_usage();
            return ENC_EXIT_USAGE;
        }
    }
    if (input_path == 0 || saw_width != saw_height) {
        report_generic_usage();
        return ENC_EXIT_USAGE;
    }
    discovery_status = enc_cli_input_discover(input_path, &contract, &issue);
    if (discovery_status != ENC_OK &&
        !(saw_width && (contract.format == ENC_CLI_INPUT_I400 ||
                        contract.format == ENC_CLI_INPUT_I420 ||
                        contract.format == ENC_CLI_INPUT_I444))) {
        return report_discovery_error(discovery_status, issue);
    }
    if (output_path == 0) {
        if (enc_cli_default_output(input_path, default_output,
                                   sizeof(default_output)) != ENC_OK) {
            report_error("encoder: cannot derive sibling .obu output path\n");
            return ENC_EXIT_USAGE;
        }
        output_path = default_output;
    }
    if (contract.format == ENC_CLI_INPUT_PNG ||
        contract.format == ENC_CLI_INPUT_BMP) {
        if (saw_width || saw_height || saw_stride) {
            report_error(
                "encoder: dimensions and stride apply only to raw input\n");
            return ENC_EXIT_USAGE;
        }
        return run_image_encode(input_path, qindex, output_path);
    }
    if (!saw_width) {
        width = contract.width;
        height = contract.height;
    }
    if (contract.format != ENC_CLI_INPUT_I400 && saw_stride) {
        report_error(
            "encoder: --stride is available only for I400 raw input\n");
        return ENC_EXIT_USAGE;
    }
    if (!saw_stride) stride = width;
    return run_raw_encode(
        input_path,
        contract.format == ENC_CLI_INPUT_I400 ? ENC_PIXEL_FORMAT_I400_8 :
        contract.format == ENC_CLI_INPUT_I420 ? ENC_PIXEL_FORMAT_I420_8 :
                                                ENC_PIXEL_FORMAT_I444_8,
        width, height, stride, qindex, output_path);
}

int encoder_main(long argument_count, char **arguments) {
    quality_search = ENC_AC8X8_SEARCH_BOUNDED;
    if (arguments == 0 || argument_count < 2) {
        report_generic_usage();
        return ENC_EXIT_USAGE;
    }
    if (argument_count == 3 &&
        strings_equal(arguments[1], "--constant-128-8x8")) {
        return run_constant_mode(arguments[2]);
    }
    if (argument_count == 2 &&
        (strings_equal(arguments[1], "--help") ||
         strings_equal(arguments[1], "-h"))) {
        report_generic_usage();
        return ENC_EXIT_OK;
    }
    if (strings_equal(arguments[1], "--i400-8")) {
        return run_i400_mode(argument_count, arguments);
    }
    if (strings_equal(arguments[1], "--experimental-m4-ac")) {
        return run_m4_mode(argument_count, arguments);
    }
    return run_inferred_mode(argument_count, arguments);
}

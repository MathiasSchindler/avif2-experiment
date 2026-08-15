#include "encoder/cli_input.h"
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
            "encoder: DC subset supports 1..896 pixels per axis\n");
        return ENC_EXIT_UNSUPPORTED;
    }
    status = enc_m3_query_workspace(input.width, input.height, &layout);
    if (status != ENC_OK) {
        (void)enc_i400_input_close(&input);
        report_error("encoder: I400 workspace geometry overflow\n");
        return ENC_EXIT_ENCODER;
    }
    if (enc_page_allocate(layout.size, &workspace) != ENC_OK ||
        enc_page_allocate(layout.stream_capacity, &stream) != ENC_OK ||
        enc_page_allocate(layout.pixel_count, &reconstruction) != ENC_OK) {
        (void)release_i400_memory(&workspace, &stream, &reconstruction);
        (void)enc_i400_input_close(&input);
        report_error("encoder: cannot allocate I400 workspace\n");
        return ENC_EXIT_ENCODER;
    }
    status = enc_m3_dc_i400_q_workspace(
        input.plane, input.width, input.height, input.stride,
        (unsigned int)qindex,
        workspace.data, workspace.size,
        stream.data, stream.size,
        (uint8_t *)reconstruction.data, reconstruction.size, &result);
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

static int run_i400_mode(long argument_count, char **arguments) {
    size_t width;
    size_t height;
    size_t stride;
    size_t qindex = ENC_M3_DEFAULT_QINDEX;
    long option;
    int saw_stride = 0;
    int saw_qindex = 0;
    const char *output_path;
    if ((argument_count != 8 && argument_count != 10 &&
         argument_count != 12) ||
        !strings_equal(arguments[3], "--width") ||
        !strings_equal(arguments[5], "--height")) {
        report_error("usage: encoder --i400-8 INPUT --width W --height H "
                     "[--stride S] [--qindex Q] OUTPUT\n");
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
        } else {
            report_error("usage: encoder --i400-8 INPUT --width W --height H "
                         "[--stride S] [--qindex Q] OUTPUT\n");
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
        "[--output OUTPUT] [--qindex Q]\n"
        "       encoder --i400-8 INPUT --width W --height H "
        "[--stride S] [--qindex Q] OUTPUT\n");
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
    long index;
    for (index = 1; index < argument_count; ++index) {
        const char *argument = arguments[index];
        if (strings_equal(argument, "--width") ||
            strings_equal(argument, "--height") ||
            strings_equal(argument, "--stride") ||
            strings_equal(argument, "--qindex") ||
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
        int collision_status = validate_output_path(input_path, output_path);
        if (collision_status != ENC_EXIT_OK) return collision_status;
        report_error(
            "encoder: BMP/PNG inputs default to 8-bit I444, but color "
            "encoding is not supported by this encoder subset\n");
        return ENC_EXIT_UNSUPPORTED;
    }
    if (contract.format != ENC_CLI_INPUT_I400) {
        int collision_status = validate_output_path(input_path, output_path);
        if (collision_status != ENC_EXIT_OK) return collision_status;
        report_error(
            "encoder: inferred raw chroma input is not supported by this "
            "encoder subset\n");
        return ENC_EXIT_UNSUPPORTED;
    }
    if (!saw_width) {
        width = contract.width;
        height = contract.height;
    }
    if (!saw_stride) stride = width;
    return run_i400_encode(input_path, width, height, stride, qindex,
                           output_path);
}

int encoder_main(long argument_count, char **arguments) {
    if (arguments == 0 || argument_count < 2) {
        report_generic_usage();
        return ENC_EXIT_USAGE;
    }
    if (argument_count == 3 &&
        strings_equal(arguments[1], "--constant-128-8x8")) {
        return run_constant_mode(arguments[2]);
    }
    if (strings_equal(arguments[1], "--i400-8")) {
        return run_i400_mode(argument_count, arguments);
    }
    if (strings_equal(arguments[1], "--experimental-m4-ac")) {
        return run_m4_mode(argument_count, arguments);
    }
    return run_inferred_mode(argument_count, arguments);
}

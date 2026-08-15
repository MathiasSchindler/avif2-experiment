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
    report_u64(result->pixel_count);
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

static int run_i400_mode(long argument_count, char **arguments) {
    EncI400Input input;
    EncM3Result result;
    uint8_t stream[ENC_M3_MAX_STREAM_SIZE];
    uint8_t reconstruction[ENC_M3_MAX_PIXELS];
    EncStatus status;
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
    if (output_path == 0 || output_path[0] == '\0') {
        report_error("encoder: invalid output path\n");
        return ENC_EXIT_USAGE;
    }
    status = enc_i400_input_open(arguments[2], width, height, stride, &input);
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
    if (width < ENC_M3_MIN_DIMENSION || width > ENC_M3_MAX_DIMENSION ||
        height < ENC_M3_MIN_DIMENSION || height > ENC_M3_MAX_DIMENSION) {
        (void)enc_i400_input_close(&input);
        report_error(
            "encoder: DC subset supports 1 through 16 pixels per axis\n");
        return ENC_EXIT_UNSUPPORTED;
    }
    status = enc_m3_dc_i400_q(
        input.plane, input.width, input.height, input.stride,
        (unsigned int)qindex,
        stream, sizeof(stream),
        reconstruction, sizeof(reconstruction), &result);
    if (status != ENC_OK) {
        (void)enc_i400_input_close(&input);
        report_error("encoder: I400 DC encoding failed\n");
        return ENC_EXIT_ENCODER;
    }
    status = enc_i400_input_close(&input);
    if (status != ENC_OK) {
        report_error("encoder: cannot release I400 input\n");
        return ENC_EXIT_INPUT_IO;
    }
    status = enc_file_write_atomic(output_path, stream, result.stream_size);
    if (status != ENC_OK) {
        report_error("encoder: output write failed\n");
        return ENC_EXIT_OUTPUT_IO;
    }
    report_m3_metrics(&result);
    return ENC_EXIT_OK;
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

int encoder_main(long argument_count, char **arguments) {
    if (arguments == 0 || argument_count < 2) {
        report_error("usage: encoder --constant-128-8x8 OUTPUT or "
                     "--i400-8 INPUT --width W --height H "
                     "[--stride S] [--qindex Q] OUTPUT\n");
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
    report_error("encoder: unknown mode\n");
    return ENC_EXIT_USAGE;
}

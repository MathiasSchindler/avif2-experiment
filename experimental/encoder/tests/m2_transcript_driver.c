#include "encoder/range.h"
#include "encoder/cdf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    OP_SYMBOL,
    OP_BYPASS,
    OP_LITERAL
} OperationKind;

typedef struct {
    OperationKind kind;
    unsigned int value;
    unsigned int argument;
} Operation;

typedef struct {
    const char *name;
    const Operation *operations;
    size_t operation_count;
} Transcript;

static const Operation binary_sign[] = {
    { OP_SYMBOL, 1U, 2U },
    { OP_BYPASS, 1U, 0U },
    { OP_BYPASS, 0U, 0U },
};
static const Operation adaptive_three[] = {
    { OP_SYMBOL, 0U, 3U }, { OP_SYMBOL, 2U, 3U },
    { OP_SYMBOL, 1U, 3U }, { OP_SYMBOL, 1U, 3U },
    { OP_SYMBOL, 0U, 3U }, { OP_SYMBOL, 2U, 3U },
    { OP_SYMBOL, 2U, 3U }, { OP_SYMBOL, 1U, 3U },
};
static const Operation base_br_five[] = {
    { OP_SYMBOL, 4U, 5U },
    { OP_SYMBOL, 3U, 5U },
    { OP_BYPASS, 1U, 0U },
};
static const Operation remainder_eight[] = {
    { OP_SYMBOL, 7U, 8U },
    { OP_LITERAL, 0x13U, 5U },
    { OP_BYPASS, 0U, 0U },
    { OP_LITERAL, 0x2d3U, 10U },
};

static const Transcript transcripts[] = {
    { "binary-sign", binary_sign,
      sizeof(binary_sign) / sizeof(binary_sign[0]) },
    { "adaptive-three", adaptive_three,
      sizeof(adaptive_three) / sizeof(adaptive_three[0]) },
    { "base-br-five", base_br_five,
      sizeof(base_br_five) / sizeof(base_br_five[0]) },
    { "remainder-eight", remainder_eight,
      sizeof(remainder_eight) / sizeof(remainder_eight[0]) },
};

static const Transcript *find_transcript(const char *name) {
    size_t index;
    for (index = 0U;
         index < sizeof(transcripts) / sizeof(transcripts[0]); ++index) {
        if (strcmp(name, transcripts[index].name) == 0) {
            return &transcripts[index];
        }
    }
    return 0;
}

static uint16_t *cdf_for(unsigned int alphabet, uint16_t *adaptive) {
    static uint16_t cdf2[] = { 16384U, 0U };
    static uint16_t cdf5[] = { 30000U, 22000U, 14000U, 6000U, 0U };
    static uint16_t cdf8[] = {
        31500U, 28000U, 23500U, 18500U, 13000U, 8000U, 3500U, 0U
    };
    if (alphabet == 2U) return cdf2;
    if (alphabet == 3U) return adaptive;
    if (alphabet == 5U) return cdf5;
    if (alphabet == 8U) return cdf8;
    return 0;
}

int main(int argc, char **argv) {
    uint8_t storage[64] = { 0U };
    uint16_t matrix_cdf[ENC_CDF_MAX_SIZE];
    uint16_t adaptive[] = { 24576U, 8192U, 0U, 0U, 0U, 1U, 2U };
    size_t tells[ENC_CDF_MAX_SYMBOLS * 2U] = { 0U };
    const Transcript *transcript;
    EncRangeWriter writer;
    EncStatus status = ENC_OK;
    size_t capacity;
    size_t cdf_size = 4U;
    size_t operation_count;
    size_t index;
    unsigned int matrix_alphabet = 0U;
    unsigned int matrix_count = 0U;
    int matrix = 0;

    if (argc == 5 && strcmp(argv[1], "matrix") == 0) {
        matrix = 1;
        matrix_alphabet = (unsigned int)strtoul(argv[2], 0, 10);
        matrix_count = (unsigned int)strtoul(argv[3], 0, 10);
        capacity = (size_t)strtoul(argv[4], 0, 10);
        if (matrix_alphabet < 2U ||
            matrix_alphabet > ENC_CDF_MAX_SYMBOLS || matrix_count > 32U) {
            fprintf(stderr, "invalid matrix alphabet or count\n");
            return 2;
        }
        operation_count = (size_t)matrix_alphabet * 2U;
        cdf_size = (size_t)matrix_alphabet + 4U;
        for (index = 0U; index < matrix_alphabet; ++index) {
            matrix_cdf[index] = (uint16_t)(
                ((matrix_alphabet - (unsigned int)index - 1U) * 32768U) /
                matrix_alphabet);
        }
        matrix_cdf[matrix_alphabet] = (uint16_t)matrix_count;
        matrix_cdf[matrix_alphabet + 1U] =
            (uint16_t)(matrix_alphabet < 4U ? 2U : 3U);
        matrix_cdf[matrix_alphabet + 2U] =
            (uint16_t)(matrix_cdf[matrix_alphabet + 1U] + 1U);
        matrix_cdf[matrix_alphabet + 3U] =
            (uint16_t)(matrix_cdf[matrix_alphabet + 1U] + 2U);
        transcript = 0;
    } else if (argc == 3) {
        transcript = find_transcript(argv[1]);
        capacity = (size_t)strtoul(argv[2], 0, 10);
        operation_count = transcript == 0 ? 0U : transcript->operation_count;
    } else {
        fprintf(stderr,
                "usage: m2-transcript-driver NAME CAPACITY\n"
                "   or: m2-transcript-driver matrix ALPHABET COUNT CAPACITY\n");
        return 2;
    }
    if ((!matrix && transcript == 0) || capacity > sizeof(storage)) {
        fprintf(stderr, "invalid transcript or capacity\n");
        return 2;
    }

    enc_range_writer_init(&writer, storage, capacity);
    for (index = 0U; index < operation_count; ++index) {
        if (matrix) {
            unsigned int symbol = index < matrix_alphabet
                ? (unsigned int)index
                : matrix_alphabet * 2U - (unsigned int)index - 1U;
            status = enc_range_writer_symbol(
                &writer, symbol, matrix_cdf, matrix_alphabet, 1);
        } else {
            const Operation *operation = &transcript->operations[index];
            if (operation->kind == OP_SYMBOL) {
                status = enc_range_writer_symbol(
                    &writer, operation->value,
                    cdf_for(operation->argument, adaptive),
                    operation->argument, operation->argument == 3U);
            } else if (operation->kind == OP_BYPASS) {
                status = enc_range_writer_bypass(&writer, operation->value);
            } else {
                status = enc_range_writer_literal(
                    &writer, operation->value, operation->argument);
            }
        }
        tells[index] = enc_range_writer_tell(&writer);
        if (status != ENC_OK) break;
    }
    if (status == ENC_OK) status = enc_range_writer_finish(&writer);

    printf("{\"status\":%u,\"bytes_used\":%zu,\"tell\":%zu,\"hex\":\"",
           (unsigned int)status, enc_range_writer_bytes_used(&writer),
           enc_range_writer_tell(&writer));
    for (index = 0U; index < enc_range_writer_bytes_used(&writer); ++index) {
        printf("%02x", storage[index]);
    }
    printf("\",\"tells\":[");
    for (index = 0U; index < operation_count; ++index) {
        if (index != 0U) putchar(',');
        printf("%zu", tells[index]);
    }
    printf("],\"cdf\":[");
    for (index = 0U; index < cdf_size; ++index) {
        const uint16_t *cdf = matrix ? matrix_cdf : adaptive;
        if (index != 0U) putchar(',');
        printf("%u", cdf[index]);
    }
    printf("]}\n");
    return 0;
}

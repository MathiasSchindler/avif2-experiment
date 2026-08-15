#ifndef EXPERIMENTAL_ENCODER_CLI_INPUT_H
#define EXPERIMENTAL_ENCODER_CLI_INPUT_H

#include "encoder/platform.h"

#define ENC_CLI_PATH_CAPACITY 4096U

typedef enum {
    ENC_CLI_INPUT_UNKNOWN = 0,
    ENC_CLI_INPUT_I400,
    ENC_CLI_INPUT_I420,
    ENC_CLI_INPUT_I444,
    ENC_CLI_INPUT_PNG,
    ENC_CLI_INPUT_BMP
} EncCliInputFormat;

typedef enum {
    ENC_CLI_CHROMA_I400 = 0,
    ENC_CLI_CHROMA_I420,
    ENC_CLI_CHROMA_I444
} EncCliChroma;

typedef enum {
    ENC_CLI_INPUT_ISSUE_NONE = 0,
    ENC_CLI_INPUT_ISSUE_UNKNOWN_FORMAT,
    ENC_CLI_INPUT_ISSUE_CONTENT_MISMATCH,
    ENC_CLI_INPUT_ISSUE_METADATA_MISSING,
    ENC_CLI_INPUT_ISSUE_METADATA_AMBIGUOUS,
    ENC_CLI_INPUT_ISSUE_METADATA_CONFLICT
} EncCliInputIssue;

typedef struct {
    EncCliInputFormat format;
    EncCliChroma chroma;
    size_t width;
    size_t height;
} EncCliInputContract;

EncStatus enc_cli_input_discover(const char *path,
                                 EncCliInputContract *contract,
                                 EncCliInputIssue *issue);
EncStatus enc_cli_default_output(const char *path, char *output,
                                 size_t capacity);

#endif

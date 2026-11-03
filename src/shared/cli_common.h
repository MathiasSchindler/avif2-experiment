#ifndef AV2H_SHARED_CLI_COMMON_H
#define AV2H_SHARED_CLI_COMMON_H

#include "decoder/frame_output.h"
#include "shared/png_writer.h"

#define AV2H_CLI_MAX_INPUT (256U * 1024U * 1024U)

typedef struct {
    void *memory;
    size_t size;
} Av2hOwnedMemory;

typedef struct {
    void *context;
    int (*allocate)(void *context, size_t size, size_t alignment, Av2hOwnedMemory *owned);
    void (*release)(void *context, Av2hOwnedMemory *owned);
} Av2hCliAllocator;

typedef struct {
    const Av2hExecution *execution;
    uint8_t validate_format_first;
    uint8_t retain_trace;
} Av2hCliDecodeOptions;

typedef struct {
    Av2hOwnedMemory output;
    Av2hOwnedMemory diagnostic_memory;
    uint16_t width;
    uint16_t height;
    uint8_t chroma_format_idc;
    Av2hPixelFormat format;
    Av2hFrameDecodeResult result;
    Av2hError error;
} Av2hCliImage;

typedef struct {
    const char *text;
    const uint16_t *wide;
} Av2hCliPath;

typedef enum {
    AV2H_CLI_OUTPUT_INVALID = 0,
    AV2H_CLI_OUTPUT_RAW,
    AV2H_CLI_OUTPUT_BMP,
    AV2H_CLI_OUTPUT_PNG
} Av2hCliOutputKind;

typedef struct {
    Av2hCliOutputKind kind;
    const char *raw_extension;
} Av2hCliOutput;

typedef struct {
    void *context;
    Av2hStatus (*open)(void *context);
    Av2hPngWriteCallback write;
    Av2hStatus (*close)(void *context);
} Av2hCliFile;

typedef enum {
    AV2H_CLI_WRITE_OK = 0,
    AV2H_CLI_WRITE_FAILED,
    AV2H_CLI_WRITE_RAW_MISMATCH,
    AV2H_CLI_WRITE_PREVIEW_DEPTH
} Av2hCliWriteResult;

typedef enum {
    AV2H_CLI_DIAGNOSTIC_COMPACT = 0,
    AV2H_CLI_DIAGNOSTIC_STATUS,
    AV2H_CLI_DIAGNOSTIC_TRACE
} Av2hCliDiagnosticStyle;

typedef enum {
    AV2H_CLI_MESSAGE_READ_INPUT = 0,
    AV2H_CLI_MESSAGE_WORKERS,
    AV2H_CLI_MESSAGE_COMMAND_LINE,
    AV2H_CLI_MESSAGE_DIAGNOSTICS
} Av2hCliMessage;

Av2hStatus av2h_cli_decode(Av2hSpan input, const Av2hCliAllocator *allocator,
                           const Av2hCliDecodeOptions *options, Av2hCliImage *image);
void av2h_cli_image_release(const Av2hCliAllocator *allocator, Av2hCliImage *image);

Av2hStatus av2h_cli_output_parse(Av2hCliPath path, Av2hCliOutput *output);
Av2hCliWriteResult av2h_cli_write_output(const Av2hCliImage *image, const Av2hCliOutput *output,
                                         const Av2hCliAllocator *allocator,
                                         const Av2hCliFile *file);
int av2h_cli_write_exit_code(Av2hCliWriteResult result);

Av2hStatus av2h_cli_write_usage(const char *program, int diagnostics_supported,
                                Av2hCliDiagnosticStyle style, Av2hPngWriteCallback write,
                                void *context);
Av2hStatus av2h_cli_write_message(Av2hCliMessage message, Av2hCliDiagnosticStyle style,
                                  Av2hPngWriteCallback write, void *context);
Av2hStatus av2h_cli_write_decode_error(const Av2hCliImage *image, Av2hStatus status,
                                       Av2hCliDiagnosticStyle style, Av2hPngWriteCallback write,
                                       void *context);
Av2hStatus av2h_cli_write_output_error(Av2hCliWriteResult result, Av2hCliDiagnosticStyle style,
                                       Av2hPngWriteCallback write, void *context);
Av2hStatus av2h_cli_write_json(const Av2hCliImage *image, Av2hPngWriteCallback write,
                               void *context);
Av2hStatus av2h_cli_write_trace(const Av2hFrameDecodeResult *result, Av2hPngWriteCallback write,
                                void *context);

#endif

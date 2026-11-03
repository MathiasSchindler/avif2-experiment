#include "shared/cli_common.h"

#include "platform/windows/win32_min.h"

#include <stddef.h>

#define AV2H_CLI_ARGUMENT_CAPACITY 4

typedef struct {
    const Av2hWinChar *path;
    Av2hWinHandle handle;
} Av2hWindowsFile;

static size_t wide_length(const Av2hWinChar *text) {
    size_t length = 0U;
    while (text[length] != 0U)
        ++length;
    return length;
}

static int ascii_equal(const Av2hWinChar *wide, const char *ascii) {
    size_t index = 0U;
    while (wide[index] != 0U && ascii[index] != '\0') {
        if (wide[index] != (uint8_t)ascii[index])
            return 0;
        ++index;
    }
    return wide[index] == 0U && ascii[index] == '\0';
}

static int parse_arguments(Av2hWinChar *text, Av2hWinChar **arguments, int capacity) {
    Av2hWinChar *read = text;
    Av2hWinChar *write = text;
    int count = 0;
    while (*read != 0U) {
        int quoted = 0;
        while (*read == ' ' || *read == '\t')
            ++read;
        if (*read == 0U)
            break;
        if (count == capacity)
            return -1;
        arguments[count++] = write;
        while (*read != 0U) {
            if (*read == '"') {
                quoted = !quoted;
                ++read;
                continue;
            }
            if (!quoted && (*read == ' ' || *read == '\t'))
                break;
            *write++ = *read++;
        }
        if (quoted)
            return -1;
        if (*read != 0U)
            ++read;
        *write++ = 0U;
        while (*read == ' ' || *read == '\t')
            ++read;
    }
    return count;
}

static void release_memory(void *context, Av2hOwnedMemory *owned) {
    (void)context;
    if (owned->memory != NULL) {
        (void)VirtualFree(owned->memory, 0U, AV2H_WIN_MEM_RELEASE);
    }
    owned->memory = NULL;
    owned->size = 0U;
}

static int allocate_memory(void *context, size_t size, size_t alignment, Av2hOwnedMemory *owned) {
    (void)context;
    if (alignment > 65536U)
        return 0;
    owned->memory =
        VirtualAlloc(NULL, size == 0U ? 1U : size, AV2H_WIN_MEM_RESERVE | AV2H_WIN_MEM_COMMIT,
                     AV2H_WIN_PAGE_READWRITE);
    owned->size = owned->memory != NULL ? size : 0U;
    return owned->memory != NULL;
}

static int read_entire_file(const Av2hWinChar *path, Av2hOwnedMemory *owned) {
    Av2hWinHandle file = CreateFileW(path, AV2H_WIN_GENERIC_READ, AV2H_WIN_FILE_SHARE_READ, NULL,
                                     AV2H_WIN_OPEN_EXISTING, AV2H_WIN_FILE_ATTRIBUTE_NORMAL, NULL);
    Av2hWinLargeInteger size;
    Av2hWinDword read = 0U;
    int result = 0;
    if (file == AV2H_WIN_INVALID_HANDLE)
        return 0;
    if (!GetFileSizeEx(file, &size) || size.quad <= 0 || (uint64_t)size.quad > AV2H_CLI_MAX_INPUT ||
        !allocate_memory(NULL, (size_t)size.quad, sizeof(void *), owned)) {
        (void)CloseHandle(file);
        return 0;
    }
    if (ReadFile(file, owned->memory, (Av2hWinDword)owned->size, &read, NULL) &&
        read == owned->size) {
        result = 1;
    }
    (void)CloseHandle(file);
    if (!result)
        release_memory(NULL, owned);
    return result;
}

static int write_all(Av2hWinHandle file, const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    while (size != 0U) {
        Av2hWinDword chunk = size > UINT32_MAX ? UINT32_MAX : (Av2hWinDword)size;
        Av2hWinDword written = 0U;
        if (!WriteFile(file, bytes, chunk, &written, NULL) || written != chunk) {
            return 0;
        }
        bytes += chunk;
        size -= chunk;
    }
    return 1;
}

static Av2hStatus write_console(void *context, const uint8_t *data, size_t size) {
    Av2hWinHandle error = GetStdHandle(AV2H_WIN_STD_ERROR_HANDLE);
    (void)context;
    if (error == NULL || error == AV2H_WIN_INVALID_HANDLE) {
        return AV2H_INVALID_DATA;
    }
    return write_all(error, data, size) ? AV2H_OK : AV2H_INVALID_DATA;
}

static Av2hStatus open_output(void *context) {
    Av2hWindowsFile *file = context;
    file->handle = CreateFileW(file->path, AV2H_WIN_GENERIC_WRITE, 0U, NULL, AV2H_WIN_CREATE_ALWAYS,
                               AV2H_WIN_FILE_ATTRIBUTE_NORMAL, NULL);
    return file->handle != AV2H_WIN_INVALID_HANDLE ? AV2H_OK : AV2H_INVALID_DATA;
}

static Av2hStatus write_output(void *context, const uint8_t *data, size_t size) {
    Av2hWindowsFile *file = context;
    return write_all(file->handle, data, size) ? AV2H_OK : AV2H_INVALID_DATA;
}

static Av2hStatus close_output(void *context) {
    Av2hWindowsFile *file = context;
    int result = CloseHandle(file->handle);
    file->handle = AV2H_WIN_INVALID_HANDLE;
    return result ? AV2H_OK : AV2H_INVALID_DATA;
}

static unsigned int run_decoder(void) {
    const Av2hWinChar *system_command_line = GetCommandLineW();
    size_t command_line_units = wide_length(system_command_line) + 1U;
    Av2hOwnedMemory command_line = {NULL, 0U};
    Av2hOwnedMemory input = {NULL, 0U};
    Av2hCliAllocator allocator = {NULL, allocate_memory, release_memory};
    Av2hCliDecodeOptions options = {NULL, 1, 1};
    Av2hCliImage image;
    Av2hCliOutput output;
    Av2hWindowsFile output_file;
    Av2hCliFile file = {&output_file, open_output, write_output, close_output};
    Av2hWinChar *arguments[AV2H_CLI_ARGUMENT_CAPACITY];
    Av2hStatus status;
    Av2hCliWriteResult result;
    int argument_count;
    unsigned int exit_code;
    if (!allocate_memory(NULL, command_line_units * sizeof(Av2hWinChar), sizeof(void *),
                         &command_line)) {
        (void)av2h_cli_write_message(AV2H_CLI_MESSAGE_COMMAND_LINE, AV2H_CLI_DIAGNOSTIC_TRACE,
                                     write_console, NULL);
        return 1U;
    }
    av2h_memory_copy(command_line.memory, system_command_line,
                     command_line_units * sizeof(Av2hWinChar));
    argument_count = parse_arguments(command_line.memory, arguments, AV2H_CLI_ARGUMENT_CAPACITY);
    if (argument_count != 4 || !ascii_equal(arguments[2], "-o") ||
        av2h_cli_output_parse((Av2hCliPath){NULL, arguments[3]}, &output) != AV2H_OK) {
        exit_code = av2h_cli_write_usage("av2heif-decoder.exe", 0, AV2H_CLI_DIAGNOSTIC_TRACE,
                                         write_console, NULL) == AV2H_OK
                        ? 2U
                        : 1U;
        release_memory(NULL, &command_line);
        return exit_code;
    }
    if (!read_entire_file(arguments[1], &input)) {
        (void)av2h_cli_write_message(AV2H_CLI_MESSAGE_READ_INPUT, AV2H_CLI_DIAGNOSTIC_TRACE,
                                     write_console, NULL);
        release_memory(NULL, &command_line);
        return 1U;
    }
    status = av2h_cli_decode((Av2hSpan){input.memory, input.size}, &allocator, &options, &image);
    if (status != AV2H_OK) {
        int reported = av2h_cli_write_decode_error(&image, status, AV2H_CLI_DIAGNOSTIC_TRACE,
                                                   write_console, NULL) == AV2H_OK;
        exit_code = reported && status == AV2H_UNSUPPORTED ? 3U : 1U;
    } else if (av2h_cli_write_trace(&image.result, write_console, NULL) != AV2H_OK) {
        exit_code = 1U;
    } else {
        release_memory(NULL, &image.diagnostic_memory);
        image.result.diagnostics = NULL;
        output_file.path = arguments[3];
        output_file.handle = AV2H_WIN_INVALID_HANDLE;
        result = av2h_cli_write_output(&image, &output, &allocator, &file);
        exit_code = (unsigned int)av2h_cli_write_exit_code(result);
        if (av2h_cli_write_output_error(result, AV2H_CLI_DIAGNOSTIC_TRACE, write_console, NULL) !=
            AV2H_OK)
            exit_code = 1U;
    }
    av2h_cli_image_release(&allocator, &image);
    release_memory(NULL, &input);
    release_memory(NULL, &command_line);
    return exit_code;
}

__declspec(noreturn) void mainCRTStartup(void) {
    ExitProcess(run_decoder());
}

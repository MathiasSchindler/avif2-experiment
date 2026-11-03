#include "shared/cli_common.h"

#if defined(__APPLE__) && defined(__aarch64__)
#include "platform/macos/macos_abi.h"
#else
#include "platform/linux/linux_abi.h"
#endif
#include "runtime/concurrency.h"

#include <stddef.h>

#if defined(__APPLE__) && defined(__aarch64__)
#define AV2H_ABI_EINTR AV2H_MACOS_EINTR
#define AV2H_ABI_PAGE_SIZE AV2H_MACOS_PAGE_SIZE
#define AV2H_ABI_SYS_WRITE AV2H_MACOS_SYS_WRITE
#define AV2H_ABI_SYS_READ AV2H_MACOS_SYS_READ
#define AV2H_ABI_SYS_CLOSE AV2H_MACOS_SYS_CLOSE
#define AV2H_ABI_SYS_MMAP AV2H_MACOS_SYS_MMAP
#define AV2H_ABI_SYS_MUNMAP AV2H_MACOS_SYS_MUNMAP
#define AV2H_ABI_SYS_LSEEK AV2H_MACOS_SYS_LSEEK
#define AV2H_ABI_PROT_READ AV2H_MACOS_PROT_READ
#define AV2H_ABI_PROT_WRITE AV2H_MACOS_PROT_WRITE
#define AV2H_ABI_MAP_PRIVATE AV2H_MACOS_MAP_PRIVATE
#define AV2H_ABI_MAP_ANONYMOUS AV2H_MACOS_MAP_ANONYMOUS
#define AV2H_ABI_O_RDONLY AV2H_MACOS_O_RDONLY
#define AV2H_ABI_O_WRONLY AV2H_MACOS_O_WRONLY
#define AV2H_ABI_O_CREAT AV2H_MACOS_O_CREAT
#define AV2H_ABI_O_TRUNC AV2H_MACOS_O_TRUNC
#define AV2H_ABI_O_CLOEXEC AV2H_MACOS_O_CLOEXEC
#define AV2H_ABI_SEEK_SET AV2H_MACOS_SEEK_SET
#define AV2H_ABI_SEEK_END AV2H_MACOS_SEEK_END
#define av2h_abi_syscall1 av2h_macos_syscall1
#define av2h_abi_syscall2 av2h_macos_syscall2
#define av2h_abi_syscall3 av2h_macos_syscall3
#define av2h_abi_syscall6 av2h_macos_syscall6
#define av2h_abi_open(path, flags, mode) \
    av2h_macos_syscall3(AV2H_MACOS_SYS_OPEN, (long)(path), (flags), (mode))
#else
#define AV2H_ABI_EINTR 4
#define AV2H_ABI_PAGE_SIZE 4096U
#define AV2H_ABI_SYS_WRITE AV2H_LINUX_SYS_WRITE
#define AV2H_ABI_SYS_READ AV2H_LINUX_SYS_READ
#define AV2H_ABI_SYS_CLOSE AV2H_LINUX_SYS_CLOSE
#define AV2H_ABI_SYS_MMAP AV2H_LINUX_SYS_MMAP
#define AV2H_ABI_SYS_MUNMAP AV2H_LINUX_SYS_MUNMAP
#define AV2H_ABI_SYS_LSEEK AV2H_LINUX_SYS_LSEEK
#define AV2H_ABI_PROT_READ AV2H_LINUX_PROT_READ
#define AV2H_ABI_PROT_WRITE AV2H_LINUX_PROT_WRITE
#define AV2H_ABI_MAP_PRIVATE AV2H_LINUX_MAP_PRIVATE
#define AV2H_ABI_MAP_ANONYMOUS AV2H_LINUX_MAP_ANONYMOUS
#define AV2H_ABI_O_RDONLY AV2H_LINUX_O_RDONLY
#define AV2H_ABI_O_WRONLY AV2H_LINUX_O_WRONLY
#define AV2H_ABI_O_CREAT AV2H_LINUX_O_CREAT
#define AV2H_ABI_O_TRUNC AV2H_LINUX_O_TRUNC
#define AV2H_ABI_O_CLOEXEC AV2H_LINUX_O_CLOEXEC
#define AV2H_ABI_SEEK_SET AV2H_LINUX_SEEK_SET
#define AV2H_ABI_SEEK_END AV2H_LINUX_SEEK_END
#define av2h_abi_syscall1 av2h_linux_syscall1
#define av2h_abi_syscall2 av2h_linux_syscall2
#define av2h_abi_syscall3 av2h_linux_syscall3
#define av2h_abi_syscall6 av2h_linux_syscall6
#define av2h_abi_open(path, flags, mode) \
    av2h_linux_syscall4(AV2H_LINUX_SYS_OPENAT, AV2H_LINUX_AT_FDCWD, \
                        (long)(path), (flags), (mode))
#endif
typedef struct {
    const char *path;
    int descriptor;
} Av2hSystemFile;

static int text_equal(const char *left, const char *right) {
    size_t index = 0U;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return 0;
        ++index;
    }
    return left[index] == right[index];
}

static void release_memory(void *context, Av2hOwnedMemory *owned) {
    (void)context;
    if (owned->memory != NULL && owned->size != 0U) {
        (void)av2h_abi_syscall2(
            AV2H_ABI_SYS_MUNMAP, (long)owned->memory, (long)owned->size);
    }
    owned->memory = NULL;
    owned->size = 0U;
}

static int allocate_memory(void *context, size_t size,
                           size_t alignment,
                           Av2hOwnedMemory *owned) {
    long mapped;
    (void)context;
    if (size == 0U || alignment > AV2H_ABI_PAGE_SIZE) return 0;
    mapped = av2h_abi_syscall6(
        AV2H_ABI_SYS_MMAP, 0, (long)size,
        AV2H_ABI_PROT_READ | AV2H_ABI_PROT_WRITE,
        AV2H_ABI_MAP_PRIVATE | AV2H_ABI_MAP_ANONYMOUS, -1, 0);
    if (mapped < 0) return 0;
    owned->memory = (void *)mapped;
    owned->size = size;
    return 1;
}

static int close_file(int descriptor) {
    long result;
    do {
        result = av2h_abi_syscall1(AV2H_ABI_SYS_CLOSE, descriptor);
    } while (result == -AV2H_ABI_EINTR);
    return result == 0;
}

static int read_all(int descriptor, void *data, size_t size) {
    uint8_t *cursor = (uint8_t *)data;
    while (size != 0U) {
        long count = av2h_abi_syscall3(
            AV2H_ABI_SYS_READ, descriptor, (long)cursor, (long)size);
        if (count == -AV2H_ABI_EINTR) continue;
        if (count <= 0) return 0;
        cursor += (size_t)count;
        size -= (size_t)count;
    }
    return 1;
}

static int write_all(int descriptor, const void *data, size_t size) {
    const uint8_t *cursor = (const uint8_t *)data;
    while (size != 0U) {
        long count = av2h_abi_syscall3(
            AV2H_ABI_SYS_WRITE, descriptor, (long)cursor, (long)size);
        if (count == -AV2H_ABI_EINTR) continue;
        if (count <= 0) return 0;
        cursor += (size_t)count;
        size -= (size_t)count;
    }
    return 1;
}

static int open_read(const char *path) {
    long descriptor =
        av2h_abi_open(path, AV2H_ABI_O_RDONLY | AV2H_ABI_O_CLOEXEC, 0);
    return descriptor < 0 ? -1 : (int)descriptor;
}

static int open_write(const char *path) {
    long descriptor = av2h_abi_open(
        path,
        AV2H_ABI_O_WRONLY | AV2H_ABI_O_CREAT |
            AV2H_ABI_O_TRUNC | AV2H_ABI_O_CLOEXEC,
        0666);
    return descriptor < 0 ? -1 : (int)descriptor;
}

static int read_entire_file(const char *path, Av2hOwnedMemory *owned) {
    int descriptor = open_read(path);
    long length;
    int result;
    if (descriptor < 0) return 0;
    length = av2h_abi_syscall3(
        AV2H_ABI_SYS_LSEEK, descriptor, 0, AV2H_ABI_SEEK_END);
    if (length <= 0 || (unsigned long)length > AV2H_CLI_MAX_INPUT ||
        av2h_abi_syscall3(
            AV2H_ABI_SYS_LSEEK, descriptor, 0,
            AV2H_ABI_SEEK_SET) != 0 ||
        !allocate_memory(NULL, (size_t)length, sizeof(void *), owned)) {
        (void)close_file(descriptor);
        return 0;
    }
    result = read_all(descriptor, owned->memory, owned->size);
    if (!close_file(descriptor)) result = 0;
    if (!result) release_memory(NULL, owned);
    return result;
}

static Av2hStatus write_console(
    void *context, const uint8_t *data, size_t size) {
    (void)context;
    return write_all(2, data, size) ? AV2H_OK : AV2H_INVALID_DATA;
}

static Av2hStatus open_output(void *context) {
    Av2hSystemFile *file = context;
    file->descriptor = open_write(file->path);
    return file->descriptor >= 0 ? AV2H_OK : AV2H_INVALID_DATA;
}

static Av2hStatus write_output(
    void *context, const uint8_t *data, size_t size) {
    Av2hSystemFile *file = context;
    return write_all(file->descriptor, data, size) ?
        AV2H_OK : AV2H_INVALID_DATA;
}

static Av2hStatus close_output(void *context) {
    Av2hSystemFile *file = context;
    int result = close_file(file->descriptor);
    file->descriptor = -1;
    return result ? AV2H_OK : AV2H_INVALID_DATA;
}

int main(int argc, char **argv) {
    Av2hOwnedMemory input = { NULL, 0U };
    Av2hCliAllocator allocator = { NULL, allocate_memory, release_memory };
    Av2hCliDecodeOptions options = { NULL, 1, 0 };
    Av2hCliImage image;
    Av2hCliOutput output;
    Av2hSystemFile output_file;
    Av2hCliFile file = {
        &output_file, open_output, write_output, close_output
    };
    Av2hTaskPool task_pool;
    Av2hExecution execution;
    Av2hStatus status;
    Av2hCliWriteResult result;
    int exit_code;
    if (argc != 4 || !text_equal(argv[2], "-o") ||
        av2h_cli_output_parse(
            (Av2hCliPath){ argv[3], NULL }, &output) != AV2H_OK) {
        return av2h_cli_write_usage(
            argv[0], 0, AV2H_CLI_DIAGNOSTIC_STATUS,
            write_console, NULL) == AV2H_OK ? 2 : 1;
    }
    if (!read_entire_file(argv[1], &input)) {
        (void)av2h_cli_write_message(
            AV2H_CLI_MESSAGE_READ_INPUT, AV2H_CLI_DIAGNOSTIC_STATUS,
            write_console, NULL);
        return 1;
    }
    if (av2h_task_pool_init(&task_pool, 0U) != 0) {
        release_memory(NULL, &input);
        (void)av2h_cli_write_message(
            AV2H_CLI_MESSAGE_WORKERS, AV2H_CLI_DIAGNOSTIC_STATUS,
            write_console, NULL);
        return 1;
    }
    av2h_task_pool_execution(&task_pool, &execution);
    options.execution = &execution;
    status = av2h_cli_decode(
        (Av2hSpan){ input.memory, input.size }, &allocator, &options, &image);
    av2h_task_pool_destroy(&task_pool);
    if (status != AV2H_OK) {
        int reported = av2h_cli_write_decode_error(
            &image, status, AV2H_CLI_DIAGNOSTIC_STATUS,
            write_console, NULL) == AV2H_OK;
        av2h_cli_image_release(&allocator, &image);
        release_memory(NULL, &input);
        return reported && status == AV2H_UNSUPPORTED ? 3 : 1;
    }
    output_file.path = argv[3];
    output_file.descriptor = -1;
    result = av2h_cli_write_output(&image, &output, &allocator, &file);
    exit_code = av2h_cli_write_exit_code(result);
    if (av2h_cli_write_output_error(
            result, AV2H_CLI_DIAGNOSTIC_STATUS,
            write_console, NULL) != AV2H_OK) exit_code = 1;
    av2h_cli_image_release(&allocator, &image);
    release_memory(NULL, &input);
    return exit_code;
}

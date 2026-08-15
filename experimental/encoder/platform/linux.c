#include "encoder/platform.h"

#define ENC_AT_FDCWD (-100)
#define ENC_O_RDONLY 0
#define ENC_O_WRONLY 1
#define ENC_O_CREAT 0100
#define ENC_O_EXCL 0200
#define ENC_O_CLOEXEC 02000000
#define ENC_EINTR 4
#define ENC_EEXIST 17

#define ENC_TEMP_PATH_CAPACITY 4096U
#define ENC_TEMP_ATTEMPTS 128U

#define ENC_SYS_READ 0
#define ENC_SYS_WRITE 1
#define ENC_SYS_CLOSE 3
#define ENC_SYS_FSTAT 5
#define ENC_SYS_MMAP 9
#define ENC_SYS_MUNMAP 11
#define ENC_SYS_GETPID 39
#define ENC_SYS_EXIT 60
#define ENC_SYS_OPENAT 257
#define ENC_SYS_UNLINKAT 263
#define ENC_SYS_RENAMEAT 264

#define ENC_PROT_READ 1
#define ENC_MAP_PRIVATE 2

typedef struct {
    unsigned long device;
    unsigned long inode;
    unsigned long link_count;
    unsigned int mode;
    unsigned int user;
    unsigned int group;
    unsigned int padding;
    unsigned long special_device;
    long size;
    long block_size;
    long block_count;
    long access_time;
    unsigned long access_time_nanoseconds;
    long modification_time;
    unsigned long modification_time_nanoseconds;
    long change_time;
    unsigned long change_time_nanoseconds;
    long unused[3];
} EncLinuxStat;

static long enc_syscall1(long number, long argument0) {
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(argument0)
                     : "rcx", "r11", "memory");
    return result;
}

static long enc_syscall2(long number, long argument0, long argument1) {
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(argument0), "S"(argument1)
                     : "rcx", "r11", "memory");
    return result;
}

static long enc_syscall3(long number, long argument0, long argument1,
                         long argument2) {
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(argument0), "S"(argument1),
                       "d"(argument2)
                     : "rcx", "r11", "memory");
    return result;
}

static long enc_syscall4(long number, long argument0, long argument1,
                         long argument2, long argument3) {
    register long r10 __asm__("r10") = argument3;
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(argument0), "S"(argument1),
                       "d"(argument2), "r"(r10)
                     : "rcx", "r11", "memory");
    return result;
}

static long enc_syscall6(long number, long argument0, long argument1,
                         long argument2, long argument3, long argument4,
                         long argument5) {
    register long r10 __asm__("r10") = argument3;
    register long r8 __asm__("r8") = argument4;
    register long r9 __asm__("r9") = argument5;
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(argument0), "S"(argument1),
                       "d"(argument2), "r"(r10), "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory");
    return result;
}

static EncStatus enc_file_open(const char *path, long flags, long mode,
                               EncFile *file) {
    long result;
    if (path == 0 || file == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    result = enc_syscall4(ENC_SYS_OPENAT, ENC_AT_FDCWD, (long)path, flags, mode);
    if (result < 0) {
        file->descriptor = -1;
        return ENC_IO_ERROR;
    }
    file->descriptor = (int)result;
    return ENC_OK;
}

EncStatus enc_file_open_read(const char *path, EncFile *file) {
    return enc_file_open(path, ENC_O_RDONLY | ENC_O_CLOEXEC, 0, file);
}

EncStatus enc_file_read(EncFile *file, void *buffer, size_t capacity,
                        size_t *bytes_read) {
    long result;
    if (file == 0 || file->descriptor < 0 || buffer == 0 || bytes_read == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    do {
        result = enc_syscall3(ENC_SYS_READ, file->descriptor, (long)buffer,
                              (long)capacity);
    } while (result == -ENC_EINTR);
    if (result < 0) {
        return ENC_IO_ERROR;
    }
    *bytes_read = (size_t)result;
    return ENC_OK;
}

EncStatus enc_file_write_all(EncFile *file, const void *buffer, size_t size) {
    const uint8_t *bytes = (const uint8_t *)buffer;
    size_t position = 0;
    if (file == 0 || file->descriptor < 0 || (buffer == 0 && size != 0)) {
        return ENC_INVALID_ARGUMENT;
    }
    while (position < size) {
        long result = enc_syscall3(ENC_SYS_WRITE, file->descriptor,
                                   (long)(bytes + position),
                                   (long)(size - position));
        if (result == -ENC_EINTR) {
            continue;
        }
        if (result <= 0) {
            return ENC_IO_ERROR;
        }
        position += (size_t)result;
    }
    return ENC_OK;
}

EncStatus enc_file_close(EncFile *file) {
    long result;
    if (file == 0 || file->descriptor < 0) {
        return ENC_INVALID_ARGUMENT;
    }
    result = enc_syscall1(ENC_SYS_CLOSE, file->descriptor);
    file->descriptor = -1;
    return result < 0 ? ENC_IO_ERROR : ENC_OK;
}

static int enc_append_decimal(char *path, size_t capacity, size_t *position,
                              unsigned long value) {
    char digits[24U];
    size_t used = 0U;
    do {
        digits[used++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    if (*position + used >= capacity) {
        return 0;
    }
    while (used != 0U) {
        path[(*position)++] = digits[--used];
    }
    return 1;
}

static int enc_make_temp_path(const char *path, unsigned long process,
                              unsigned long counter, char *temporary,
                              size_t capacity) {
    static const char suffix[] = ".tmp.";
    size_t position = 0U;
    size_t suffix_position;
    if (path == 0 || path[0] == '\0' || temporary == 0) {
        return 0;
    }
    while (path[position] != '\0') {
        if (position + 1U >= capacity) {
            return 0;
        }
        temporary[position] = path[position];
        ++position;
    }
    for (suffix_position = 0U;
         suffix_position + 1U < sizeof(suffix);
         ++suffix_position) {
        if (position + 1U >= capacity) {
            return 0;
        }
        temporary[position++] = suffix[suffix_position];
    }
    if (!enc_append_decimal(temporary, capacity, &position, process) ||
        position + 1U >= capacity) {
        return 0;
    }
    temporary[position++] = '.';
    if (!enc_append_decimal(temporary, capacity, &position, counter)) {
        return 0;
    }
    temporary[position] = '\0';
    return 1;
}

EncStatus enc_file_write_atomic(const char *path, const void *buffer,
                                size_t size) {
    static unsigned long next_counter;
    char temporary[ENC_TEMP_PATH_CAPACITY];
    unsigned long process;
    unsigned int attempt;
    EncFile file = {-1};
    EncStatus status;
    long result;
    if (path == 0 || path[0] == '\0' || (buffer == 0 && size != 0U)) {
        return ENC_INVALID_ARGUMENT;
    }
    result = enc_syscall1(ENC_SYS_GETPID, 0);
    if (result < 0) {
        return ENC_IO_ERROR;
    }
    process = (unsigned long)result;
    for (attempt = 0U; attempt < ENC_TEMP_ATTEMPTS; ++attempt) {
        unsigned long counter = next_counter++;
        if (!enc_make_temp_path(path, process, counter, temporary,
                                sizeof(temporary))) {
            return ENC_IO_ERROR;
        }
        result = enc_syscall4(ENC_SYS_OPENAT, ENC_AT_FDCWD, (long)temporary,
                              ENC_O_WRONLY | ENC_O_CREAT | ENC_O_EXCL |
                                  ENC_O_CLOEXEC,
                              0644);
        if (result >= 0) {
            file.descriptor = (int)result;
            break;
        }
        if (result != -ENC_EEXIST) {
            return ENC_IO_ERROR;
        }
    }
    if (file.descriptor < 0) {
        return ENC_IO_ERROR;
    }
    status = enc_file_write_all(&file, buffer, size);
    if (enc_file_close(&file) != ENC_OK && status == ENC_OK) {
        status = ENC_IO_ERROR;
    }
    if (status == ENC_OK) {
        result = enc_syscall4(ENC_SYS_RENAMEAT, ENC_AT_FDCWD, (long)temporary,
                              ENC_AT_FDCWD, (long)path);
        if (result < 0) {
            status = ENC_IO_ERROR;
        }
    }
    if (status != ENC_OK) {
        (void)enc_syscall3(ENC_SYS_UNLINKAT, ENC_AT_FDCWD, (long)temporary, 0);
    }
    return status;
}

EncStatus enc_file_map_readonly_exact(const char *path, size_t expected_size,
                                      EncReadMapping *mapping) {
    EncFile file = {-1};
    EncLinuxStat information;
    long result;
    EncStatus close_status;
    if (path == 0 || expected_size == 0U || mapping == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    mapping->data = 0;
    mapping->size = 0U;
    if (enc_file_open_read(path, &file) != ENC_OK) {
        return ENC_IO_ERROR;
    }
    result = enc_syscall2(ENC_SYS_FSTAT, file.descriptor, (long)&information);
    if (result < 0) {
        (void)enc_file_close(&file);
        return ENC_IO_ERROR;
    }
    if (information.size < 0 ||
        (unsigned long)information.size != (unsigned long)expected_size) {
        (void)enc_file_close(&file);
        return ENC_SIZE_MISMATCH;
    }
    result = enc_syscall6(ENC_SYS_MMAP, 0, (long)expected_size, ENC_PROT_READ,
                          ENC_MAP_PRIVATE, file.descriptor, 0);
    close_status = enc_file_close(&file);
    if (result < 0) {
        return ENC_IO_ERROR;
    }
    if (close_status != ENC_OK) {
        (void)enc_syscall2(ENC_SYS_MUNMAP, result, (long)expected_size);
        return close_status;
    }
    mapping->data = (const uint8_t *)result;
    mapping->size = expected_size;
    return ENC_OK;
}

EncStatus enc_file_unmap(EncReadMapping *mapping) {
    long result;
    if (mapping == 0 || mapping->data == 0 || mapping->size == 0U) {
        return ENC_INVALID_ARGUMENT;
    }
    result = enc_syscall2(ENC_SYS_MUNMAP, (long)mapping->data,
                          (long)mapping->size);
    mapping->data = 0;
    mapping->size = 0U;
    return result < 0 ? ENC_IO_ERROR : ENC_OK;
}

EncStatus enc_standard_error_write(const char *message, size_t size) {
    EncFile error = {2};
    return enc_file_write_all(&error, message, size);
}

void enc_process_exit(int status) {
    (void)enc_syscall1(ENC_SYS_EXIT, status);
    for (;;) {
    }
}

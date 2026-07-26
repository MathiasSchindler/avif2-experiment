#ifndef AV2H_WIN32_MIN_H
#define AV2H_WIN32_MIN_H

#include <stddef.h>
#include <stdint.h>

typedef void *Av2hWinHandle;
typedef uint16_t Av2hWinChar;
typedef int Av2hWinBool;
typedef uint32_t Av2hWinDword;

typedef union {
    struct {
        uint32_t low;
        int32_t high;
    } parts;
    int64_t quad;
} Av2hWinLargeInteger;

#define AV2H_WIN_INVALID_HANDLE ((Av2hWinHandle)(intptr_t)-1)
#define AV2H_WIN_STD_ERROR_HANDLE ((Av2hWinDword)-12)
#define AV2H_WIN_GENERIC_READ 0x80000000U
#define AV2H_WIN_GENERIC_WRITE 0x40000000U
#define AV2H_WIN_FILE_SHARE_READ 0x00000001U
#define AV2H_WIN_CREATE_ALWAYS 2U
#define AV2H_WIN_OPEN_EXISTING 3U
#define AV2H_WIN_FILE_ATTRIBUTE_NORMAL 0x00000080U
#define AV2H_WIN_MEM_COMMIT 0x00001000U
#define AV2H_WIN_MEM_RESERVE 0x00002000U
#define AV2H_WIN_MEM_RELEASE 0x00008000U
#define AV2H_WIN_PAGE_READWRITE 0x00000004U

__declspec(dllimport) void *__stdcall GetCommandLineW(void);
__declspec(dllimport) Av2hWinHandle __stdcall GetStdHandle(Av2hWinDword value);
__declspec(dllimport) Av2hWinHandle __stdcall CreateFileW(
    const Av2hWinChar *name,
    Av2hWinDword access,
    Av2hWinDword share,
    void *security,
    Av2hWinDword creation,
    Av2hWinDword attributes,
    Av2hWinHandle template_file);
__declspec(dllimport) Av2hWinBool __stdcall GetFileSizeEx(
    Av2hWinHandle file,
    Av2hWinLargeInteger *size);
__declspec(dllimport) Av2hWinBool __stdcall ReadFile(
    Av2hWinHandle file,
    void *buffer,
    Av2hWinDword bytes,
    Av2hWinDword *read,
    void *overlapped);
__declspec(dllimport) Av2hWinBool __stdcall WriteFile(
    Av2hWinHandle file,
    const void *buffer,
    Av2hWinDword bytes,
    Av2hWinDword *written,
    void *overlapped);
__declspec(dllimport) Av2hWinBool __stdcall CloseHandle(Av2hWinHandle handle);
__declspec(dllimport) void *__stdcall VirtualAlloc(
    void *address,
    size_t size,
    Av2hWinDword allocation_type,
    Av2hWinDword protection);
__declspec(dllimport) Av2hWinBool __stdcall VirtualFree(
    void *address,
    size_t size,
    Av2hWinDword free_type);
__declspec(dllimport) __attribute__((noreturn))
void __stdcall ExitProcess(unsigned int exit_code);

#endif
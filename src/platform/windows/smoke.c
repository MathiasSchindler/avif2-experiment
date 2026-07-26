#include "av2heif.h"

__declspec(dllimport) __attribute__((noreturn))
void __stdcall ExitProcess(unsigned int exit_code);

__declspec(noreturn) void mainCRTStartup(void) {
    unsigned char encoded[2];
    size_t encoded_size = 0U;
    size_t decoded = 0U;
    size_t decoded_size = 0U;
    Av2hError error;
    Av2hStatus status;

    status = av2h_leb128_write(300U, encoded, sizeof(encoded), &encoded_size);
    if (status == AV2H_OK && encoded_size == sizeof(encoded) &&
        encoded[0] == 0xacU && encoded[1] == 0x02U) {
        status = av2h_leb128_read(
            (Av2hSpan){ encoded, encoded_size }, &decoded, &decoded_size, &error);
    }
    ExitProcess(status == AV2H_OK && decoded == 300U &&
                        decoded_size == encoded_size
                    ? 0U
                    : 1U);
}
#include "encoder/platform.h"

int main(void) {
    EncMemoryMapping mapping = {0, 0U};
    EncStatus status = enc_page_allocate(4097U, &mapping);
    uint8_t *bytes;
    if (status != ENC_OK || mapping.data == 0 || mapping.size != 4097U) {
        return 1;
    }
    bytes = (uint8_t *)mapping.data;
    bytes[0] = 0x12U;
    bytes[4096U] = 0x34U;
    if (bytes[0] != 0x12U || bytes[4096U] != 0x34U) return 2;
    if (enc_page_free(&mapping) != ENC_OK ||
        mapping.data != 0 || mapping.size != 0U) {
        return 3;
    }
    if (enc_page_allocate(SIZE_MAX, &mapping) != ENC_OVERFLOW ||
        mapping.data != 0 || mapping.size != 0U) {
        return 4;
    }
    if (enc_page_allocate(SIZE_MAX / 2U, &mapping) != ENC_OUT_OF_SPACE ||
        mapping.data != 0 || mapping.size != 0U) {
        return 5;
    }
    return 0;
}

#include <stdint.h>
#include <stdio.h>

#include "avm_dsp/entenc.h"
#include "avm_dsp/prob.h"

int main(void) {
    static const uint16_t cdf2[] = { 16384U, 0U };
    static const uint16_t cdf3[] = { 24576U, 8192U, 0U };
    static const uint16_t cdf5[] = {
        30000U, 22000U, 14000U, 6000U, 0U
    };
    od_ec_enc encoder;
    unsigned char *bytes;
    uint32_t size;
    uint32_t index;

    avm_od_ec_enc_init(&encoder, 32U);
    printf("tell 0 %d\n", avm_od_ec_enc_tell(&encoder));
    avm_od_ec_encode_cdf_q15(&encoder, 0, cdf3, 3);
    printf("tell 1 %d\n", avm_od_ec_enc_tell(&encoder));
    od_ec_encode_literal_bypass(&encoder, 21, 5);
    printf("tell 2 %d\n", avm_od_ec_enc_tell(&encoder));
    avm_od_ec_encode_cdf_q15(&encoder, 1, cdf2, 2);
    printf("tell 3 %d\n", avm_od_ec_enc_tell(&encoder));
    avm_od_ec_encode_cdf_q15(&encoder, 4, cdf5, 5);
    printf("tell 4 %d\n", avm_od_ec_enc_tell(&encoder));
    od_ec_encode_bool_bypass(&encoder, 0);
    printf("tell 5 %d\n", avm_od_ec_enc_tell(&encoder));
    avm_od_ec_encode_cdf_q15(&encoder, 2, cdf3, 3);
    printf("tell 6 %d\n", avm_od_ec_enc_tell(&encoder));
    od_ec_encode_literal_bypass(&encoder, 0x2d3, 10);
    printf("tell 7 %d\n", avm_od_ec_enc_tell(&encoder));
    avm_od_ec_encode_cdf_q15(&encoder, 0, cdf2, 2);
    printf("tell 8 %d\n", avm_od_ec_enc_tell(&encoder));
    bytes = avm_od_ec_enc_done(&encoder, &size);
    printf("bytes %u", size);
    for (index = 0U; index < size; ++index) printf(" %02x", bytes[index]);
    printf("\n");
    avm_od_ec_enc_clear(&encoder);

    {
        static const int symbols[] = { 0, 2, 1, 1, 0, 2, 2, 1 };
        uint16_t adaptive[] = { 24576U, 8192U, 0U, 0U, 0U, 1U, 2U };
        size_t symbol_index;
        avm_od_ec_enc_init(&encoder, 32U);
        for (symbol_index = 0U;
             symbol_index < sizeof(symbols) / sizeof(symbols[0]);
             ++symbol_index) {
            avm_od_ec_encode_cdf_q15(&encoder, symbols[symbol_index],
                                      adaptive, 3);
            update_cdf(adaptive, (int8_t)symbols[symbol_index], 3);
            printf("adaptive %zu %d %u %u %u %u %d\n", symbol_index,
                   symbols[symbol_index], adaptive[0], adaptive[1],
                   adaptive[2], adaptive[3], avm_od_ec_enc_tell(&encoder));
        }
        bytes = avm_od_ec_enc_done(&encoder, &size);
        printf("adaptive-bytes %u", size);
        for (index = 0U; index < size; ++index) printf(" %02x", bytes[index]);
        printf("\n");
        avm_od_ec_enc_clear(&encoder);
    }
    return 0;
}
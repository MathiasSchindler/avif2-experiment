#include "decoder/cdf.h"
#include "decoder/partition_cdf.h"

#include <stdio.h>

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static void check_row(uint16_t threshold,
                      uint8_t parameter_index,
                      uint8_t count,
                      uint16_t first_rate,
                      uint16_t second_rate,
                      uint16_t third_rate) {
    uint16_t cdf[AV2H_BINARY_CDF_SIZE];
    Av2hBinaryCdfSpec specification = {
        threshold, parameter_index, count
    };
    CHECK(av2h_binary_cdf_init(cdf, specification) == AV2H_OK);
    CHECK(cdf[0] == 32768U - threshold);
    CHECK(cdf[1] == 0U);
    CHECK(cdf[2] == count);
    CHECK(cdf[3] == first_rate);
    CHECK(cdf[4] == second_rate);
    CHECK(cdf[5] == third_rate);
}

static void test_normative_partition_rows(void) {
    check_row(28084U, 3U, 0U, 2U, 3U, 5U);
    check_row(23755U, 93U, 0U, 3U, 4U, 5U);
    check_row(18000U, 7U, 0U, 2U, 2U, 2U);
    check_row(14644U, 0U, 0U, 2U, 3U, 4U);
    check_row(31091U, 3U, 0U, 2U, 3U, 5U);
    check_row(25789U, 31U, 0U, 1U, 2U, 3U);
}

static void check_cdf(const uint16_t *thresholds,
                      size_t symbol_count,
                      uint8_t parameter_index,
                      const uint16_t *expected) {
    uint16_t cdf[AV2H_MAX_CDF_SIZE];
    size_t index;
    CHECK(av2h_cdf_init(cdf, AV2H_MAX_CDF_SIZE, thresholds, symbol_count,
                        parameter_index, 0U) == AV2H_OK);
    for (index = 0U; index < symbol_count + 4U; ++index) {
        CHECK(cdf[index] == expected[index]);
    }
}

static void test_normative_luma_mode_rows(void) {
    static const uint16_t set_thresholds[] = { 28863U, 31022U, 31724U };
    static const uint16_t set_expected[] = {
        3905U, 1746U, 1044U, 0U, 0U, 4U, 5U, 6U
    };
    static const uint16_t index_thresholds[] = {
        15175U, 20075U, 21728U, 24098U, 26405U, 27655U, 28860U
    };
    static const uint16_t index_expected[] = {
        17593U, 12693U, 11040U, 8670U, 6363U, 5113U, 3908U, 0U,
        0U, 3U, 3U, 5U
    };
    static const uint16_t offset_thresholds[] = {
        12743U, 18172U, 20194U, 23648U, 26419U
    };
    static const uint16_t offset_expected[] = {
        20025U, 14596U, 12574U, 9120U, 6349U, 0U, 0U, 3U, 3U, 4U
    };
    check_cdf(set_thresholds, 4U, 93U, set_expected);
    check_cdf(index_thresholds, 8U, 5U, index_expected);
    check_cdf(offset_thresholds, 6U, 6U, offset_expected);
}

static void test_failures(void) {
    uint16_t cdf[AV2H_BINARY_CDF_SIZE];
    static const uint16_t descending[] = { 100U, 99U, 200U };
    static const uint16_t valid[] = { 100U, 200U, 300U };
    CHECK(av2h_binary_cdf_init(0, (Av2hBinaryCdfSpec){ 1U, 0U, 0U }) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_binary_cdf_init(cdf,
              (Av2hBinaryCdfSpec){ 0U, 0U, 0U }) == AV2H_INVALID_DATA);
    CHECK(av2h_binary_cdf_init(cdf,
              (Av2hBinaryCdfSpec){ 32768U, 0U, 0U }) == AV2H_INVALID_DATA);
    CHECK(av2h_binary_cdf_init(cdf,
              (Av2hBinaryCdfSpec){ 1U, 125U, 0U }) == AV2H_INVALID_DATA);
    CHECK(av2h_binary_cdf_init(cdf,
              (Av2hBinaryCdfSpec){ 1U, 0U, 33U }) == AV2H_INVALID_DATA);
        CHECK(av2h_cdf_init(cdf, AV2H_BINARY_CDF_SIZE, descending, 4U, 0U, 0U) ==
            AV2H_INVALID_DATA);
        CHECK(av2h_cdf_init(cdf, AV2H_BINARY_CDF_SIZE, valid, 4U, 0U, 0U) ==
            AV2H_INVALID_DATA);
        CHECK(av2h_cdf_init(cdf, AV2H_BINARY_CDF_SIZE, valid, 1U, 0U, 0U) ==
            AV2H_INVALID_DATA);
}

static void test_partition_defaults(void) {
    Av2hPartitionCdfs context;
    CHECK(sizeof(context) == 6336U);
    CHECK(av2h_partition_cdfs_init(0) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_partition_cdfs_init(&context) == AV2H_OK);
    CHECK(context.do_split[0][0][0] == 32768U - 28084U);
    CHECK(context.do_split[0][0][3] == 2U);
    CHECK(context.do_split[0][1][3] == 3U);
    CHECK(context.do_square_split[0][0][0] == 32768U - 18000U);
    CHECK(context.rect_type[0][0][0] == 32768U - 14644U);
    CHECK(context.do_ext_partition[0][4][0] == 32768U - 31091U);
    CHECK(context.do_uneven_4way_partition[0][8][0] ==
          32768U - 25789U);
}

int main(void) {
    test_normative_partition_rows();
    test_normative_luma_mode_rows();
    test_failures();
    test_partition_defaults();
    if (failures != 0) {
        fprintf(stderr, "%d CDF checks failed\n", failures);
        return 1;
    }
    puts("cdf unit: pass");
    return 0;
}
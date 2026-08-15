#include "encoder/cdf.h"
#include "encoder/m1.h"
#include "encoder/obu.h"
#include "encoder/range.h"
#include "encoder/syntax.h"

static int bytes_equal(const uint8_t *left, const uint8_t *right, size_t size) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static int words_equal(
    const uint16_t *left, const uint16_t *right, size_t size) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static int test_fields(void) {
    static const uint8_t expected_sequence[] = {
        0x82U, 0x04U, 0x88U, 0xbfU, 0x18U,
        0x7eU, 0x6fU, 0x10U, 0x22U
    };
    static const uint8_t expected_frame[] = {
        0xe3U, 0xfeU, 0x6cU, 0x00U
    };
    static const uint8_t expected_tile[] = {0x8dU, 0x80U};
    uint8_t data[16U];
    size_t size;
    if (enc_m1_sequence_header(data, sizeof(data), &size) != ENC_OK ||
        size != sizeof(expected_sequence) ||
        !bytes_equal(data, expected_sequence, size)) {
        return 1;
    }
    if (enc_m1_frame_header(data, sizeof(data), &size) != ENC_OK ||
        size != sizeof(expected_frame) ||
        !bytes_equal(data, expected_frame, size)) {
        return 2;
    }
    if (enc_m1_tile(data, sizeof(data), &size) != ENC_OK ||
        size != sizeof(expected_tile) ||
        !bytes_equal(data, expected_tile, size)) {
        return 3;
    }
    return 0;
}

static int test_stream_and_exhaustion(void) {
    static const uint8_t expected[] = {
        0x01U, 0x08U,
        0x0aU, 0x04U, 0x82U, 0x04U, 0x88U, 0xbfU, 0x18U, 0x7eU,
        0x6fU, 0x10U, 0x22U,
        0x07U, 0x10U, 0xe3U, 0xfeU, 0x6cU, 0x00U, 0x8dU, 0x80U
    };
    uint8_t data[ENC_M1_STREAM_SIZE + 1U];
    size_t capacity;
    size_t size;
    data[ENC_M1_STREAM_SIZE] = 0xa5U;
    if (enc_m1_constant_128_8x8(
            data, ENC_M1_STREAM_SIZE, &size) != ENC_OK ||
        size != sizeof(expected) || !bytes_equal(data, expected, size) ||
        data[ENC_M1_STREAM_SIZE] != 0xa5U) {
        return 1;
    }
    for (capacity = 0U; capacity < ENC_M1_STREAM_SIZE; ++capacity) {
        data[capacity] = 0xa5U;
        size = 99U;
        if (enc_m1_constant_128_8x8(data, capacity, &size) !=
                ENC_OUT_OF_SPACE ||
            size != 0U || data[capacity] != 0xa5U) {
            return 2;
        }
    }
    if (enc_m1_constant_128_8x8(data, sizeof(data), 0) !=
            ENC_INVALID_ARGUMENT ||
        enc_m1_constant_128_8x8(0, 1U, &size) != ENC_INVALID_ARGUMENT) {
        return 3;
    }
    return 0;
}

static int test_cdf_and_leb128(void) {
    static const uint16_t thresholds[] = {16594U, 23325U, 26424U,
                                         28225U, 29358U, 30099U, 30613U};
    static const uint16_t expected[] = {16174U, 9443U, 6344U, 4543U,
                                       3410U, 2669U, 2155U, 0U,
                                       0U, 1U, 3U, 4U};
    uint16_t cdf[12U];
    uint8_t bytes[8U];
    EncByteWriter writer;
    if (enc_cdf_materialize(
            cdf, 12U, thresholds, 8U, 56U, 0U) != ENC_OK ||
        !bytes_equal(
            (const uint8_t *)cdf, (const uint8_t *)expected,
            sizeof(expected))) {
        return 1;
    }
    if (enc_cdf_materialize(cdf, 11U, thresholds, 8U, 56U, 0U) !=
            ENC_INVALID_ARGUMENT) {
        return 2;
    }
    enc_byte_writer_init(&writer, bytes, sizeof(bytes));
    if (enc_leb128_write(&writer, 0U) != ENC_OK ||
        enc_leb128_write(&writer, 127U) != ENC_OK ||
        enc_leb128_write(&writer, 128U) != ENC_OK ||
        writer.position != 4U || bytes[0] != 0U || bytes[1] != 127U ||
        bytes[2] != 128U || bytes[3] != 1U) {
        return 3;
    }
    return 0;
}

static int test_normative_syntax_cdfs(void) {
    static const uint16_t expected_do_split[] = {
        4684U, 0U, 0U, 2U, 3U, 5U
    };
    static const uint16_t expected_eob[] = {
        30920U, 30411U, 28376U, 25139U, 19440U, 11238U, 0U,
        0U, 2U, 3U, 3U
    };
    static const uint16_t expected_base_eob[] = {
        21845U, 10923U, 0U, 0U, 2U, 3U, 4U
    };
    EncSyntaxCdfs cdfs;
    uint16_t *row;
    if (enc_syntax_cdfs_init(&cdfs) != ENC_OK ||
        enc_syntax_cdfs_init(0) != ENC_INVALID_ARGUMENT) {
        return 1;
    }
    row = enc_syntax_do_split_cdf(&cdfs, 0U, 0U);
    if (row == 0 || !words_equal(row, expected_do_split, 6U)) {
        return 2;
    }
    row = enc_syntax_eob_pt_64_cdf(&cdfs, 0U, 0U);
    if (row == 0 || !words_equal(row, expected_eob, 11U)) {
        return 3;
    }
    row = enc_syntax_coeff_base_eob_cdf(&cdfs, 0U, 0U, 0U);
    if (row == 0 || !words_equal(row, expected_base_eob, 7U)) {
        return 4;
    }
    row = enc_syntax_dc_sign_cdf(&cdfs, 0U, 0U, 0U, 0U);
    if (row == 0 || row[0] != 16937U) {
        return 5;
    }
    row[0] = 123U;
    if (enc_syntax_dc_sign_cdf(&cdfs, 0U, 0U, 0U, 0U)[0] != 123U) {
        return 6;
    }
    if (enc_syntax_do_split_cdf(&cdfs, 2U, 0U) != 0 ||
        enc_syntax_y_mode_index_cdf(&cdfs, 3U) != 0 ||
        enc_syntax_fsc_mode_cdf(&cdfs, 4U, 0U) != 0 ||
        enc_syntax_txb_skip_cdf(&cdfs, 4U, 0U, 0U, 0U) != 0 ||
        enc_syntax_eob_pt_64_cdf(&cdfs, 0U, 3U) != 0 ||
        enc_syntax_coeff_base_lf_eob_cdf(&cdfs, 0U, 0U, 4U) != 0 ||
        enc_syntax_intra_tx_type_long_cdf(&cdfs, 4U) != 0) {
        return 7;
    }
    return 0;
}

static int test_fixed_dc_range_transcript(void) {
    static const uint8_t expected[] = {0x00U, 0xc0U};
    EncSyntaxCdfs cdfs;
    EncRangeWriter writer;
    uint8_t data[8U];
    if (enc_syntax_cdfs_init(&cdfs) != ENC_OK) {
        return 1;
    }
    enc_range_writer_init(&writer, data, sizeof(data));
    if (enc_range_writer_symbol(
            &writer, 0U, enc_syntax_intra_tx_type_set1_cdf(&cdfs, 0U),
            7U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, 0U, enc_syntax_txb_skip_cdf(
                &cdfs, 0U, 0U, 1U, 0U), 2U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, 0U, enc_syntax_eob_pt_64_cdf(&cdfs, 0U, 0U),
            7U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, 0U, enc_syntax_coeff_base_lf_eob_cdf(
                &cdfs, 0U, 1U, 0U), 5U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, 1U, enc_syntax_dc_sign_cdf(
                &cdfs, 0U, 0U, 0U, 0U), 2U, 1) != ENC_OK ||
        enc_range_writer_finish(&writer) != ENC_OK ||
        enc_range_writer_bytes_used(&writer) != sizeof(expected) ||
        !bytes_equal(data, expected, sizeof(expected))) {
        return 2;
    }
    return 0;
}

int main(void) {
    int result = test_fields();
    if (result != 0) return 10 + result;
    result = test_stream_and_exhaustion();
    if (result != 0) return 20 + result;
    result = test_cdf_and_leb128();
    if (result != 0) return 30 + result;
    result = test_normative_syntax_cdfs();
    if (result != 0) return 40 + result;
    result = test_fixed_dc_range_transcript();
    if (result != 0) return 50 + result;
    return 0;
}

#ifndef AV2HEIF_H
#define AV2HEIF_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    AV2H_OK = 0,
    AV2H_INVALID_ARGUMENT,
    AV2H_TRUNCATED,
    AV2H_INVALID_DATA,
    AV2H_OVERFLOW,
    AV2H_LIMIT_EXCEEDED,
    AV2H_OUT_OF_MEMORY,
    AV2H_UNSUPPORTED
} Av2hStatus;

typedef enum {
    AV2H_CONTEXT_NONE = 0,
    AV2H_CONTEXT_LEB128 = 1,
    AV2H_CONTEXT_OBU_HEADER = 2,
    AV2H_CONTEXT_OBU_ORDER = 3,
    AV2H_CONTEXT_SEQUENCE_HEADER = 4,
    AV2H_CONTEXT_CONTENT_INTERPRETATION = 5,
    AV2H_CONTEXT_AV2C = 6,
    AV2H_CONTEXT_BOX = 7,
    AV2H_CONTEXT_HEIF = 8,
    AV2H_CONTEXT_DIALECT = 9,
    AV2H_CONTEXT_LIMITS = 10,
    AV2H_CONTEXT_QUERY = 11,
    AV2H_CONTEXT_FRAME = 12,
    AV2H_CONTEXT_WORKSPACE = 13,
    AV2H_CONTEXT_OUTPUT = 14,
    AV2H_CONTEXT_BINDING = 15,
    AV2H_CONTEXT_DECODE = 16,
    AV2H_CONTEXT_RECONSTRUCTION = 17,
    AV2H_CONTEXT_FILTER = 18
} Av2hErrorContext;

typedef struct {
    Av2hStatus status;
    size_t offset;
    uint32_t context;
    size_t obu_index;
    uint8_t obu_type;
} Av2hError;

const char *av2h_status_string(Av2hStatus status);

typedef struct {
    const unsigned char *data;
    size_t size;
} Av2hSpan;

typedef enum {
    AV2H_OBU_SEQUENCE_HEADER = 1,
    AV2H_OBU_TEMPORAL_DELIMITER = 2,
    AV2H_OBU_MULTI_FRAME_HEADER = 3,
    AV2H_OBU_CLOSED_LOOP_KEY = 4,
    AV2H_OBU_OPEN_LOOP_KEY = 5,
    AV2H_OBU_LEADING_TILE_GROUP = 6,
    AV2H_OBU_REGULAR_TILE_GROUP = 7,
    AV2H_OBU_METADATA_SHORT = 8,
    AV2H_OBU_METADATA_GROUP = 9,
    AV2H_OBU_SWITCH = 10,
    AV2H_OBU_LEADING_SEF = 11,
    AV2H_OBU_REGULAR_SEF = 12,
    AV2H_OBU_LEADING_TIP = 13,
    AV2H_OBU_REGULAR_TIP = 14,
    AV2H_OBU_BUFFER_REMOVAL_TIMING = 15,
    AV2H_OBU_LAYER_CONFIGURATION_RECORD = 16,
    AV2H_OBU_ATLAS_SEGMENT = 17,
    AV2H_OBU_OPERATING_POINT_SET = 18,
    AV2H_OBU_BRIDGE_FRAME = 19,
    AV2H_OBU_MULTI_STREAM_DECODER_OPERATION = 20,
    AV2H_OBU_RAS_FRAME = 21,
    AV2H_OBU_QUANTIZATION_MATRIX = 22,
    AV2H_OBU_FILM_GRAIN_MODEL = 23,
    AV2H_OBU_CONTENT_INTERPRETATION = 24,
    AV2H_OBU_PADDING = 25
} Av2hObuType;

typedef struct {
    size_t index;
    size_t offset;
    size_t length_field_size;
    size_t obu_size;
    size_t header_size;
    size_t payload_offset;
    size_t payload_size;
    uint8_t type;
    uint8_t temporal_id;
    uint8_t mlayer_id;
    uint8_t xlayer_id;
    uint8_t extension_flag;
} Av2hObu;

typedef struct {
    Av2hObu *entries;
    size_t capacity;
    size_t count;
} Av2hObuLedger;

typedef struct {
    uint8_t seq_profile_idc;
    uint8_t seq_level_idx;
    uint8_t seq_tier;
    uint8_t chroma_format_idc;
    uint8_t bit_depth_idc;
    uint8_t monotonic_output_order_flag;
    uint8_t still_picture;
    uint8_t film_grain_params_present;
    uint8_t initial_display_delay_present;
    uint8_t initial_display_delay_minus_1;
    uint16_t max_frame_width_minus_1;
    uint16_t max_frame_height_minus_1;
} Av2hSequenceInfo;

typedef struct {
    uint8_t color_description_present;
    uint8_t color_description_idc;
    uint8_t color_primaries;
    uint8_t transfer_characteristics;
    uint8_t matrix_coefficients;
    uint8_t full_range_flag;
} Av2hColorInfo;

typedef struct {
    uint8_t configuration_version;
    Av2hSequenceInfo sequence;
    Av2hSpan config_obus;
} Av2hAv2C;

typedef struct {
    size_t offset;
    size_t header_size;
    uint64_t size;
    uint64_t payload_size;
    uint32_t type;
} Av2hBoxHeader;

typedef struct {
    Av2hSpan config_obus;
    Av2hSpan item_payload;
    Av2hColorInfo color;
} Av2hHeifSource;

typedef enum {
    AV2H_SAMPLE_STORAGE_UNKNOWN = 0,
    AV2H_SAMPLE_STORAGE_U8,
    AV2H_SAMPLE_STORAGE_U16
} Av2hSampleStorage;

typedef enum {
    AV2H_BYTE_ORDER_NOT_APPLICABLE = 0,
    AV2H_BYTE_ORDER_LITTLE_ENDIAN,
    AV2H_BYTE_ORDER_BIG_ENDIAN
} Av2hByteOrder;

typedef enum {
    AV2H_PLANE_ORDER_UNKNOWN = 0,
    AV2H_PLANE_ORDER_YUV
} Av2hPlaneOrder;

typedef struct {
    uint8_t chroma_format_idc;
    uint8_t bit_depth;
    uint8_t bytes_per_sample;
    uint8_t sample_storage;
    uint8_t byte_order;
    uint8_t plane_order;
} Av2hPixelFormat;

typedef struct {
    Av2hHeifSource source;
    Av2hSequenceInfo sequence;
    uint32_t width;
    uint32_t height;
    uint8_t channel_count;
    uint8_t bits_per_channel;
    Av2hPixelFormat pixel_format;
    size_t property_count;
} Av2hHeifImage;

typedef enum {
    AV2H_DIALECT_UNKNOWN = 0,
    AV2H_DIALECT_CANONICAL_V0,
    AV2H_DIALECT_LIBAVIF_AVM_2026_07,
    AV2H_DIALECT_CONFLICTING
} Av2hDialect;

typedef struct {
    size_t max_input_size;
    size_t max_config_obus_size;
    size_t max_item_payload_size;
    size_t max_pixels;
    size_t max_output_size;
    size_t max_workspace_size;
    uint32_t max_width;
    uint32_t max_height;
    size_t max_obu_count;
    size_t max_property_count;
    size_t max_recursion_depth;
} Av2hLimits;

typedef struct {
    /* Width and height are samples; all remaining fields are byte counts. */
    uint32_t width;
    uint32_t height;
    size_t row_bytes;
    size_t offset;
    size_t size;
} Av2hPlaneLayout;

typedef struct {
    Av2hDialect dialect;
    Av2hHeifImage image;
    Av2hPlaneLayout planes[3];
    /* Total byte count required for the tightly packed decoded planes. */
    size_t output_size;
    /* Caller workspace requirement in bytes and its power-of-two alignment. */
    size_t workspace_size;
    size_t workspace_alignment;
    size_t obu_count;
    size_t property_count;
    size_t container_depth;
    size_t input_size;
    uint64_t input_fingerprint;
} Av2hInfo;

Av2hStatus av2h_leb128_read(Av2hSpan input,
                            size_t *value,
                            size_t *encoded_size,
                            Av2hError *error);
Av2hStatus av2h_leb128_write(size_t value,
                             void *output,
                             size_t output_size,
                             size_t *written);
Av2hStatus av2h_obu_parse(Av2hSpan input,
                          Av2hObuLedger *ledger,
                          Av2hError *error);
Av2hStatus av2h_sequence_header_parse(Av2hSpan input,
                                      const Av2hObu *obu,
                                      Av2hSequenceInfo *sequence,
                                      Av2hError *error);
void av2h_color_info_default(Av2hColorInfo *color);
Av2hStatus av2h_content_interpretation_parse(Av2hSpan input,
                                             const Av2hObu *obu,
                                             Av2hColorInfo *color,
                                             Av2hError *error);
Av2hStatus av2h_av2c_parse(Av2hSpan input,
                           Av2hAv2C *configuration,
                           Av2hObuLedger *ledger,
                           Av2hError *error);
Av2hStatus av2h_av2c_write(const Av2hAv2C *configuration,
                           void *output,
                           size_t output_size,
                           size_t *written);
Av2hStatus av2h_carriage_validate(Av2hSpan config_obus,
                                  Av2hSpan item_payload,
                                  Av2hSequenceInfo *sequence,
                                  Av2hError *error);
Av2hStatus av2h_box_header_parse(Av2hSpan input,
                                 size_t offset,
                                 Av2hBoxHeader *box,
                                 Av2hError *error);
Av2hStatus av2h_box_header_write(uint32_t type,
                                 uint64_t payload_size,
                                 void *output,
                                 size_t output_size,
                                 size_t *written);
Av2hStatus av2h_heif_parse(Av2hSpan input,
                           Av2hHeifImage *image,
                           Av2hError *error);
Av2hStatus av2h_heif_write(const Av2hHeifSource *source,
                           void *output,
                           size_t output_size,
                           size_t *written,
                           Av2hError *error);
void av2h_limits_default(Av2hLimits *limits);
Av2hStatus av2h_dialect_detect(Av2hSpan input,
                               Av2hDialect *dialect,
                               Av2hError *error);
Av2hStatus av2h_query(Av2hSpan input,
                      const Av2hLimits *limits,
                      Av2hInfo *info,
                      Av2hError *error);
Av2hStatus av2h_validate(Av2hSpan input,
                         const Av2hLimits *limits,
                         Av2hError *error);

/* Decode canonical AV2-HEIF into the contiguous planar layout from info. */
Av2hStatus av2h_decode(Av2hSpan input,
                       const Av2hInfo *info,
                       void *workspace,
                       size_t workspace_size,
                       void *output,
                       size_t output_size,
                       Av2hError *error);

#endif
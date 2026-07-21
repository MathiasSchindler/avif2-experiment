#include "av2heif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOURCC(a, b, c, d)                                                    \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) |                         \
     ((uint32_t)(c) << 8) | (uint32_t)(d))

#define BOX_META FOURCC('m', 'e', 't', 'a')
#define BOX_IINF FOURCC('i', 'i', 'n', 'f')
#define BOX_IPRP FOURCC('i', 'p', 'r', 'p')
#define BOX_IPCO FOURCC('i', 'p', 'c', 'o')
#define BOX_IPMA FOURCC('i', 'p', 'm', 'a')

typedef struct {
    unsigned char *data;
    size_t size;
} OwnedBytes;

typedef struct {
    uint32_t types[16];
    unsigned char essential[16];
    size_t count;
} PropertyTable;

static void usage(const char *program) {
    fprintf(stderr,
            "usage:\n"
            "  %s pack CONFIG PAYLOAD OUTPUT CP TC MC limited|full\n"
            "  %s inspect INPUT OUTPUT_JSON\n"
            "  %s extract INPUT CONFIG PAYLOAD RECONSTRUCTED\n",
            program, program, program);
}

static int read_file(const char *path, OwnedBytes *owned) {
    FILE *file;
    long length;
    size_t read_size;
    int close_status;
    owned->data = NULL;
    owned->size = 0U;
    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "error: cannot open %s\n", path);
        return 0;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) < 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "error: cannot size %s\n", path);
        fclose(file);
        return 0;
    }
    if ((unsigned long)length > (size_t)-1) {
        fprintf(stderr, "error: %s is too large\n", path);
        fclose(file);
        return 0;
    }
    owned->size = (size_t)length;
    owned->data = (unsigned char *)malloc(owned->size == 0U ? 1U : owned->size);
    if (owned->data == NULL) {
        fprintf(stderr, "error: cannot allocate %lu bytes\n",
                (unsigned long)owned->size);
        fclose(file);
        return 0;
    }
    read_size = fread(owned->data, 1U, owned->size, file);
    close_status = fclose(file);
    if (read_size != owned->size || close_status != 0) {
        fprintf(stderr, "error: cannot read %s\n", path);
        free(owned->data);
        owned->data = NULL;
        owned->size = 0U;
        return 0;
    }
    return 1;
}

static int write_file(const char *path, const void *data, size_t size) {
    FILE *file = fopen(path, "wb");
    size_t write_size;
    int close_status;
    if (file == NULL) {
        fprintf(stderr, "error: cannot create %s\n", path);
        return 0;
    }
    write_size = fwrite(data, 1U, size, file);
    close_status = fclose(file);
    if (write_size != size || close_status != 0) {
        fprintf(stderr, "error: cannot write %s\n", path);
        return 0;
    }
    return 1;
}

static uint16_t read_u16be(const unsigned char *data) {
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static uint32_t read_u32be(const unsigned char *data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

static int parse_u8(const char *text, uint8_t *value) {
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);
    if (text[0] == '\0' || end == NULL || end[0] != '\0' || parsed > 255UL) {
        return 0;
    }
    *value = (uint8_t)parsed;
    return 1;
}

static void report_status(const char *operation,
                          Av2hStatus status,
                          const Av2hError *error) {
    fprintf(stderr,
            "error: %s: %s at byte %lu (context %u)\n",
            operation,
            av2h_status_string(status),
            (unsigned long)error->offset,
            (unsigned int)error->context);
}

static int child_bounds(Av2hSpan input,
                        const Av2hBoxHeader *box,
                        size_t *start,
                        size_t *end) {
    size_t prefix = 0U;
    if (box->size > (uint64_t)input.size || box->offset > input.size) return 0;
    *end = box->offset + (size_t)box->size;
    if (box->type == BOX_META) prefix = 4U;
    if (box->type == BOX_IINF) prefix = 6U;
    if (prefix > (size_t)box->payload_size) return 0;
    *start = box->offset + box->header_size + prefix;
    return *start <= *end;
}

static int box_has_children(uint32_t type) {
    return type == BOX_META || type == BOX_IINF || type == BOX_IPRP ||
           type == BOX_IPCO;
}

static void print_indent(FILE *output, unsigned int depth) {
    unsigned int index;
    for (index = 0U; index < depth; ++index) fputs("  ", output);
}

static void print_fourcc(FILE *output, uint32_t type) {
    unsigned int shift;
    fputc('"', output);
    for (shift = 24U;; shift -= 8U) {
        unsigned char value = (unsigned char)(type >> shift);
        if (value == '"' || value == '\\') fputc('\\', output);
        if (value >= 32U && value <= 126U) {
            fputc(value, output);
        } else {
            fprintf(output, "\\u%04x", (unsigned int)value);
        }
        if (shift == 0U) break;
    }
    fputc('"', output);
}

static int print_box_array(FILE *output,
                           Av2hSpan input,
                           size_t start,
                           size_t end,
                           unsigned int depth);

static int print_box(FILE *output,
                     Av2hSpan input,
                     size_t offset,
                     unsigned int depth,
                     size_t *next) {
    Av2hBoxHeader box;
    Av2hError error;
    Av2hStatus status = av2h_box_header_parse(input, offset, &box, &error);
    size_t child_start;
    size_t child_end;
    if (status != AV2H_OK || box.size > (uint64_t)(input.size - offset)) {
        report_status("box tree", status, &error);
        return 0;
    }
    print_indent(output, depth);
    fputs("{\"type\": ", output);
    print_fourcc(output, box.type);
    fprintf(output,
            ", \"offset\": %lu, \"size\": %llu, "
            "\"header_bytes\": %lu, \"payload_offset\": %lu, "
            "\"payload_bytes\": %llu",
            (unsigned long)box.offset,
            (unsigned long long)box.size,
            (unsigned long)box.header_size,
            (unsigned long)(box.offset + box.header_size),
            (unsigned long long)box.payload_size);
    if (box_has_children(box.type)) {
        if (!child_bounds(input, &box, &child_start, &child_end)) return 0;
        fputs(", \"children\": [\n", output);
        if (!print_box_array(output, input, child_start, child_end, depth + 1U)) {
            return 0;
        }
        fputc('\n', output);
        print_indent(output, depth);
        fputc(']', output);
    }
    fputc('}', output);
    *next = offset + (size_t)box.size;
    return 1;
}

static int print_box_array(FILE *output,
                           Av2hSpan input,
                           size_t start,
                           size_t end,
                           unsigned int depth) {
    size_t offset = start;
    int first = 1;
    while (offset < end) {
        size_t next;
        if (!first) fputs(",\n", output);
        if (!print_box(output, input, offset, depth, &next)) return 0;
        if (next <= offset || next > end) return 0;
        offset = next;
        first = 0;
    }
    return offset == end;
}

static int find_box(Av2hSpan input,
                    size_t start,
                    size_t end,
                    uint32_t type,
                    Av2hBoxHeader *found) {
    size_t offset = start;
    while (offset < end) {
        Av2hError error;
        Av2hBoxHeader box;
        if (av2h_box_header_parse(input, offset, &box, &error) != AV2H_OK ||
            box.size > (uint64_t)(end - offset)) {
            return 0;
        }
        if (box.type == type) {
            *found = box;
            return 1;
        }
        offset += (size_t)box.size;
    }
    return 0;
}

static int inspect_properties(Av2hSpan input, PropertyTable *properties) {
    Av2hBoxHeader meta;
    Av2hBoxHeader iprp;
    Av2hBoxHeader ipco;
    Av2hBoxHeader ipma;
    size_t start;
    size_t end;
    size_t offset;
    size_t index;
    const unsigned char *association;
    unsigned int association_count;
    memset(properties, 0, sizeof(*properties));
    if (!find_box(input, 0U, input.size, BOX_META, &meta) ||
        !child_bounds(input, &meta, &start, &end) ||
        !find_box(input, start, end, BOX_IPRP, &iprp) ||
        !child_bounds(input, &iprp, &start, &end) ||
        !find_box(input, start, end, BOX_IPCO, &ipco) ||
        !find_box(input, start, end, BOX_IPMA, &ipma) ||
        !child_bounds(input, &ipco, &start, &end)) {
        return 0;
    }
    offset = start;
    while (offset < end) {
        Av2hBoxHeader property;
        Av2hError error;
        if (properties->count == 16U ||
            av2h_box_header_parse(input, offset, &property, &error) != AV2H_OK ||
            property.size > (uint64_t)(end - offset)) {
            return 0;
        }
        properties->types[properties->count++] = property.type;
        offset += (size_t)property.size;
    }
    start = ipma.offset + ipma.header_size;
    if (ipma.payload_size < 11U || start > input.size - 11U ||
        read_u32be(input.data + start) != 0U ||
        read_u32be(input.data + start + 4U) != 1U ||
        read_u16be(input.data + start + 8U) != 1U) {
        return 0;
    }
    association_count = input.data[start + 10U];
    if (association_count != properties->count ||
        association_count > (size_t)ipma.payload_size - 11U) {
        return 0;
    }
    association = input.data + start + 11U;
    for (index = 0U; index < association_count; ++index) {
        unsigned int property_index = association[index] & 0x7fU;
        if (property_index == 0U || property_index > properties->count) return 0;
        properties->essential[property_index - 1U] =
            (unsigned char)(association[index] >> 7);
    }
    return 1;
}

static int print_obu_ledger(FILE *output, Av2hSpan span, unsigned int depth) {
    Av2hObu entries[4];
    Av2hObuLedger ledger;
    Av2hError error;
    Av2hStatus status;
    size_t index;
    ledger.entries = entries;
    ledger.capacity = sizeof(entries) / sizeof(entries[0]);
    ledger.count = 0U;
    status = av2h_obu_parse(span, &ledger, &error);
    if (status != AV2H_OK) {
        report_status("OBU inspection", status, &error);
        return 0;
    }
    for (index = 0U; index < ledger.count; ++index) {
        const Av2hObu *obu = &ledger.entries[index];
        if (index != 0U) fputs(",\n", output);
        print_indent(output, depth);
        fprintf(output,
                "{\"index\": %lu, \"type\": %u, \"temporal_id\": %u, "
                "\"mlayer_id\": %u, \"xlayer_id\": %u, "
                "\"length_span\": {\"offset\": %lu, \"bytes\": %lu}, "
                "\"obu_span\": {\"offset\": %lu, \"bytes\": %lu}, "
                "\"header_span\": {\"offset\": %lu, \"bytes\": %lu}, "
                "\"payload_span\": {\"offset\": %lu, \"bytes\": %lu}}",
                (unsigned long)obu->index,
                (unsigned int)obu->type,
                (unsigned int)obu->temporal_id,
                (unsigned int)obu->mlayer_id,
                (unsigned int)obu->xlayer_id,
                (unsigned long)obu->offset,
                (unsigned long)obu->length_field_size,
                (unsigned long)obu->offset,
                (unsigned long)(obu->length_field_size + obu->obu_size),
                (unsigned long)(obu->offset + obu->length_field_size),
                (unsigned long)obu->header_size,
                (unsigned long)obu->payload_offset,
                (unsigned long)obu->payload_size);
    }
    return 1;
}

static int command_pack(int argc, char **argv) {
    OwnedBytes config;
    OwnedBytes payload;
    unsigned char *output = NULL;
    Av2hHeifSource source;
    Av2hError error;
    Av2hStatus status;
    size_t required = 0U;
    int result = 1;
    if (argc != 9) return -1;
    if (!parse_u8(argv[5], &source.color.color_primaries) ||
        !parse_u8(argv[6], &source.color.transfer_characteristics) ||
        !parse_u8(argv[7], &source.color.matrix_coefficients) ||
        (strcmp(argv[8], "limited") != 0 && strcmp(argv[8], "full") != 0)) {
        fprintf(stderr, "error: invalid nclx arguments\n");
        return 1;
    }
    source.color.color_description_present = 1U;
    source.color.color_description_idc = 0U;
    source.color.full_range_flag =
        (uint8_t)(strcmp(argv[8], "full") == 0 ? 1U : 0U);
    if (!read_file(argv[2], &config)) return 1;
    if (!read_file(argv[3], &payload)) {
        free(config.data);
        return 1;
    }
    source.config_obus.data = config.data;
    source.config_obus.size = config.size;
    source.item_payload.data = payload.data;
    source.item_payload.size = payload.size;
    status = av2h_heif_write(&source, NULL, 0U, &required, &error);
    if (status != AV2H_OK) {
        report_status("pack sizing", status, &error);
        goto done;
    }
    output = (unsigned char *)malloc(required == 0U ? 1U : required);
    if (output == NULL) {
        fprintf(stderr, "error: cannot allocate output\n");
        goto done;
    }
    status = av2h_heif_write(&source, output, required, &required, &error);
    if (status != AV2H_OK) {
        report_status("pack", status, &error);
        goto done;
    }
    if (!write_file(argv[4], output, required)) goto done;
    result = 0;
done:
    free(output);
    free(payload.data);
    free(config.data);
    return result;
}

static int command_extract(int argc, char **argv) {
    OwnedBytes input;
    unsigned char *reconstructed = NULL;
    Av2hHeifImage image;
    Av2hError error;
    Av2hStatus status;
    size_t reconstructed_size;
    int result = 1;
    if (argc != 6) return -1;
    if (!read_file(argv[2], &input)) return 1;
    status = av2h_heif_parse((Av2hSpan){ input.data, input.size }, &image, &error);
    if (status != AV2H_OK) {
        report_status("extract", status, &error);
        goto done;
    }
    if (image.source.config_obus.size > (size_t)-1 -
                                             image.source.item_payload.size) {
        fprintf(stderr, "error: reconstructed stream size overflow\n");
        goto done;
    }
    reconstructed_size = image.source.config_obus.size +
                         image.source.item_payload.size;
    reconstructed = (unsigned char *)malloc(reconstructed_size == 0U
                                                 ? 1U
                                                 : reconstructed_size);
    if (reconstructed == NULL) {
        fprintf(stderr, "error: cannot allocate reconstructed stream\n");
        goto done;
    }
    memcpy(reconstructed, image.source.config_obus.data,
           image.source.config_obus.size);
    memcpy(reconstructed + image.source.config_obus.size,
           image.source.item_payload.data, image.source.item_payload.size);
    if (!write_file(argv[3], image.source.config_obus.data,
                    image.source.config_obus.size) ||
        !write_file(argv[4], image.source.item_payload.data,
                    image.source.item_payload.size) ||
        !write_file(argv[5], reconstructed, reconstructed_size)) {
        goto done;
    }
    result = 0;
done:
    free(reconstructed);
    free(input.data);
    return result;
}

static int command_inspect(int argc, char **argv) {
    OwnedBytes input;
    Av2hInfo info;
    Av2hHeifImage image;
    Av2hError error;
    Av2hStatus status;
    PropertyTable properties;
    FILE *output = NULL;
    size_t index;
    size_t payload_offset;
    int result = 1;
    if (argc != 4) return -1;
    if (!read_file(argv[2], &input)) return 1;
    status = av2h_query((Av2hSpan){ input.data, input.size }, 0, &info, &error);
    if (status != AV2H_OK) {
        report_status("inspect", status, &error);
        goto done;
    }
    image = info.image;
    if (!inspect_properties((Av2hSpan){ input.data, input.size }, &properties)) {
        fprintf(stderr, "error: cannot inspect property table\n");
        goto done;
    }
    payload_offset = (size_t)(image.source.item_payload.data - input.data);
    output = fopen(argv[3], "wb");
    if (output == NULL) {
        fprintf(stderr, "error: cannot create %s\n", argv[3]);
        goto done;
    }
        fputs("{\n  \"schema\": \"av2heif-artifact-inspection-v2\",\n", output);
        fprintf(output,
            "  \"query\": {\"dialect\": \"canonical-v0\", "
            "\"input_bytes\": %lu, \"input_fingerprint\": \"%016llx\", "
            "\"output_bytes\": %lu, \"workspace_bytes\": %lu, "
            "\"workspace_alignment\": %lu, \"obu_count\": %lu, "
            "\"property_count\": %lu, \"container_depth\": %lu, "
            "\"planes\": [\n",
            (unsigned long)info.input_size,
            (unsigned long long)info.input_fingerprint,
            (unsigned long)info.output_size,
            (unsigned long)info.workspace_size,
            (unsigned long)info.workspace_alignment,
            (unsigned long)info.obu_count,
            (unsigned long)info.property_count,
            (unsigned long)info.container_depth);
        for (index = 0U; index < 3U; ++index) {
        const Av2hPlaneLayout *plane = &info.planes[index];
        fprintf(output,
            "    {\"index\": %lu, \"width\": %u, \"height\": %u, "
            "\"row_bytes\": %lu, \"offset\": %lu, \"bytes\": %lu}%s\n",
            (unsigned long)index,
            plane->width,
            plane->height,
            (unsigned long)plane->row_bytes,
            (unsigned long)plane->offset,
            (unsigned long)plane->size,
            index == 2U ? "" : ",");
        }
        fputs("  ]},\n", output);
    fprintf(output, "  \"container\": {\n    \"bytes\": %lu,\n",
            (unsigned long)input.size);
    fputs("    \"box_tree\": [\n", output);
    if (!print_box_array(output, (Av2hSpan){ input.data, input.size },
                         0U, input.size, 3U)) {
        goto done;
    }
    fputs("\n    ],\n    \"primary_item_id\": 1,\n", output);
    fprintf(output,
            "    \"items\": [{\"id\": 1, \"type\": \"av02\", "
            "\"extent\": {\"offset\": %lu, \"bytes\": %lu}}],\n",
            (unsigned long)payload_offset,
            (unsigned long)image.source.item_payload.size);
    fputs("    \"properties\": [\n", output);
    for (index = 0U; index < properties.count; ++index) {
        fputs("      {\"index\": ", output);
        fprintf(output, "%lu, \"type\": ", (unsigned long)(index + 1U));
        print_fourcc(output, properties.types[index]);
        fprintf(output, ", \"essential\": %s}%s\n",
                properties.essential[index] ? "true" : "false",
                index + 1U == properties.count ? "" : ",");
    }
    fputs("    ],\n    \"associations\": [{\"item_id\": 1, "
          "\"property_indices\": [",
          output);
    for (index = 0U; index < properties.count; ++index) {
        fprintf(output, "%s%lu", index == 0U ? "" : ", ",
                (unsigned long)(index + 1U));
    }
    fputs("]}]\n  },\n  \"obus\": {\n    \"config_obus\": [\n", output);
    if (!print_obu_ledger(output, image.source.config_obus, 3U)) goto done;
    fputs("\n    ],\n    \"item_payload\": [\n", output);
    if (!print_obu_ledger(output, image.source.item_payload, 3U)) goto done;
    fputs("\n    ]\n  },\n  \"codec\": {\n", output);
    fprintf(output,
            "    \"sequence\": {\"profile\": %u, \"level\": %u, "
            "\"tier\": %u, \"chroma_format_idc\": %u, "
            "\"bit_depth_idc\": %u, \"still_picture\": %u},\n",
            (unsigned int)image.sequence.seq_profile_idc,
            (unsigned int)image.sequence.seq_level_idx,
            (unsigned int)image.sequence.seq_tier,
            (unsigned int)image.sequence.chroma_format_idc,
            (unsigned int)image.sequence.bit_depth_idc,
            (unsigned int)image.sequence.still_picture);
    fprintf(output,
            "    \"effective\": {\"width\": %u, \"height\": %u, "
            "\"pixel_format\": \"I420\", \"channels\": %u, "
            "\"bits_per_channel\": %u, \"range\": \"%s\", "
            "\"cicp\": {\"color_primaries\": %u, "
            "\"transfer_characteristics\": %u, "
            "\"matrix_coefficients\": %u}}\n  }\n}\n",
            image.width,
            image.height,
            (unsigned int)image.channel_count,
            (unsigned int)image.bits_per_channel,
            image.source.color.full_range_flag ? "full" : "limited",
            (unsigned int)image.source.color.color_primaries,
            (unsigned int)image.source.color.transfer_characteristics,
            (unsigned int)image.source.color.matrix_coefficients);
    if (fclose(output) != 0) {
        output = NULL;
        fprintf(stderr, "error: cannot close %s\n", argv[3]);
        goto done;
    }
    output = NULL;
    result = 0;
done:
    if (output != NULL) fclose(output);
    free(input.data);
    return result;
}

int main(int argc, char **argv) {
    int result;
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "pack") == 0) {
        result = command_pack(argc, argv);
    } else if (strcmp(argv[1], "inspect") == 0) {
        result = command_inspect(argc, argv);
    } else if (strcmp(argv[1], "extract") == 0) {
        result = command_extract(argc, argv);
    } else {
        usage(argv[0]);
        return 2;
    }
    if (result < 0) {
        usage(argv[0]);
        return 2;
    }
    return result;
}
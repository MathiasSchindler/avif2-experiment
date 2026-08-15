#include "encoder/cli_input.h"

#define ENC_CLI_MANIFEST_CAPACITY 65536U

static int enc_ascii_lower(int character) {
    if (character >= 'A' && character <= 'Z') return character + ('a' - 'A');
    return character;
}

static size_t enc_text_size(const char *text) {
    size_t size = 0U;
    if (text != 0) while (text[size] != '\0') ++size;
    return size;
}

static int enc_extension_equal(const char *extension, const char *expected) {
    while (*extension != '\0' && *expected != '\0') {
        if (enc_ascii_lower(*extension) != enc_ascii_lower(*expected)) return 0;
        ++extension;
        ++expected;
    }
    return *extension == *expected;
}

static const char *enc_path_basename(const char *path) {
    const char *base = path;
    while (*path != '\0') {
        if (*path == '/') base = path + 1;
        ++path;
    }
    return base;
}

static const char *enc_path_extension(const char *path) {
    const char *base = enc_path_basename(path);
    const char *extension = 0;
    while (*base != '\0') {
        if (*base == '.') extension = base;
        ++base;
    }
    return extension;
}

static int enc_parse_decimal(const char *text, size_t limit, size_t *used,
                             size_t *value) {
    size_t position = 0U;
    size_t parsed = 0U;
    if (limit == 0U || text[0] < '0' || text[0] > '9') return 0;
    while (position < limit && text[position] >= '0' && text[position] <= '9') {
        size_t digit = (size_t)(text[position] - '0');
        if (parsed > (SIZE_MAX - digit) / 10U) return 0;
        parsed = parsed * 10U + digit;
        ++position;
    }
    if (parsed == 0U) return 0;
    *used = position;
    *value = parsed;
    return 1;
}

static int enc_ascii_alphanumeric(char character) {
    return (character >= '0' && character <= '9') ||
           (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z');
}

static unsigned int enc_filename_dimensions(const char *path, size_t *width,
                                            size_t *height) {
    const char *base = enc_path_basename(path);
    const char *extension = enc_path_extension(path);
    size_t length = extension == 0 ? enc_text_size(base)
                                  : (size_t)(extension - base);
    size_t position;
    unsigned int matches = 0U;
    for (position = 0U; position < length; ++position) {
        size_t width_used;
        size_t height_used;
        size_t parsed_width;
        size_t parsed_height;
        size_t separator;
        size_t end;
        if (position != 0U && enc_ascii_alphanumeric(base[position - 1U])) {
            continue;
        }
        if (!enc_parse_decimal(base + position, length - position,
                               &width_used, &parsed_width)) {
            continue;
        }
        separator = position + width_used;
        if (separator >= length ||
            (base[separator] != 'x' && base[separator] != 'X')) {
            continue;
        }
        if (!enc_parse_decimal(base + separator + 1U,
                               length - separator - 1U, &height_used,
                               &parsed_height)) {
            continue;
        }
        end = separator + 1U + height_used;
        if (end < length && enc_ascii_alphanumeric(base[end])) continue;
        *width = parsed_width;
        *height = parsed_height;
        ++matches;
        position = end - 1U;
    }
    return matches;
}

static int enc_bytes_equal(const char *left, const char *right, size_t count) {
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static unsigned int enc_manifest_object(
    const char *manifest, size_t search_start, size_t search_end, const char *key,
    size_t key_size, size_t *object_start, size_t *object_end) {
    size_t position;
    unsigned int matches = 0U;
    for (position = search_start; position + key_size <= search_end; ++position) {
        size_t cursor;
        size_t depth = 0U;
        int in_string = 0;
        int escaped = 0;
        if (!enc_bytes_equal(manifest + position, key, key_size)) continue;
        cursor = position + key_size;
        while (cursor < search_end &&
               (manifest[cursor] == ' ' || manifest[cursor] == '\n' ||
                manifest[cursor] == '\r' || manifest[cursor] == '\t')) {
            ++cursor;
        }
        if (cursor == search_end || manifest[cursor++] != ':') return 2U;
        while (cursor < search_end &&
               (manifest[cursor] == ' ' || manifest[cursor] == '\n' ||
                manifest[cursor] == '\r' || manifest[cursor] == '\t')) {
            ++cursor;
        }
        if (cursor == search_end || manifest[cursor] != '{') return 2U;
        *object_start = cursor;
        for (; cursor < search_end; ++cursor) {
            char value = manifest[cursor];
            if (in_string) {
                if (escaped) {
                    escaped = 0;
                } else if (value == '\\') {
                    escaped = 1;
                } else if (value == '"') {
                    in_string = 0;
                }
            } else if (value == '"') {
                in_string = 1;
            } else if (value == '{') {
                ++depth;
            } else if (value == '}') {
                if (depth == 0U) return 2U;
                --depth;
                if (depth == 0U) {
                    *object_end = cursor + 1U;
                    break;
                }
            }
        }
        if (cursor == search_end || in_string || depth != 0U) return 2U;
        ++matches;
        position = cursor;
    }
    return matches;
}

static unsigned int enc_manifest_record_string(
    const char *manifest, size_t start, size_t end, const char *key,
    size_t key_size, const char *expected, size_t expected_size) {
    size_t position;
    unsigned int matches = 0U;
    for (position = start; position + key_size <= end; ++position) {
        size_t cursor;
        if (!enc_bytes_equal(manifest + position, key, key_size)) continue;
        cursor = position + key_size;
        while (cursor < end &&
               (manifest[cursor] == ' ' || manifest[cursor] == '\n' ||
                manifest[cursor] == '\r' || manifest[cursor] == '\t')) {
            ++cursor;
        }
        if (cursor == end || manifest[cursor++] != ':') return 2U;
        while (cursor < end &&
               (manifest[cursor] == ' ' || manifest[cursor] == '\n' ||
                manifest[cursor] == '\r' || manifest[cursor] == '\t')) {
            ++cursor;
        }
        if (cursor == end || manifest[cursor++] != '"' ||
            cursor + expected_size >= end ||
            !enc_bytes_equal(manifest + cursor, expected, expected_size) ||
            manifest[cursor + expected_size] != '"') {
            return 2U;
        }
        ++matches;
    }
    return matches;
}

static unsigned int enc_manifest_record_size(
    const char *manifest, size_t start, size_t end, const char *key,
    size_t key_size, size_t expected) {
    size_t position;
    unsigned int matches = 0U;
    for (position = start; position + key_size <= end; ++position) {
        size_t cursor;
        size_t used;
        size_t value;
        if (!enc_bytes_equal(manifest + position, key, key_size)) continue;
        cursor = position + key_size;
        while (cursor < end &&
               (manifest[cursor] == ' ' || manifest[cursor] == '\n' ||
                manifest[cursor] == '\r' || manifest[cursor] == '\t')) {
            ++cursor;
        }
        if (cursor == end || manifest[cursor++] != ':') return 2U;
        while (cursor < end &&
               (manifest[cursor] == ' ' || manifest[cursor] == '\n' ||
                manifest[cursor] == '\r' || manifest[cursor] == '\t')) {
            ++cursor;
        }
        if (!enc_parse_decimal(manifest + cursor, end - cursor, &used,
                               &value) ||
            value != expected) {
            return 2U;
        }
        ++matches;
    }
    return matches;
}

static unsigned int enc_manifest_dimensions(
    const char *path, size_t *width, size_t *height) {
    static const char dimensions_key[] = "\"dimensions\"";
    static const char filename_key[] = "\"filename\"";
    static const char byte_size_key[] = "\"byte_size\"";
    static const char outputs_key[] = "\"outputs\"";
    static const char gray_key[] = "\"gray8\"";
    static const char i420_key[] = "\"yuv420p8\"";
    static const char i444_key[] = "\"yuv444p8\"";
    char manifest_path[ENC_CLI_PATH_CAPACITY];
    char manifest[ENC_CLI_MANIFEST_CAPACITY];
    const char *base = enc_path_basename(path);
    const char *extension = enc_path_extension(path);
    const char *record_key;
    size_t record_key_size;
    size_t base_size = enc_text_size(base);
    size_t directory_size = (size_t)(base - path);
    size_t manifest_size;
    size_t position;
    size_t object_start = 0U;
    size_t object_end = 0U;
    size_t outputs_start = 0U;
    size_t outputs_end = 0U;
    size_t pixels;
    size_t expected_size;
    size_t parsed_width = 0U;
    size_t parsed_height = 0U;
    unsigned int dimension_count = 0U;
    if (directory_size + sizeof("manifest.json") > sizeof(manifest_path)) {
        return 2U;
    }
    for (position = 0U; position < directory_size; ++position) {
        manifest_path[position] = path[position];
    }
    {
        static const char name[] = "manifest.json";
        size_t index;
        for (index = 0U; index < sizeof(name); ++index) {
            manifest_path[directory_size + index] = name[index];
        }
    }
    if (enc_file_read_small(manifest_path, manifest, sizeof(manifest),
                            &manifest_size) != ENC_OK) {
        return 0U;
    }
    for (position = 0U; position + sizeof(dimensions_key) - 1U < manifest_size;
         ++position) {
        size_t cursor;
        size_t used;
        size_t first;
        size_t second;
        if (!enc_bytes_equal(manifest + position, dimensions_key,
                             sizeof(dimensions_key) - 1U)) {
            continue;
        }
        cursor = position + sizeof(dimensions_key) - 1U;
        while (cursor < manifest_size && manifest[cursor] != '[') ++cursor;
        if (cursor == manifest_size) continue;
        ++cursor;
        while (cursor < manifest_size &&
               (manifest[cursor] == ' ' || manifest[cursor] == '\n' ||
                manifest[cursor] == '\r' || manifest[cursor] == '\t')) {
            ++cursor;
        }
        if (!enc_parse_decimal(manifest + cursor, manifest_size - cursor,
                               &used, &first)) {
            continue;
        }
        cursor += used;
        while (cursor < manifest_size && manifest[cursor] != ',') ++cursor;
        if (cursor == manifest_size) continue;
        ++cursor;
        while (cursor < manifest_size &&
               (manifest[cursor] == ' ' || manifest[cursor] == '\n' ||
                manifest[cursor] == '\r' || manifest[cursor] == '\t')) {
            ++cursor;
        }
        if (!enc_parse_decimal(manifest + cursor, manifest_size - cursor,
                               &used, &second)) {
            continue;
        }
        parsed_width = first;
        parsed_height = second;
        ++dimension_count;
    }
    if (dimension_count != 1U) return 2U;
    if (!enc_size_multiply(parsed_width, parsed_height, &pixels)) return 2U;
    if (extension != 0 && enc_extension_equal(extension, ".i400")) {
        record_key = gray_key;
        record_key_size = sizeof(gray_key) - 1U;
        expected_size = pixels;
    } else if (extension != 0 && enc_extension_equal(extension, ".i420")) {
        size_t chroma_width;
        size_t chroma_height;
        size_t chroma_pixels;
        if (parsed_width == SIZE_MAX || parsed_height == SIZE_MAX) return 2U;
        chroma_width = (parsed_width + 1U) / 2U;
        chroma_height = (parsed_height + 1U) / 2U;
        if (!enc_size_multiply(chroma_width, chroma_height, &chroma_pixels) ||
            chroma_pixels > (SIZE_MAX - pixels) / 2U) {
            return 2U;
        }
        record_key = i420_key;
        record_key_size = sizeof(i420_key) - 1U;
        expected_size = pixels + 2U * chroma_pixels;
    } else if (extension != 0 && enc_extension_equal(extension, ".i444")) {
        if (pixels > SIZE_MAX / 3U) return 2U;
        record_key = i444_key;
        record_key_size = sizeof(i444_key) - 1U;
        expected_size = 3U * pixels;
    } else {
        return 2U;
    }
    if (enc_manifest_object(manifest, 0U, manifest_size, outputs_key,
                            sizeof(outputs_key) - 1U, &outputs_start,
                            &outputs_end) != 1U ||
        enc_manifest_object(manifest, outputs_start + 1U, outputs_end,
                            record_key, record_key_size, &object_start,
                            &object_end) != 1U ||
        enc_manifest_record_string(
            manifest, object_start, object_end, filename_key,
            sizeof(filename_key) - 1U, base, base_size) != 1U ||
        enc_manifest_record_size(
            manifest, object_start, object_end, byte_size_key,
            sizeof(byte_size_key) - 1U, expected_size) != 1U) {
        return 2U;
    }
    *width = parsed_width;
    *height = parsed_height;
    return 1U;
}

static EncCliInputFormat enc_input_format(const char *path) {
    const char *extension = enc_path_extension(path);
    if (extension == 0) return ENC_CLI_INPUT_UNKNOWN;
    if (enc_extension_equal(extension, ".i400")) return ENC_CLI_INPUT_I400;
    if (enc_extension_equal(extension, ".i420")) return ENC_CLI_INPUT_I420;
    if (enc_extension_equal(extension, ".i444")) return ENC_CLI_INPUT_I444;
    if (enc_extension_equal(extension, ".png")) return ENC_CLI_INPUT_PNG;
    if (enc_extension_equal(extension, ".bmp")) return ENC_CLI_INPUT_BMP;
    return ENC_CLI_INPUT_UNKNOWN;
}

static int enc_image_signature_format(const char *path,
                                      EncCliInputFormat *format) {
    EncFile file = {-1};
    uint8_t signature[8U];
    size_t size = 0U;
    EncStatus status;
    if (enc_file_open_read(path, &file) != ENC_OK) return -1;
    status = enc_file_read(&file, signature, sizeof(signature), &size);
    if (enc_file_close(&file) != ENC_OK && status == ENC_OK) {
        status = ENC_IO_ERROR;
    }
    if (status != ENC_OK) return -1;
    {
        static const uint8_t png[8U] = {
            0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU};
        size_t index;
        int png_match = size == sizeof(png);
        for (index = 0U; png_match && index < sizeof(png); ++index) {
            if (signature[index] != png[index]) png_match = 0;
        }
        if (png_match) {
            *format = ENC_CLI_INPUT_PNG;
            return 1;
        }
    }
    if (size >= 2U && signature[0] == 'B' && signature[1] == 'M') {
        *format = ENC_CLI_INPUT_BMP;
        return 1;
    }
    *format = ENC_CLI_INPUT_UNKNOWN;
    return 0;
}

EncStatus enc_cli_input_discover(const char *path,
                                 EncCliInputContract *contract,
                                 EncCliInputIssue *issue) {
    unsigned int filename_result;
    unsigned int manifest_result;
    size_t filename_width = 0U;
    size_t filename_height = 0U;
    size_t manifest_width = 0U;
    size_t manifest_height = 0U;
    if (path == 0 || contract == 0 || issue == 0) return ENC_INVALID_ARGUMENT;
    contract->format = enc_input_format(path);
    contract->width = 0U;
    contract->height = 0U;
    *issue = ENC_CLI_INPUT_ISSUE_NONE;
    if (contract->format == ENC_CLI_INPUT_UNKNOWN) {
        EncCliInputFormat content_format;
        int detected = enc_image_signature_format(path, &content_format);
        if (detected < 0) return ENC_IO_ERROR;
        if (!detected) {
            *issue = ENC_CLI_INPUT_ISSUE_UNKNOWN_FORMAT;
            return ENC_INVALID_ARGUMENT;
        }
        contract->format = content_format;
    }
    if (contract->format == ENC_CLI_INPUT_PNG ||
        contract->format == ENC_CLI_INPUT_BMP) {
        EncCliInputFormat content_format;
        int valid = enc_image_signature_format(path, &content_format);
        contract->chroma = ENC_CLI_CHROMA_I444;
        if (valid < 0) return ENC_IO_ERROR;
        if (!valid || content_format != contract->format) {
            *issue = ENC_CLI_INPUT_ISSUE_CONTENT_MISMATCH;
            return ENC_INVALID_ARGUMENT;
        }
        return ENC_OK;
    }
    contract->chroma = contract->format == ENC_CLI_INPUT_I400
                           ? ENC_CLI_CHROMA_I400
                           : contract->format == ENC_CLI_INPUT_I420
                                 ? ENC_CLI_CHROMA_I420
                                 : ENC_CLI_CHROMA_I444;
    filename_result =
        enc_filename_dimensions(path, &filename_width, &filename_height);
    manifest_result =
        enc_manifest_dimensions(path, &manifest_width, &manifest_height);
    if (filename_result > 1U || manifest_result > 1U) {
        *issue = ENC_CLI_INPUT_ISSUE_METADATA_AMBIGUOUS;
        return ENC_INVALID_ARGUMENT;
    }
    if (filename_result == 1U && manifest_result == 1U &&
        (filename_width != manifest_width ||
         filename_height != manifest_height)) {
        *issue = ENC_CLI_INPUT_ISSUE_METADATA_CONFLICT;
        return ENC_INVALID_ARGUMENT;
    }
    if (filename_result == 1U) {
        contract->width = filename_width;
        contract->height = filename_height;
    } else if (manifest_result == 1U) {
        contract->width = manifest_width;
        contract->height = manifest_height;
    } else {
        *issue = ENC_CLI_INPUT_ISSUE_METADATA_MISSING;
        return ENC_INVALID_ARGUMENT;
    }
    return ENC_OK;
}

EncStatus enc_cli_default_output(const char *path, char *output,
                                 size_t capacity) {
    const char *extension;
    size_t prefix;
    size_t position;
    static const char obu[] = ".obu";
    if (path == 0 || output == 0 || capacity == 0U) {
        return ENC_INVALID_ARGUMENT;
    }
    extension = enc_path_extension(path);
    prefix = extension != 0 &&
                     enc_input_format(path) != ENC_CLI_INPUT_UNKNOWN
                 ? (size_t)(extension - path)
                 : enc_text_size(path);
    if (prefix + sizeof(obu) > capacity) return ENC_OUT_OF_SPACE;
    for (position = 0U; position < prefix; ++position) {
        output[position] = path[position];
    }
    for (position = 0U; position < sizeof(obu); ++position) {
        output[prefix + position] = obu[position];
    }
    return ENC_OK;
}

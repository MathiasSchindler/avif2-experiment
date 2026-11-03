#include "qmatrix.h"

#include "internal/coeff_modules.h"

#include <stddef.h>

static Av2hStatus qmatrix_fail(Av2hError *error, Av2hStatus status, const Av2hObu *obu,
                               const Av2hBitReader *reader, uint32_t value) {
    if (error != NULL) {
        error->status = status;
        error->offset = reader != NULL ? av2h_bit_reader_offset(reader) : 0U;
        error->context = AV2H_CONTEXT_DECODE;
        error->obu_index = obu != NULL ? obu->index : SIZE_MAX;
        error->obu_type = AV2H_OBU_QUANTIZATION_MATRIX;
        error->parsed_value = value;
    }
    return status;
}

static int qmatrix_trailing_bits_valid(Av2hBitReader *reader) {
    size_t remaining = av2h_bit_reader_remaining(reader);
    size_t index;
    if (remaining == 0U || remaining > 8U || av2h_bit_reader_read(reader, 1U) != 1U) {
        return 0;
    }
    for (index = 1U; index < remaining; ++index) {
        if (av2h_bit_reader_read(reader, 1U) != 0U)
            return 0;
    }
    return reader->status == AV2H_OK;
}

static Av2hStatus qmatrix_read_fundamental(Av2hBitReader *reader, uint8_t *matrix, uint16_t width,
                                           uint16_t height, int symmetric) {
    uint16_t index;
    uint8_t previous = 32U;
    int repeat = 0;
    for (index = 0U; index < width * height; ++index) {
        uint16_t position = av2h_coeff_scan_position(index, width, height, 0U, 0U);
        uint16_t row = position / width;
        uint16_t column = position % width;
        if (symmetric && column > row) {
            previous = matrix[column * width + row];
        } else if (!repeat) {
            int32_t delta = av2h_bit_reader_svlc(reader);
            int32_t coefficient;
            if (reader->status != AV2H_OK)
                return reader->status;
            if (delta < -128 || delta > 127)
                return AV2H_INVALID_DATA;
            coefficient = ((int32_t)previous + delta + 256) & 255;
            if (coefficient == 0) {
                repeat = 1;
            } else {
                previous = (uint8_t)coefficient;
            }
        }
        matrix[position] = previous;
    }
    return AV2H_OK;
}

static Av2hStatus qmatrix_read_custom_definition(Av2hBitReader *reader, Av2hFrameState *frame,
                                                 uint8_t id, uint8_t plane_count) {
    static const uint16_t widths[3] = {8U, 8U, 4U};
    static const uint16_t heights[3] = {8U, 4U, 8U};
    static const uint16_t offsets[3] = {0U, 64U, 96U};
    uint8_t validation_definition[AV2H_FRAME_PLANE_COUNT][AV2H_CUSTOM_QM_FUNDAMENTAL_SIZE];
    uint8_t (*definition)[AV2H_CUSTOM_QM_FUNDAMENTAL_SIZE] =
        frame->custom_qm != NULL ? frame->custom_qm[id] : validation_definition;
    unsigned int shape;
    unsigned int plane;
    if (frame->custom_qm == NULL)
        av2h_memory_fill(validation_definition, 0U, sizeof(validation_definition));
    for (shape = 0U; shape < 3U; ++shape) {
        for (plane = 0U; plane < plane_count; ++plane) {
            uint8_t *matrix = &definition[plane][offsets[shape]];
            if (plane > 0U && av2h_bit_reader_read(reader, 1U) != 0U) {
                av2h_memory_copy(matrix, &definition[plane - 1U][offsets[shape]],
                                 (size_t)widths[shape] * heights[shape]);
                continue;
            }
            if (shape == 2U && av2h_bit_reader_read(reader, 1U) != 0U) {
                uint16_t row;
                uint16_t column;
                const uint8_t *source = &definition[plane][64U];
                for (row = 0U; row < 8U; ++row) {
                    for (column = 0U; column < 4U; ++column) {
                        matrix[row * 4U + column] = source[column * 8U + row];
                    }
                }
                continue;
            }
            {
                int symmetric = shape == 0U ? (int)av2h_bit_reader_read(reader, 1U) : 0;
                Av2hStatus status = qmatrix_read_fundamental(reader, matrix, widths[shape],
                                                             heights[shape], symmetric);
                if (status != AV2H_OK)
                    return status;
            }
        }
    }
    return reader->status;
}

static Av2hStatus qmatrix_read_definition(Av2hBitReader *reader, Av2hFrameState *frame, uint8_t id,
                                          uint8_t plane_count,
                                          Av2hQMatrixSyntaxReferences *references) {
    if (av2h_bit_reader_read(reader, 1U) != 0U)
        return reader->status;
    frame->custom_qm_bitmap |= (uint16_t)(1U << id);
    frame->custom_qm_plane_count[id] = plane_count;
    if (references != NULL) {
        Av2hQMatrixSyntaxReference *reference = &references->definition[id];
        reference->payload = (Av2hSpan){reader->data, reader->size};
        reference->bit_position = reader->bit_position;
        reference->byte_offset = reader->base_offset;
    }
    return qmatrix_read_custom_definition(reader, frame, id, plane_count);
}

void av2h_qmatrix_state_reset(Av2hFrameState *frame) {
    if (frame == NULL)
        return;
    frame->custom_qm_definition_bitmap = 0U;
    frame->custom_qm_bitmap = 0U;
    av2h_memory_fill(frame->custom_qm_plane_count, 0U, sizeof(frame->custom_qm_plane_count));
    if (frame->custom_qm != NULL)
        av2h_memory_fill(frame->custom_qm, 0U, sizeof(((Av2hFrameSyntaxStorage *)0)->custom_qm));
}

Av2hStatus av2h_qmatrix_obu_parse(Av2hSpan input, const Av2hObu *obu, Av2hFrameState *frame,
                                  Av2hQMatrixSyntaxReferences *references, Av2hError *error) {
    Av2hBitReader reader;
    uint16_t bitmap;
    uint8_t plane_count;
    uint8_t id;
    if (obu == NULL || frame == NULL || input.data == NULL ||
        obu->type != AV2H_OBU_QUANTIZATION_MATRIX || obu->payload_offset > input.size ||
        obu->payload_size > input.size - obu->payload_offset) {
        return qmatrix_fail(error, AV2H_INVALID_ARGUMENT, obu, NULL, 0U);
    }
    av2h_bit_reader_init(&reader, input.data + obu->payload_offset, obu->payload_size,
                         obu->payload_offset);
    bitmap = (uint16_t)av2h_bit_reader_read(&reader, 15U);
    plane_count = av2h_bit_reader_read(&reader, 1U) != 0U ? 3U : 1U;
    if (bitmap == 0U) {
        if (frame->custom_qm_definition_bitmap != 0U) {
            return qmatrix_fail(error, AV2H_INVALID_DATA, obu, &reader, 0U);
        }
        frame->custom_qm_definition_bitmap = 0x7fffU;
        frame->custom_qm_bitmap = 0U;
    } else {
        if ((frame->custom_qm_definition_bitmap & bitmap) != 0U ||
            frame->custom_qm_definition_bitmap == 0x7fffU) {
            return qmatrix_fail(error, AV2H_INVALID_DATA, obu, &reader, bitmap);
        }
        frame->custom_qm_definition_bitmap |= bitmap;
        for (id = 0U; id < AV2H_CUSTOM_QM_COUNT; ++id) {
            Av2hStatus status;
            if ((bitmap & (uint16_t)(1U << id)) == 0U)
                continue;
            status = qmatrix_read_definition(&reader, frame, id, plane_count, references);
            if (status != AV2H_OK) {
                return qmatrix_fail(error, status, obu, &reader, id);
            }
        }
    }
    if (!qmatrix_trailing_bits_valid(&reader)) {
        return qmatrix_fail(error, reader.status == AV2H_OK ? AV2H_INVALID_DATA : reader.status,
                            obu, &reader, 0U);
    }
    return AV2H_OK;
}

Av2hStatus av2h_qmatrix_ledger_parse(Av2hSpan input, const Av2hObuLedger *ledger,
                                     Av2hFrameState *frame, Av2hQMatrixSyntaxReferences *references,
                                     Av2hError *error) {
    size_t index;
    if (ledger == NULL || frame == NULL)
        return AV2H_INVALID_ARGUMENT;
    for (index = 0U; index < ledger->count; ++index) {
        Av2hStatus status;
        if (ledger->entries[index].type != AV2H_OBU_QUANTIZATION_MATRIX)
            continue;
        status = av2h_qmatrix_obu_parse(input, &ledger->entries[index], frame, references, error);
        if (status != AV2H_OK)
            return status;
    }
    return AV2H_OK;
}

Av2hStatus av2h_qmatrix_materialize(Av2hFrameState *frame,
                                    const Av2hQMatrixSyntaxReferences *references,
                                    Av2hError *error) {
    unsigned int id;
    if (frame == NULL || references == NULL || frame->custom_qm == NULL)
        return qmatrix_fail(error, AV2H_INVALID_ARGUMENT, NULL, NULL, 0U);
    av2h_memory_fill(frame->custom_qm, 0U, sizeof(((Av2hFrameSyntaxStorage *)0)->custom_qm));
    for (id = 0U; id < AV2H_CUSTOM_QM_COUNT; ++id) {
        const Av2hQMatrixSyntaxReference *reference = &references->definition[id];
        Av2hBitReader reader;
        Av2hStatus status;
        if ((frame->custom_qm_bitmap & (uint16_t)(1U << id)) == 0U)
            continue;
        if (reference->payload.data == NULL ||
            (frame->custom_qm_plane_count[id] != 1U && frame->custom_qm_plane_count[id] != 3U))
            return qmatrix_fail(error, AV2H_INVALID_ARGUMENT, NULL, NULL, id);
        av2h_bit_reader_init(&reader, reference->payload.data, reference->payload.size,
                             reference->byte_offset);
        (void)av2h_bit_reader_skip(&reader, reference->bit_position);
        status = qmatrix_read_custom_definition(&reader, frame, (uint8_t)id,
                                                frame->custom_qm_plane_count[id]);
        if (status != AV2H_OK)
            return qmatrix_fail(error, status, NULL, &reader, id);
    }
    return AV2H_OK;
}

Av2hStatus av2h_qmatrix_references_validate(const Av2hFrameState *frame) {
    size_t matrix;
    unsigned int plane;
    if (frame == NULL)
        return AV2H_INVALID_ARGUMENT;
    if (frame->using_qmatrix == 0U)
        return AV2H_OK;
    for (matrix = 0U; matrix < frame->picture_qm_count; ++matrix) {
        for (plane = 0U; plane < frame->plane_count; ++plane) {
            uint8_t id = plane == 0U   ? frame->qm_y[matrix]
                         : plane == 1U ? frame->qm_u[matrix]
                                       : frame->qm_v[matrix];
            if (id < AV2H_CUSTOM_QM_COUNT &&
                (frame->custom_qm_bitmap & (uint16_t)(1U << id)) != 0U &&
                plane >= frame->custom_qm_plane_count[id]) {
                return AV2H_INVALID_DATA;
            }
        }
    }
    return AV2H_OK;
}
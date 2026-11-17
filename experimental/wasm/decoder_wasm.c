#include "av2heif.h"
#include "decoder/frame.h"
#include "decoder/frame_output.h"
#include "decoder/internal/frame_workspace.h"
#include "decoder/qmatrix.h"
#include "decoder/sequence.h"

#define AV2H_WASM_PAGE_SIZE 65536U
#define AV2H_WASM_MAX_INPUT_SIZE (64U * 1024U * 1024U)
#define AV2H_WASM_UINTPTR_MAX ((uintptr_t)-1)

extern unsigned char __heap_base;

typedef struct {
    uint32_t output_pointer;
    uint32_t output_size;
    uint32_t width;
    uint32_t height;
    uint32_t status;
    uint32_t cause;
    uint32_t stage;
    uint32_t channel_count;
    uint32_t chroma_format_idc;
    uint32_t bit_depth;
    uint32_t bytes_per_sample;
    uint32_t byte_order;
} Av2hWasmResult;

static uintptr_t heap_position;
static Av2hWasmResult wasm_result;

static uintptr_t align_up(uintptr_t value, size_t alignment) {
    uintptr_t mask = (uintptr_t)alignment - 1U;
    if (alignment == 0U || (alignment & (alignment - 1U)) != 0U ||
        value > AV2H_WASM_UINTPTR_MAX - mask) {
        return 0U;
    }
    return (value + mask) & ~mask;
}

static uint8_t ensure_memory(uintptr_t end) {
    size_t pages = __builtin_wasm_memory_size(0);
    uintptr_t available = (uintptr_t)pages * AV2H_WASM_PAGE_SIZE;
    size_t additional_pages;
    if (end <= available) return 1U;
    additional_pages = (size_t)(
        (end - available + AV2H_WASM_PAGE_SIZE - 1U) /
        AV2H_WASM_PAGE_SIZE);
    return __builtin_wasm_memory_grow(0, additional_pages) != (size_t)-1;
}

static void *allocate_bytes(size_t size, size_t alignment) {
    uintptr_t start;
    uintptr_t end;
    if (heap_position == 0U) heap_position = (uintptr_t)&__heap_base;
    start = align_up(heap_position, alignment);
    if (start == 0U || size > AV2H_WASM_UINTPTR_MAX - start) return 0;
    end = start + size;
    if (!ensure_memory(end)) return 0;
    heap_position = end;
    return (void *)start;
}

static const Av2hObu *find_first_obu(const Av2hObuLedger *ledger,
                                     uint8_t type) {
    size_t index;
    for (index = 0U; index < ledger->count; ++index) {
        if (ledger->entries[index].type == type) return &ledger->entries[index];
    }
    return 0;
}

static int find_unique_obu(const Av2hObuLedger *ledger, uint8_t type,
                           const Av2hObu **found) {
    size_t index;
    const Av2hObu *match = 0;
    for (index = 0U; index < ledger->count; ++index) {
        if (ledger->entries[index].type == type) {
            if (match != 0) return 0;
            match = &ledger->entries[index];
        }
    }
    *found = match;
    return match != 0;
}

__attribute__((export_name("av2h_reset")))
void av2h_wasm_reset(void) {
    heap_position = (uintptr_t)&__heap_base;
    wasm_result = (Av2hWasmResult){ 0 };
}

__attribute__((export_name("av2h_alloc")))
uint32_t av2h_wasm_alloc(uint32_t size) {
    void *memory;
    if (size == 0U || size > AV2H_WASM_MAX_INPUT_SIZE) return 0U;
    memory = allocate_bytes(size, 16U);
    return (uint32_t)(uintptr_t)memory;
}

__attribute__((export_name("av2h_decode")))
uint32_t av2h_wasm_decode(uint32_t input_pointer, uint32_t input_size) {
    Av2hLimits limits;
    Av2hObu entries[16U];
    Av2hObuLedger ledger = { entries, 16U, 0U };
    Av2hError error;
    const Av2hObu *sequence_obu = 0;
    const Av2hObu *frame_obu = 0;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hFrameDecodeWorkspace workspace;
    Av2hFilterWorkspace filter_workspace;
    Av2hFilterWorkspaceStorage filter_storage;
    Av2hFrameDecodeWorkspaceLayout workspace_layout;
    Av2hFrameDecodeResult decode_result = { 0 };
    uint8_t *workspace_memory;
    uint8_t *output;
    size_t current_pages = __builtin_wasm_memory_size(0);
    uintptr_t current_memory_bytes = (uintptr_t)current_pages * AV2H_WASM_PAGE_SIZE;
    uintptr_t heap_base = (uintptr_t)&__heap_base;
    uintptr_t input_start = (uintptr_t)input_pointer;
    size_t pixels;
    size_t output_size;
    size_t bytes_per_sample;
    Av2hSpan input;
    Av2hStatus status;

    wasm_result = (Av2hWasmResult){ 0 };
    if (input_pointer == 0U || input_size == 0U ||
        input_size > AV2H_WASM_MAX_INPUT_SIZE ||
        input_start < heap_base || input_start >= current_memory_bytes ||
        (uintptr_t)input_size > current_memory_bytes - input_start) {
        wasm_result.status = AV2H_INVALID_ARGUMENT;
        return wasm_result.status;
    }
    if (heap_position < input_start + (uintptr_t)input_size) {
        heap_position = input_start + (uintptr_t)input_size;
    }
    av2h_limits_default(&limits);
    input = (Av2hSpan){ (const uint8_t *)input_start, input_size };
    status = av2h_obu_parse(input, &ledger, &error);
    if (status != AV2H_OK) goto done;
    if (!find_unique_obu(&ledger, AV2H_OBU_SEQUENCE_HEADER, &sequence_obu)) {
        status = AV2H_INVALID_DATA;
        goto done;
    }
    frame_obu = find_first_obu(&ledger, AV2H_OBU_CLOSED_LOOP_KEY);
    if (frame_obu == 0) {
        status = AV2H_INVALID_DATA;
        goto done;
    }
    status = av2h_sequence_state_parse(input, sequence_obu, &sequence, &error);
    if (status != AV2H_OK) goto done;
    if ((sequence.info.bit_depth_idc == 0U &&
         sequence.info.chroma_format_idc != 0U &&
         sequence.info.chroma_format_idc != 2U) ||
        (sequence.info.bit_depth_idc != 0U &&
         (sequence.info.bit_depth_idc != 1U ||
          sequence.info.chroma_format_idc > 2U))) {
        status = AV2H_UNSUPPORTED;
        goto done;
    }
    bytes_per_sample = sequence.info.bit_depth_idc == 0U ? 2U : 1U;
    status = av2h_frame_header_closed_key_parse(
        (Av2hSpan){ input.data + frame_obu->payload_offset,
                    frame_obu->payload_size },
        &sequence, &frame, NULL, NULL, &error);
    if (status != AV2H_OK) goto done;
    if (frame.width > limits.max_width || frame.height > limits.max_height) {
        status = AV2H_LIMIT_EXCEEDED;
        goto done;
    }
    if (!av2h_size_multiply((size_t)frame.width, (size_t)frame.height,
                            &pixels)) {
        status = AV2H_OVERFLOW;
        goto done;
    }
    if (pixels > limits.max_pixels) {
        status = AV2H_LIMIT_EXCEEDED;
        goto done;
    }
    av2h_qmatrix_state_reset(&frame);
    status = av2h_qmatrix_ledger_parse(input, &ledger, &frame, NULL, &error);
    if (status == AV2H_OK) status = av2h_qmatrix_references_validate(&frame);
    if (status != AV2H_OK) goto done;
    status = av2h_tile_groups_ledger_validate(input, &ledger, &frame);
    if (status != AV2H_OK) goto done;
    status = av2h_planar_output_size(
        &frame, bytes_per_sample, &output_size);
    if (status != AV2H_OK) goto done;
    if (output_size > UINT32_MAX) {
        status = AV2H_OVERFLOW;
        goto done;
    }
    if (output_size > limits.max_output_size) {
        status = AV2H_LIMIT_EXCEEDED;
        goto done;
    }
    status = av2h_frame_decode_workspace_query(
        &sequence, &frame, bytes_per_sample, NULL, &workspace_layout);
    if (status != AV2H_OK) goto done;
    if (workspace_layout.size_bytes > limits.max_workspace_size) {
        status = AV2H_LIMIT_EXCEEDED;
        goto done;
    }
    workspace_memory = allocate_bytes(
        workspace_layout.size_bytes, workspace_layout.alignment);
    output = allocate_bytes(output_size, 16U);
    if (workspace_memory == 0 || output == 0) {
        status = AV2H_OUT_OF_MEMORY;
        goto done;
    }
    status = av2h_frame_decode_workspace_init(
        &sequence, &frame, bytes_per_sample, &workspace_layout,
        workspace_memory, workspace_layout.size_bytes,
        &workspace, &filter_workspace, &filter_storage, NULL);
    if (status != AV2H_OK) goto done;
    {
        Av2hDecodePreparation *preparation =
            &workspace.decode_scratch->preparation;
        av2h_memory_copy(&preparation->sequence, &sequence, sizeof(sequence));
        preparation->group = &workspace.decode_scratch->group;
        status = av2h_frame_header_closed_key_parse(
            (Av2hSpan){ input.data + frame_obu->payload_offset,
                        frame_obu->payload_size },
            &preparation->sequence, &preparation->frame,
            &workspace.decode_scratch->syntax, NULL, &error);
        if (status != AV2H_OK) goto done;
        av2h_qmatrix_state_reset(&preparation->frame);
        status = av2h_qmatrix_ledger_parse(
            input, &ledger, &preparation->frame, NULL, &error);
        if (status == AV2H_OK) {
            status = av2h_qmatrix_references_validate(&preparation->frame);
        }
        if (status != AV2H_OK) goto done;
        status = av2h_tile_groups_ledger_parse(
            input, &ledger, &preparation->frame, preparation->group);
        if (status != AV2H_OK) goto done;
        status = av2h_decode_frame_group_planar_private(
            preparation->group, &preparation->sequence, &preparation->frame,
            &workspace, output, output_size, &decode_result, NULL);
        if (status != AV2H_OK) goto done;
    }
    wasm_result.output_pointer = (uint32_t)(uintptr_t)output;
    wasm_result.output_size = (uint32_t)output_size;
    wasm_result.width = frame.width;
    wasm_result.height = frame.height;
    wasm_result.channel_count = av2h_frame_plane_count(&frame);
    wasm_result.chroma_format_idc = frame.chroma_format_idc;
    wasm_result.bit_depth = bytes_per_sample == 2U ? 10U : 8U;
    wasm_result.bytes_per_sample = (uint32_t)bytes_per_sample;
    wasm_result.byte_order = bytes_per_sample == 2U ?
        AV2H_BYTE_ORDER_LITTLE_ENDIAN : AV2H_BYTE_ORDER_NOT_APPLICABLE;

done:
    wasm_result.status = status;
    wasm_result.cause = decode_result.cause;
    wasm_result.stage = decode_result.stage;
    return status;
}

__attribute__((export_name("av2h_result_pointer")))
uint32_t av2h_wasm_result_pointer(void) {
    return (uint32_t)(uintptr_t)&wasm_result;
}

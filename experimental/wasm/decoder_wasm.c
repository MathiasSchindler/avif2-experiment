#include "av2heif.h"
#include "decoder/frame.h"
#include "decoder/frame_output.h"
#include "decoder/internal/frame_workspace.h"
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

static const Av2hObu *find_obu(const Av2hObuLedger *ledger, uint8_t type) {
    size_t index;
    for (index = 0U; index < ledger->count; ++index) {
        if (ledger->entries[index].type == type) return &ledger->entries[index];
    }
    return 0;
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
    Av2hObu entries[4U];
    Av2hObuLedger ledger = { entries, 4U, 0U };
    Av2hError error;
    const Av2hObu *sequence_obu;
    const Av2hObu *frame_obu;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hTileGroup group;
    Av2hFrameDecodeWorkspace workspace;
    Av2hFilterWorkspace filter_workspace;
    Av2hFilterWorkspaceStorage filter_storage;
    Av2hFrameDecodeWorkspaceLayout workspace_layout;
    Av2hFrameDecodeResult decode_result = { 0 };
    uint8_t *workspace_memory;
    uint8_t *output;
    size_t output_size;
    Av2hSpan input;
    Av2hStatus status;

    wasm_result = (Av2hWasmResult){ 0 };
    if (input_pointer == 0U || input_size == 0U ||
        input_size > AV2H_WASM_MAX_INPUT_SIZE) {
        wasm_result.status = AV2H_INVALID_ARGUMENT;
        return wasm_result.status;
    }
    input = (Av2hSpan){ (const uint8_t *)(uintptr_t)input_pointer, input_size };
    status = av2h_obu_parse(input, &ledger, &error);
    if (status != AV2H_OK) goto done;
    sequence_obu = find_obu(&ledger, AV2H_OBU_SEQUENCE_HEADER);
    frame_obu = find_obu(&ledger, AV2H_OBU_CLOSED_LOOP_KEY);
    if (sequence_obu == 0 || frame_obu == 0) {
        status = AV2H_INVALID_DATA;
        goto done;
    }
    status = av2h_sequence_state_parse(input, sequence_obu, &sequence, &error);
    if (status != AV2H_OK) goto done;
    status = av2h_frame_header_closed_key_parse(
        (Av2hSpan){ input.data + frame_obu->payload_offset,
                    frame_obu->payload_size },
        &sequence, &frame);
    if (status != AV2H_OK) goto done;
    status = av2h_tile_group_single_parse(
        (Av2hSpan){ input.data + frame_obu->payload_offset,
                    frame_obu->payload_size },
        frame.uncompressed_header_bits, &group);
    if (status != AV2H_OK) goto done;
    status = av2h_i420_output_size(&frame, &output_size);
    if (status != AV2H_OK) goto done;
    if (output_size > UINT32_MAX) {
        status = AV2H_OVERFLOW;
        goto done;
    }
    status = av2h_frame_decode_workspace_query(
        &sequence, &frame, &workspace_layout);
    if (status != AV2H_OK) goto done;
    workspace_memory = allocate_bytes(
        workspace_layout.size, workspace_layout.alignment);
    output = allocate_bytes(output_size, 16U);
    if (workspace_memory == 0 || output == 0) {
        status = AV2H_OUT_OF_MEMORY;
        goto done;
    }
    status = av2h_frame_decode_workspace_init(
        &sequence, &frame, &workspace_layout,
        workspace_memory, workspace_layout.size,
        &workspace, &filter_workspace, &filter_storage);
    if (status != AV2H_OK) goto done;
    status = av2h_decode_frame_i420_private(
        group.entropy, &sequence, &frame, &workspace,
        output, output_size, &decode_result);
    if (status != AV2H_OK) goto done;
    wasm_result.output_pointer = (uint32_t)(uintptr_t)output;
    wasm_result.output_size = (uint32_t)output_size;
    wasm_result.width = frame.width;
    wasm_result.height = frame.height;
    wasm_result.channel_count = av2h_frame_plane_count(&frame);

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

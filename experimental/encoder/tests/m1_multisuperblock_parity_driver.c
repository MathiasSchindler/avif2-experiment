#include "encoder/ac8x8.h"

#include <stdio.h>

#define WIDTH 128U
#define HEIGHT 128U
#define Y_SIZE (WIDTH * HEIGHT)
#define CHROMA_WIDTH (WIDTH / 2U)
#define CHROMA_HEIGHT (HEIGHT / 2U)
#define CHROMA_SIZE (CHROMA_WIDTH * CHROMA_HEIGHT)
#define FRAME_SIZE (Y_SIZE + 2U * CHROMA_SIZE)
#define WORKSPACE_CAPACITY 300000U
#define STREAM_CAPACITY 65536U

typedef union {
    uint64_t alignment;
    uint8_t bytes[WORKSPACE_CAPACITY];
} AlignedWorkspace;

static uint8_t input_bytes[FRAME_SIZE];
static uint8_t reconstruction_bytes[FRAME_SIZE];
static AlignedWorkspace workspace;
static uint8_t stream[STREAM_CAPACITY];

static int write_file(const char *path, const uint8_t *data, size_t size) {
    FILE *file = fopen(path, "wb");
    size_t written;
    int close_status;
    if (file == 0) return 0;
    written = fwrite(data, 1U, size, file);
    close_status = fclose(file);
    return written == size && close_status == 0;
}

static void generate_input(void) {
    size_t y;
    size_t x;
    for (y = 0U; y < HEIGHT; ++y) {
        for (x = 0U; x < WIDTH; ++x) {
            input_bytes[y * WIDTH + x] =
                (uint8_t)(11U * x + 7U * y + (x * y) / 17U);
        }
    }
    for (y = 0U; y < CHROMA_HEIGHT; ++y) {
        for (x = 0U; x < CHROMA_WIDTH; ++x) {
            input_bytes[Y_SIZE + y * CHROMA_WIDTH + x] =
                (uint8_t)(64U + 9U * x + 3U * y);
            input_bytes[Y_SIZE + CHROMA_SIZE + y * CHROMA_WIDTH + x] =
                (uint8_t)(192U + 5U * x + 13U * y);
        }
    }
}

int main(int argc, char **argv) {
    EncM3Image8 input;
    EncM3MutableImage8 reconstruction;
    EncAc8x8YuvWorkspaceLayout layout;
    EncAc8x8YuvResult result;
    EncStatus status;
    if (argc != 3) return 2;
    generate_input();
    input.format = ENC_M3_PIXEL_I420;
    input.width = WIDTH;
    input.height = HEIGHT;
    input.planes[0U] = (EncM3Plane8){input_bytes, WIDTH, Y_SIZE};
    input.planes[1U] =
        (EncM3Plane8){input_bytes + Y_SIZE, CHROMA_WIDTH, CHROMA_SIZE};
    input.planes[2U] = (EncM3Plane8){
        input_bytes + Y_SIZE + CHROMA_SIZE, CHROMA_WIDTH, CHROMA_SIZE
    };
    reconstruction.format = ENC_M3_PIXEL_I420;
    reconstruction.width = WIDTH;
    reconstruction.height = HEIGHT;
    reconstruction.planes[0U] =
        (EncM3MutablePlane8){reconstruction_bytes, WIDTH, Y_SIZE};
    reconstruction.planes[1U] = (EncM3MutablePlane8){
        reconstruction_bytes + Y_SIZE, CHROMA_WIDTH, CHROMA_SIZE
    };
    reconstruction.planes[2U] = (EncM3MutablePlane8){
        reconstruction_bytes + Y_SIZE + CHROMA_SIZE,
        CHROMA_WIDTH, CHROMA_SIZE
    };
    status = enc_ac8x8_query_yuv8_workspace(
        ENC_M3_PIXEL_I420, WIDTH, HEIGHT, &layout);
    if (status != ENC_OK || layout.size > sizeof(workspace.bytes) ||
        layout.stream_capacity > sizeof(stream)) {
        return 3;
    }
    status = enc_ac8x8_yuv8_q_workspace(
        &input, 100U, ENC_AC8X8_COEFFICIENT_SEARCH_RD,
        workspace.bytes, sizeof(workspace.bytes), stream, sizeof(stream),
        &reconstruction, &result);
    if (status != ENC_OK) return 4;
    if (!write_file(argv[1], stream, result.stream_size) ||
        !write_file(
            argv[2], reconstruction_bytes, sizeof(reconstruction_bytes))) {
        return 5;
    }
    printf(
        "%zu %llu %llu %llu %d %d\n",
        result.stream_size,
        (unsigned long long)result.squared_error[0U],
        (unsigned long long)result.squared_error[1U],
        (unsigned long long)result.squared_error[2U],
        (int)result.u_qdelta, (int)result.v_qdelta);
    return 0;
}

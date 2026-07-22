#include "decoder/filter.h"

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

static void initialize_planes(Av2hFilterPlanes *planes,
                              uint8_t *y,
                              uint8_t *u,
                              uint8_t *v) {
    planes->plane[0] = (Av2hFilterPlane){ y, 8U, 8U, 8U };
    planes->plane[1] = (Av2hFilterPlane){ u, 4U, 4U, 4U };
    planes->plane[2] = (Av2hFilterPlane){ v, 4U, 4U, 4U };
}

static void test_deblock_edge(void) {
    uint8_t pixels[16U * 12U];
    Av2hFilterPlane plane = { pixels, 16U, 16U, 12U };
    size_t row;
    size_t column;
    for (row = 0U; row < 12U; ++row) {
      for (column = 0U; column < 16U; ++column) {
        pixels[row * 16U + column] = column < 8U ? 100U : 104U;
      }
    }
    CHECK(av2h_deblock_edge(&plane, 8U, 2U, 1U, 4U, 4U, 3U, 3U) ==
        AV2H_OK);
    for (row = 2U; row < 6U; ++row) {
      CHECK(pixels[row * 16U + 7U] == 101U);
      CHECK(pixels[row * 16U + 8U] == 103U);
    }
    CHECK(pixels[1U * 16U + 7U] == 100U);
    CHECK(pixels[6U * 16U + 8U] == 104U);

    CHECK(av2h_deblock_edge(&plane, 2U, 4U, 0U, 4U, 4U, 3U, 3U) ==
        AV2H_OK);
    CHECK(av2h_deblock_edge(&plane, 2U, 4U, 0U, 4U, 4U, 0U, 3U) ==
        AV2H_OK);
    CHECK(av2h_deblock_edge(&plane, 2U, 2U, 0U, 4U, 4U, 3U, 3U) ==
        AV2H_INVALID_ARGUMENT);
    CHECK(av2h_deblock_edge(&plane, 8U, 2U, 1U, 5U, 6U, 3U, 3U) ==
        AV2H_INVALID_ARGUMENT);
}

static void test_deblock_frame(void) {
    uint8_t y[16U * 8U];
    uint8_t u[8U * 4U];
    uint8_t v[8U * 4U];
    Av2hDeblockCell y_cells[4U * 2U];
    Av2hDeblockCell u_cells[2U];
    Av2hDeblockCell v_cells[2U];
    Av2hFilterPlanes planes;
    Av2hDeblockMap map;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    size_t row;
    size_t column;
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&map, 0U, sizeof(map));
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            y[row * 16U + column] = column < 8U ? 100U : 104U;
        }
    }
    av2h_memory_fill(u, 90U, sizeof(u));
    av2h_memory_fill(v, 80U, sizeof(v));
    planes.plane[0] = (Av2hFilterPlane){ y, 16U, 16U, 8U };
    planes.plane[1] = (Av2hFilterPlane){ u, 8U, 8U, 4U };
    planes.plane[2] = (Av2hFilterPlane){ v, 8U, 8U, 4U };
    for (row = 0U; row < 2U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            Av2hDeblockCell *cell = &y_cells[row * 4U + column];
            *cell = (Av2hDeblockCell){
                (uint16_t)column, (uint16_t)row,
                (uint16_t)column, (uint16_t)row,
                4U, 4U, 128U, 0U, 0U
            };
        }
    }
    for (column = 0U; column < 2U; ++column) {
        u_cells[column] = (Av2hDeblockCell){
            (uint16_t)column, 0U, (uint16_t)column, 0U,
            4U, 4U, 128U, 0U, 0U
        };
        v_cells[column] = u_cells[column];
    }
    map.plane[0] = y_cells;
    map.plane[1] = u_cells;
    map.plane[2] = v_cells;
    map.stride[0] = 4U;
    map.stride[1] = 2U;
    map.stride[2] = 2U;
    map.columns[0] = 4U;
    map.columns[1] = 2U;
    map.columns[2] = 2U;
    map.rows[0] = 2U;
    map.rows[1] = 1U;
    map.rows[2] = 1U;
    frame.width = 16U;
    frame.height = 8U;
    frame.deblock_enabled[0] = 1U;
    CHECK(av2h_deblock_filter_frame(
              &planes, &sequence, &frame, &map) == AV2H_OK);
    for (row = 0U; row < 8U; ++row) {
        CHECK(y[row * 16U + 7U] == 101U);
        CHECK(y[row * 16U + 8U] == 103U);
    }
    map.columns[0] = 3U;
    y[0] = 77U;
    CHECK(av2h_deblock_filter_frame(
              &planes, &sequence, &frame, &map) == AV2H_INVALID_ARGUMENT);
    CHECK(y[0] == 77U);
}

static void test_cdef(void) {
    uint8_t source_data[16U * 16U];
    uint8_t destination_data[16U * 16U];
    Av2hFilterPlane source = { source_data, 16U, 16U, 16U };
    Av2hFilterPlane destination = {
        destination_data, 16U, 16U, 16U
    };
    uint8_t direction;
    uint32_t variance;
    size_t row;
    size_t column;
    for (row = 0U; row < 16U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            source_data[row * 16U + column] = 128U;
            destination_data[row * 16U + column] = 128U;
        }
    }
    CHECK(av2h_cdef_find_direction(&source, 0U, 0U, &direction, &variance) ==
          AV2H_OK);
    CHECK(direction == 0U && variance == 0U);
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            source_data[row * 16U + column] = (uint8_t)(column * 16U);
        }
    }
    CHECK(av2h_cdef_find_direction(&source, 0U, 0U, &direction, &variance) ==
          AV2H_OK);
    CHECK(direction == 6U && variance != 0U);
    for (row = 0U; row < 16U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            source_data[row * 16U + column] = 100U;
            destination_data[row * 16U + column] = 100U;
        }
    }
    source_data[8U * 16U + 8U] = 140U;
    destination_data[8U * 16U + 8U] = 140U;
    CHECK(av2h_cdef_filter_block(
              &destination, &source, 8U, 8U, 8U, 8U, 0U,
              14U, 0U, 5U, 2U, 262144U) == AV2H_OK);
    CHECK(destination_data[8U * 16U + 8U] == 137U);
    CHECK(source_data[8U * 16U + 8U] == 140U);
    CHECK(av2h_cdef_find_direction(&source, 9U, 9U, &direction, &variance) ==
          AV2H_INVALID_ARGUMENT);
}

static void test_cdef_frame(void) {
    uint8_t source_y[16U * 16U];
    uint8_t source_u[8U * 8U];
    uint8_t source_v[8U * 8U];
    uint8_t destination_y[16U * 16U];
    uint8_t destination_u[8U * 8U];
    uint8_t destination_v[8U * 8U];
    Av2hFilterPlanes source;
    Av2hFilterPlanes destination;
    Av2hFrameState frame;
    size_t index;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    for (index = 0U; index < sizeof(source_y); ++index) {
        source_y[index] = 100U;
        destination_y[index] = 100U;
    }
    for (index = 0U; index < sizeof(source_u); ++index) {
        source_u[index] = 100U;
        source_v[index] = 100U;
        destination_u[index] = 100U;
        destination_v[index] = 100U;
    }
    source_y[4U * 16U + 4U] = 108U;
    destination_y[4U * 16U + 4U] = 108U;
    source.plane[0] = (Av2hFilterPlane){ source_y, 16U, 16U, 16U };
    source.plane[1] = (Av2hFilterPlane){ source_u, 8U, 8U, 8U };
    source.plane[2] = (Av2hFilterPlane){ source_v, 8U, 8U, 8U };
    destination.plane[0] =
        (Av2hFilterPlane){ destination_y, 16U, 16U, 16U };
    destination.plane[1] =
        (Av2hFilterPlane){ destination_u, 8U, 8U, 8U };
    destination.plane[2] =
        (Av2hFilterPlane){ destination_v, 8U, 8U, 8U };
    frame.width = 16U;
    frame.height = 16U;
    frame.cdef_strength_count = 1U;
    frame.cdef_on_skip_transform = 1U;
    frame.cdef_damping = 5U;
    frame.cdef_y_strength[0] = 58U;
    frame.cdef_uv_strength[0] = 56U;
    CHECK(av2h_cdef_filter_frame(&destination, &source, &frame) == AV2H_OK);
    CHECK(destination_y[4U * 16U + 4U] < 108U);
    CHECK(source_y[4U * 16U + 4U] == 108U);
    CHECK(av2h_cdef_filter_frame(&source, &source, &frame) ==
          AV2H_INVALID_ARGUMENT);
}

static void test_gdf_frame(void) {
    static const uint8_t expected[64] = {
        1U, 18U, 34U, 51U, 68U, 85U, 102U, 119U,
        29U, 49U, 68U, 88U, 109U, 129U, 149U, 168U,
        58U, 80U, 103U, 126U, 150U, 173U, 196U, 220U,
        87U, 113U, 139U, 165U, 191U, 218U, 243U, 13U,
        116U, 145U, 176U, 202U, 233U, 5U, 32U, 60U,
        145U, 178U, 208U, 241U, 17U, 51U, 80U, 111U,
        175U, 209U, 245U, 23U, 58U, 92U, 128U, 163U,
        203U, 242U, 23U, 60U, 97U, 138U, 175U, 212U
    };
    uint8_t source_data[64];
    uint8_t destination_data[64];
    Av2hFilterPlane source = { source_data, 8U, 8U, 8U };
    Av2hFilterPlane destination = { destination_data, 8U, 8U, 8U };
    Av2hFrameState frame;
    size_t row;
    size_t column;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            source_data[row * 8U + column] = (uint8_t)(
                column * 17U + row * 29U + column * row * 3U);
            destination_data[row * 8U + column] = 0U;
        }
    }
    frame.gdf_mode = 1U;
    frame.gdf_qp_index = 3U;
    frame.gdf_scale_index = 3U;
    CHECK(av2h_gdf_filter_frame(
              &destination, &source, &source, &frame) == AV2H_OK);
    for (row = 0U; row < 64U; ++row) {
        CHECK(destination_data[row] == expected[row]);
    }
    CHECK(av2h_gdf_filter_frame(&source, &source, &source, &frame) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(source_data[0] == 0U);
    frame.gdf_mode = 2U;
    destination_data[0] = 77U;
    CHECK(av2h_gdf_filter_frame(
              &destination, &source, &source, &frame) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(destination_data[0] == 77U);
}

static void test_band_only_and_control(void) {
    uint8_t y[64];
    uint8_t reference[64];
    uint8_t u[16];
    uint8_t v[16];
    uint8_t control = 1U;
    Av2hFilterPlanes planes;
    Av2hFrameState frame;
    Av2hCcsoControlMap controls;
    size_t index;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&controls, 0U, sizeof(controls));
    for (index = 0U; index < 64U; ++index) {
        y[index] = 100U;
        reference[index] = 100U;
    }
    for (index = 0U; index < 16U; ++index) {
        u[index] = index == 0U ? 254U : 100U;
        v[index] = 77U;
    }
    initialize_planes(&planes, y, u, v);
    frame.width = 8U;
    frame.height = 8U;
    frame.ccso[1].enabled = 1U;
    frame.ccso[1].band_only = 1U;
    frame.ccso[1].filter_offset[0] = 3;
    controls.plane[1] = &control;
    controls.stride[1] = 1U;
    controls.columns = 1U;
    controls.rows = 1U;
    CHECK(av2h_ccso_filter_frame(&planes, reference, 8U, &frame,
                                  &controls) == AV2H_OK);
    CHECK(u[0] == 255U);
    for (index = 1U; index < 16U; ++index) CHECK(u[index] == 103U);
    for (index = 0U; index < 16U; ++index) CHECK(v[index] == 77U);

    control = 0U;
    CHECK(av2h_ccso_filter_frame(&planes, reference, 8U, &frame,
                                  &controls) == AV2H_OK);
    CHECK(u[1] == 103U);
}

static void test_edge_classes(void) {
    uint8_t y[64];
    uint8_t reference[64];
    uint8_t u[16];
    uint8_t v[16];
    uint8_t control = 1U;
    Av2hFilterPlanes planes;
    Av2hFrameState frame;
    Av2hCcsoControlMap controls;
    size_t index;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&controls, 0U, sizeof(controls));
    for (index = 0U; index < 64U; ++index) {
        y[index] = 100U;
        reference[index] = 100U;
    }
    for (index = 0U; index < 16U; ++index) {
        u[index] = 0U;
        v[index] = 0U;
    }
    reference[0] = 50U;
    initialize_planes(&planes, y, u, v);
    frame.width = 8U;
    frame.height = 8U;
    frame.ccso[0].enabled = 1U;
    frame.ccso[0].extended_filter_support = 1U;
    frame.ccso[0].filter_offset[1] = -7;
    frame.ccso[0].filter_offset[6] = 5;
    controls.plane[0] = &control;
    controls.stride[0] = 1U;
    controls.columns = 1U;
    controls.rows = 1U;
    CHECK(av2h_ccso_filter_frame(&planes, reference, 8U, &frame,
                                  &controls) == AV2H_OK);
    CHECK(y[0] == 105U);
    CHECK(y[1] == 93U);
    CHECK(y[2] == 100U);
}

static void test_fail_closed(void) {
    uint8_t y[64];
    uint8_t u[16];
    uint8_t v[16];
    uint8_t deblocked_y[64];
    uint8_t deblocked_u[16];
    uint8_t deblocked_v[16];
    uint8_t cdef_y[64];
    uint8_t cdef_u[16];
    uint8_t cdef_v[16];
    uint8_t control = 1U;
    Av2hDeblockCell y_cells[4];
    Av2hDeblockCell u_cell;
    Av2hDeblockCell v_cell;
    Av2hFilterPlanes planes;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hCcsoControlMap controls;
    Av2hFilterWorkspace workspace;
    size_t index;
    av2h_memory_fill(y, 91U, sizeof(y));
    av2h_memory_fill(u, 92U, sizeof(u));
    av2h_memory_fill(v, 93U, sizeof(v));
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&controls, 0U, sizeof(controls));
    av2h_memory_fill(&workspace, 0U, sizeof(workspace));
    initialize_planes(&planes, y, u, v);
    initialize_planes(
        &workspace.deblocked, deblocked_y, deblocked_u, deblocked_v);
    initialize_planes(&workspace.cdef, cdef_y, cdef_u, cdef_v);
    for (index = 0U; index < 4U; ++index) {
        uint16_t row = (uint16_t)(index / 2U);
        uint16_t column = (uint16_t)(index % 2U);
        y_cells[index] = (Av2hDeblockCell){
            column, row, column, row, 4U, 4U, 128U, 0U, 0U
        };
    }
    u_cell = (Av2hDeblockCell){ 0U, 0U, 0U, 0U,
                                4U, 4U, 128U, 0U, 0U };
    v_cell = u_cell;
    workspace.deblock_map.plane[0] = y_cells;
    workspace.deblock_map.plane[1] = &u_cell;
    workspace.deblock_map.plane[2] = &v_cell;
    workspace.deblock_map.stride[0] = 2U;
    workspace.deblock_map.stride[1] = 1U;
    workspace.deblock_map.stride[2] = 1U;
    workspace.deblock_map.columns[0] = 2U;
    workspace.deblock_map.columns[1] = 1U;
    workspace.deblock_map.columns[2] = 1U;
    workspace.deblock_map.rows[0] = 2U;
    workspace.deblock_map.rows[1] = 1U;
    workspace.deblock_map.rows[2] = 1U;
    frame.width = 8U;
    frame.height = 8U;
    frame.ccso[0].enabled = 1U;
    frame.ccso[0].band_only = 1U;
    frame.ccso[0].filter_offset[0] = 9;
    controls.plane[0] = &control;
    controls.stride[0] = 1U;
    controls.columns = 1U;
    controls.rows = 1U;
        workspace.ccso_controls = controls;

    sequence.enable_restoration = 1U;
        CHECK(av2h_filter_frame(&planes, &sequence, &frame, &workspace) ==
            AV2H_UNSUPPORTED);
    CHECK(y[0] == 91U);
    sequence.enable_restoration = 0U;
    sequence.enable_cdef = 1U;
        frame.cdef_strength_count = 2U;
        CHECK(av2h_filter_frame(&planes, &sequence, &frame, &workspace) ==
            AV2H_UNSUPPORTED);
    CHECK(y[0] == 91U);
        sequence.enable_cdef = 0U;
        frame.cdef_strength_count = 0U;
        CHECK(av2h_filter_frame(&planes, &sequence, &frame, &workspace) ==
            AV2H_OK);
        CHECK(y[0] == 100U);
        y[0] = 91U;
    controls.columns = 2U;
    CHECK(av2h_ccso_filter_frame(&planes, y, 8U, &frame,
                                  &controls) == AV2H_INVALID_ARGUMENT);
    CHECK(y[0] == 91U);
}

int main(void) {
    test_deblock_edge();
    test_deblock_frame();
    test_cdef();
    test_cdef_frame();
    test_gdf_frame();
    test_band_only_and_control();
    test_edge_classes();
    test_fail_closed();
    if (failures != 0) {
        fprintf(stderr, "%d filter checks failed\n", failures);
        return 1;
    }
    puts("filter unit: pass");
    return 0;
}
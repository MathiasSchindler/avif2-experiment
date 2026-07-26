PYTHON ?= python3
CC ?= cc
NM ?= nm
HOST_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g \
	-fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc
HOST_SUBSTRATE_UNIT := build/host/substrate-unit
HOST_CONTAINER_UNIT := build/host/container-unit
HOST_RANGE_UNIT := build/host/range-unit
HOST_TILE_UNIT := build/host/tile-unit
HOST_FRAME_UNIT := build/host/frame-unit
HOST_CDF_UNIT := build/host/cdf-unit
HOST_PARTITION_UNIT := build/host/partition-unit
HOST_LEAF_UNIT := build/host/leaf-unit
HOST_COEFF_UNIT := build/host/coeff-unit
HOST_TRANSFORM_UNIT := build/host/transform-unit
HOST_COEFF_LEVEL_UNIT := build/host/coeff-level-unit
HOST_RECONSTRUCT_UNIT := build/host/reconstruct-unit
HOST_FILTER_UNIT := build/host/filter-unit
HOST_ARTIFACT_PROFILE_UNIT := build/host/artifact-profile-unit
HOST_UNITS := $(HOST_SUBSTRATE_UNIT) $(HOST_CONTAINER_UNIT) $(HOST_RANGE_UNIT) \
	$(HOST_TILE_UNIT) $(HOST_FRAME_UNIT) $(HOST_CDF_UNIT) \
	$(HOST_PARTITION_UNIT) $(HOST_LEAF_UNIT) $(HOST_COEFF_UNIT) \
	$(HOST_TRANSFORM_UNIT) $(HOST_COEFF_LEVEL_UNIT) $(HOST_RECONSTRUCT_UNIT) \
	$(HOST_FILTER_UNIT) $(HOST_ARTIFACT_PROFILE_UNIT)
HOST_ARTIFACT_TOOL := build/host/av2heif-artifact
CORE_SOURCES := src/base.c src/carriage.c src/bmff.c src/container.c src/query.c \
	src/decoder/cdf.c src/decoder/coeff.c src/decoder/coeff_level.c \
	src/decoder/filter.c src/decoder/frame.c src/decoder/leaf.c \
	src/decoder/frame_output.c \
	src/decoder/partition.c \
	src/decoder/partition_cdf.c \
	src/decoder/range.c src/decoder/reconstruct.c src/decoder/tile.c \
	src/decoder/transform.c src/decoder/tx_geometry.c
CORE_HEADERS := include/av2heif.h src/base.h src/decoder/sequence.h \
	src/decoder/cdf.h src/decoder/coeff.h src/decoder/coeff_level.h \
	src/decoder/filter.h src/decoder/frame.h src/decoder/leaf.h \
	src/decoder/frame_output.h \
	src/decoder/partition.h \
	src/decoder/partition_cdf.h \
	src/decoder/partition_cdf_data.inc src/decoder/gdf_data.inc \
	src/decoder/emerald_q208_reconstruction.inc src/decoder/ist_data.inc \
	src/decoder/range.h src/decoder/tile.h \
	src/decoder/transform.h src/decoder/tx_geometry.h src/decoder/reconstruct.h
STRICT_DIR := build/freestanding
STRICT_OBJECTS := $(patsubst src/%.c,$(STRICT_DIR)/%.o,$(CORE_SOURCES))
STRICT_CORE := $(STRICT_DIR)/av2heif-core.o
STRICT_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
	-ffreestanding -fno-builtin -fno-stack-protector \
	-fno-unwind-tables -fno-asynchronous-unwind-tables \
	-ffunction-sections -fdata-sections -nostdinc \
	-Iinclude -Isrc -Isrc/shared

.PHONY: test test-c test-freestanding reference-avm milestone-0 milestone-3 \
	milestone-4 milestone-5-evidence

test: test-c test-freestanding $(HOST_ARTIFACT_TOOL)
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'

test-c: $(HOST_UNITS)
	$(HOST_SUBSTRATE_UNIT)
	$(HOST_CONTAINER_UNIT)
	$(HOST_RANGE_UNIT)
	$(HOST_TILE_UNIT)
	$(HOST_FRAME_UNIT)
	$(HOST_CDF_UNIT)
	$(HOST_PARTITION_UNIT)
	$(HOST_LEAF_UNIT)
	$(HOST_COEFF_UNIT)
	$(HOST_TRANSFORM_UNIT)
	$(HOST_COEFF_LEVEL_UNIT)
	$(HOST_RECONSTRUCT_UNIT)
	$(HOST_FILTER_UNIT)
	$(HOST_ARTIFACT_PROFILE_UNIT)

$(HOST_SUBSTRATE_UNIT): tests/substrate_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/substrate_unit.c $(CORE_SOURCES) -o $@

$(HOST_CONTAINER_UNIT): tests/container_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/container_unit.c $(CORE_SOURCES) -o $@

$(HOST_RANGE_UNIT): tests/range_unit.c $(CORE_SOURCES) $(CORE_HEADERS) \
		src/decoder/range.h
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/range_unit.c $(CORE_SOURCES) -o $@

$(HOST_TILE_UNIT): tests/tile_unit.c $(CORE_SOURCES) $(CORE_HEADERS) \
		src/decoder/tile.h
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/tile_unit.c $(CORE_SOURCES) -o $@

$(HOST_FRAME_UNIT): tests/frame_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/frame_unit.c $(CORE_SOURCES) -o $@

$(HOST_CDF_UNIT): tests/cdf_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/cdf_unit.c $(CORE_SOURCES) -o $@

$(HOST_PARTITION_UNIT): tests/partition_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/partition_unit.c $(CORE_SOURCES) -o $@

$(HOST_LEAF_UNIT): tests/leaf_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/leaf_unit.c $(CORE_SOURCES) -o $@

$(HOST_COEFF_UNIT): tests/coeff_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/coeff_unit.c $(CORE_SOURCES) -o $@

$(HOST_TRANSFORM_UNIT): tests/transform_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/transform_unit.c $(CORE_SOURCES) -o $@

$(HOST_COEFF_LEVEL_UNIT): tests/coeff_level_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/coeff_level_unit.c $(CORE_SOURCES) -o $@

$(HOST_RECONSTRUCT_UNIT): tests/reconstruct_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/reconstruct_unit.c $(CORE_SOURCES) -o $@

$(HOST_FILTER_UNIT): tests/filter_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/filter_unit.c $(CORE_SOURCES) -o $@

$(HOST_ARTIFACT_PROFILE_UNIT): tests/artifact_profile_unit.c \
		$(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/artifact_profile_unit.c $(CORE_SOURCES) -o $@

$(HOST_ARTIFACT_TOOL): tools/av2heif_artifact.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tools/av2heif_artifact.c $(CORE_SOURCES) -o $@

test-freestanding: $(STRICT_CORE)
	@$(NM) -u $(STRICT_CORE) > $(STRICT_DIR)/undefined-symbols.txt
	@test ! -s $(STRICT_DIR)/undefined-symbols.txt

$(STRICT_CORE): $(STRICT_OBJECTS)
	$(CC) -nostdlib -Wl,-r $(STRICT_OBJECTS) -o $@

$(STRICT_DIR)/%.o: src/%.c $(CORE_HEADERS) src/shared/limits.h \
		src/shared/stddef.h src/shared/stdint.h
	@mkdir -p $(dir $@)
	$(CC) $(STRICT_CFLAGS) -c $< -o $@

reference-avm:
	sh scripts/setup-avm.sh

milestone-0: reference-avm test
	$(PYTHON) tools/run_m0_matrix.py

milestone-3: test
	M3_ARTIFACT_COMPILER="$(CC)" \
	M3_ARTIFACT_BUILD_OPTIONS='$(HOST_CFLAGS)' \
	$(PYTHON) tools/run_m3_lab.py

milestone-4: milestone-3

milestone-5-evidence:
	@mkdir -p build/m5/evidence
	$(PYTHON) tools/generate_m5_gdf_tables.py \
		--output build/m5/evidence/gdf_data.inc
	@cmp src/decoder/gdf_data.inc build/m5/evidence/gdf_data.inc
	$(PYTHON) tools/check_m5_partition_cdfs.py
	$(PYTHON) tools/check_m5_leaf_cdfs.py
	$(PYTHON) tools/check_m5_coeff_cdfs.py
	$(PYTHON) tools/check_m5_transform_cdfs.py
	$(PYTHON) tools/check_m5_prediction.py
PYTHON ?= python3
CC ?= cc

ifeq ($(OS),Windows_NT)
ifneq ($(filter ARM64,$(PROCESSOR_ARCHITECTURE) $(PROCESSOR_ARCHITEW6432)),)
DECODER_PRESET ?= windows-arm64
DECODER_BUILD_DIR ?= build/windows-arm64
else
DECODER_PRESET ?= windows-x86_64
DECODER_BUILD_DIR ?= build/windows-x86_64
endif
else
DECODER_PRESET ?= native
DECODER_BUILD_DIR ?= build/native
endif

.PHONY: all decoder clean

all: decoder

decoder:
	cmake --preset $(DECODER_PRESET)
	cmake --build --preset $(DECODER_PRESET) --target av2heif-decoder av2heif-freestanding-check

clean:
	cmake -E rm -rf $(DECODER_BUILD_DIR)

HOST_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g \
	-fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc
HOST_PRESET ?= host-sanitized
HOST_BUILD_DIR ?= build/host-sanitized
CTEST_ARGS ?= --output-on-failure
HOST_RECONSTRUCT_UNIT := $(HOST_BUILD_DIR)/reconstruct-unit
HOST_TILE_UNIT := $(HOST_BUILD_DIR)/tile-unit
HOST_FILTER_UNIT := $(HOST_BUILD_DIR)/filter-unit
HOST_ARTIFACT_TOOL := $(HOST_BUILD_DIR)/av2heif-artifact
M9_DECODER ?= build/windows-x86_64/av2heif-decoder.exe
.PHONY: test test-c test-freestanding reference-avm milestone-0 milestone-3 \
	milestone-4 milestone-5-evidence milestone-8 milestone-9

test: test-c test-freestanding
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'

test-c:
	cmake --preset $(HOST_PRESET)
	cmake --build --preset $(HOST_PRESET)
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS)

test-freestanding:
	cmake --preset $(HOST_PRESET)
	cmake --build --preset $(HOST_PRESET) --target av2heif-freestanding-check

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

milestone-8: test-c test-freestanding
	$(HOST_RECONSTRUCT_UNIT)
	$(HOST_TILE_UNIT)

milestone-9: test-c test-freestanding
	$(HOST_FILTER_UNIT)
	$(PYTHON) -m unittest discover -s tests -p 'test_m9_filter_evidence.py'
	$(PYTHON) tools/run_m9_final_parity.py --decoder $(M9_DECODER)
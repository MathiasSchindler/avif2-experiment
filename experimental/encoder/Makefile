CC ?= cc
READELF ?= readelf
PYTHON ?= python3
AVMDEC ?= $(abspath ../../build/reference/avm/avmdec)
PROJECT_DECODER ?= $(abspath ../../build/host-cmake/av2heif-decoder)
BUILD := build
TARGET := $(BUILD)/av2-encoder-experiment
BIT_TEST := $(BUILD)/bit-writer-test
RANGE_TEST := $(BUILD)/range-writer-test
M2_MATRIX_DRIVER := $(BUILD)/m2-transcript-driver
M1_TEST := $(BUILD)/m1-test
M3_TEST := $(BUILD)/m3-test
M3_CHROMA_TEST := $(BUILD)/m3-chroma-test
M3_CHROMA_DRIVER := $(BUILD)/m3-chroma-driver
M3_PLANE_QDELTA_DRIVER := $(BUILD)/m3-plane-qdelta-driver
QDELTA_SYNTAX_TEST := $(BUILD)/qdelta-syntax-test
M3_BOUNDARY_DRIVER := $(BUILD)/m3-boundary-partition-driver
M4_TEST := $(BUILD)/m4-test
AC8X8_TEST := $(BUILD)/ac8x8-test
AC8X8_COLOR_SYNTAX_DRIVER := $(BUILD)/ac8x8-color-syntax-driver
AC8X8_COLOR_GATE_DRIVER := $(BUILD)/ac8x8-color-gate-driver
CHROMA_AC_SYNTAX_DRIVER := $(BUILD)/chroma-ac-syntax-driver
FULL_DIRECTIONAL_SYNTAX_DRIVER := $(BUILD)/full-directional-syntax-driver
PLATFORM_MEMORY_TEST := $(BUILD)/platform-memory-test
IMAGE_INPUT_TEST := $(BUILD)/image-input-test
IMAGE_INPUT_FREESTANDING := $(BUILD)/image-input-freestanding.o
INPUT_PLANAR_TEST := $(BUILD)/input-planar-test

CPPFLAGS := -Iinclude
COMMON_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
	-ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie \
	-fno-asynchronous-unwind-tables -fno-unwind-tables
NOLIBC_LDFLAGS := -nostdlib -static -no-pie -Wl,--build-id=none

CORE_SOURCES := src/memory.c src/writer.c src/range.c src/cdf.c src/obu.c \
	src/syntax_cdf.c src/syntax.c src/directional.c \
	src/m1.c src/m3.c src/m4.c src/ac8x8.c
ENCODER_SOURCES := src/main.c src/input.c src/cli_input.c src/image_input.c \
	$(CORE_SOURCES) \
	platform/linux.c \
	platform/x86_64/start.S
ENCODER_OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(filter %.c,$(ENCODER_SOURCES))) \
	$(patsubst %.S,$(BUILD)/%.o,$(filter %.S,$(ENCODER_SOURCES)))

.PHONY: all clean test test-unit check-validators audit audit-dependencies \
	check-generated chroma-ac-gate full-directional-gate

all: $(TARGET)

$(TARGET): $(ENCODER_OBJECTS)
	$(CC) $(NOLIBC_LDFLAGS) -o $@ $^

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(COMMON_CFLAGS) -MMD -MP \
		-MF $(@:.o=.d) -c -o $@ $<

$(BUILD)/src/syntax_cdf.o: src/normative_cdfs.inc src/normative_chroma_cdfs.inc
$(BUILD)/src/ac8x8.o: src/normative_dct16.inc
$(BUILD)/src/directional.o: src/normative_directional.inc

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $<

-include $(ENCODER_OBJECTS:.o=.d)

$(BIT_TEST): tests/bit_writer_test.c src/writer.c src/memory.c \
	include/encoder/writer.h include/encoder/base.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/bit_writer_test.c src/writer.c \
		src/memory.c

$(RANGE_TEST): tests/range_writer_test.c src/range.c src/cdf.c \
	include/encoder/range.h include/encoder/cdf.h include/encoder/base.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/range_writer_test.c src/range.c src/cdf.c

$(M2_MATRIX_DRIVER): tests/m2_transcript_driver.c src/range.c \
	include/encoder/range.h include/encoder/cdf.h include/encoder/base.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/m2_transcript_driver.c src/range.c

$(M1_TEST): tests/m1_test.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/m1_test.c $(CORE_SOURCES)

$(M3_TEST): tests/m3_test.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/m3_test.c $(CORE_SOURCES)

$(M3_CHROMA_TEST): tests/m3_chroma_test.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/m3_chroma_test.c $(CORE_SOURCES)

$(M3_CHROMA_DRIVER): tests/m3_chroma_driver.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/m3_chroma_driver.c $(CORE_SOURCES)

$(M3_PLANE_QDELTA_DRIVER): tests/m3_plane_qdelta_driver.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/m3_plane_qdelta_driver.c \
		$(CORE_SOURCES)

$(QDELTA_SYNTAX_TEST): tests/qdelta_syntax_test.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/qdelta_syntax_test.c $(CORE_SOURCES)

$(M3_BOUNDARY_DRIVER): tests/m3_boundary_partition_driver.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/m3_boundary_partition_driver.c \
		$(CORE_SOURCES)

$(M4_TEST): tests/m4_test.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/m4_test.c $(CORE_SOURCES)

$(AC8X8_TEST): tests/ac8x8_test.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/ac8x8_test.c $(CORE_SOURCES)

$(AC8X8_COLOR_SYNTAX_DRIVER): tests/ac8x8_color_syntax_driver.c \
		src/syntax_cdf.c src/range.c src/cdf.c src/writer.c src/memory.c \
		src/obu.c src/directional.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/ac8x8_color_syntax_driver.c \
		src/syntax_cdf.c src/range.c src/cdf.c src/writer.c src/memory.c \
		src/obu.c src/directional.c

$(AC8X8_COLOR_GATE_DRIVER): tests/ac8x8_color_gate_driver.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/ac8x8_color_gate_driver.c \
		$(CORE_SOURCES)

$(CHROMA_AC_SYNTAX_DRIVER): tests/chroma_ac_syntax_driver.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/chroma_ac_syntax_driver.c \
		$(CORE_SOURCES)

$(FULL_DIRECTIONAL_SYNTAX_DRIVER): \
		tests/full_directional_syntax_driver.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/full_directional_syntax_driver.c \
		$(CORE_SOURCES)

$(PLATFORM_MEMORY_TEST): tests/platform_memory_test.c platform/linux.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/platform_memory_test.c platform/linux.c

audit-dependencies:
	tests/dependency_audit.sh

check-generated:
	$(PYTHON) tools/generate_cdf_subset.py --check
	$(PYTHON) tools/generate_chroma_cdf_subset.py --check
	$(PYTHON) tools/generate_dct16.py --check
	$(PYTHON) tools/generate_directional_tables.py --check
	$(PYTHON) tools/generate_input_fixtures.py --check
	$(PYTHON) tools/ac8x8_model.py --check
	$(PYTHON) tools/ac8x8_color_syntax_model.py --check
	$(PYTHON) tools/directional_fast_search.py --check

$(IMAGE_INPUT_TEST): tests/image_input_test.c src/image_input.c src/memory.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/image_input_test.c src/image_input.c \
		src/memory.c

$(IMAGE_INPUT_FREESTANDING): src/image_input.c include/encoder/image_input.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(COMMON_CFLAGS) -c -o $@ src/image_input.c

$(INPUT_PLANAR_TEST): tests/input_planar_test.c src/input.c src/memory.c \
		platform/linux.c include/encoder/input.h include/encoder/platform.h \
		include/encoder/base.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/input_planar_test.c src/input.c \
		src/memory.c platform/linux.c

audit: audit-dependencies $(TARGET)
	READELF="$(READELF)" tests/audit_elf.sh "$(TARGET)"

check-validators:
	@test -f "$(AVMDEC)" && test -x "$(AVMDEC)" || { \
		printf '%s\n' "error: pinned avmdec is not executable: $(AVMDEC)" >&2; \
		exit 1; \
	}
	@test -f "$(PROJECT_DECODER)" && test -x "$(PROJECT_DECODER)" || { \
		printf '%s\n' "error: project decoder is not executable: $(PROJECT_DECODER)" >&2; \
		exit 1; \
	}

define run-tests
	./$(BIT_TEST)
	./$(RANGE_TEST)
	$(PYTHON) -m unittest tests/test_m2_transcript_matrix.py \
		tests/test_syntax_helpers.py
	./$(M1_TEST)
	./$(M3_TEST)
	./$(M3_CHROMA_TEST)
	./$(QDELTA_SYNTAX_TEST)
	./$(M4_TEST)
	./$(AC8X8_TEST)
	./$(PLATFORM_MEMORY_TEST)
	./$(IMAGE_INPUT_TEST)
	./$(INPUT_PLANAR_TEST)
	tests/smoke.sh "$(TARGET)" "$(BUILD)"
	tests/input_errors.sh "$(TARGET)" "$(BUILD)"
	tests/cli_contract.sh "$(TARGET)" "$(BUILD)"
	tests/atomic_output.sh "$(TARGET)" "$(BUILD)"
	$(1) AVMDEC="$(AVMDEC)" PROJECT_DECODER="$(PROJECT_DECODER)" \
		$(PYTHON) -m unittest tests/test_encoder_oracle.py \
		tests/test_obu_inspect.py tests/test_m1_syntax.py \
		tests/test_m1_entropy.py tests/test_m1_encoder.py \
		tests/test_m2_full_stream_matrix.py \
		tests/test_cdf_subset.py tests/test_m3_dc_model.py \
		tests/test_m3_encoder.py tests/test_m3_benchmark.py \
		tests/test_m3_prepared_corpus.py \
		tests/test_m3_odd_model.py \
		tests/test_m3_large_partition_model.py \
		tests/test_m3_multisuperblock_model.py \
		tests/test_m3_boundary_partition_transcript.py \
		tests/test_m3_multileaf_cdf_model.py \
		tests/test_m3_edge_coeff_model.py \
		tests/test_m3_chroma_boundary_model.py \
		tests/test_m3_chroma_encoder.py \
		tests/test_directional_intra_model.py \
		tests/test_directional_intra_production.py \
		tests/test_full_directional_production.py \
		tests/test_full_directional_gate.py \
		tests/test_directional_fast_search.py \
		tests/test_smooth_paeth_model.py \
		tests/test_smooth_paeth_production.py \
		tests/test_m3_plane_qdelta_model.py \
		tests/test_m3_plane_qdelta_production.py \
		tests/test_m4_ac_model.py tests/test_ac8x8_model.py \
		tests/test_ac8x8_hardbudget_model.py \
		tests/test_ac8x8_production.py \
		tests/test_ac8x8_color_syntax_model.py \
		tests/test_ac8x8_color_gate.py \
		tests/test_chroma_ac_production.py \
		tests/test_ac8x8_equalsize_qtrial.py \
		tests/test_m4_encoder.py \
		tests/test_raw_planar_input.py \
		tests/test_encoder_input_integration.py \
		tests/test_encoder_quality_baseline.py
endef

test: check-validators check-generated $(BIT_TEST) $(RANGE_TEST) \
	$(M2_MATRIX_DRIVER) $(M1_TEST) \
	$(M3_TEST) $(M3_CHROMA_TEST) $(M3_CHROMA_DRIVER) \
	$(M3_PLANE_QDELTA_DRIVER) \
	$(QDELTA_SYNTAX_TEST) \
	$(M3_BOUNDARY_DRIVER) $(M4_TEST) $(AC8X8_TEST) \
	$(AC8X8_COLOR_SYNTAX_DRIVER) $(PLATFORM_MEMORY_TEST) \
	$(AC8X8_COLOR_GATE_DRIVER) \
	$(IMAGE_INPUT_TEST) $(IMAGE_INPUT_FREESTANDING) $(INPUT_PLANAR_TEST) audit
	$(call run-tests,)

test-unit: check-generated $(BIT_TEST) $(RANGE_TEST) $(M2_MATRIX_DRIVER) \
	$(M1_TEST) $(M3_TEST) $(M3_CHROMA_TEST) $(M3_CHROMA_DRIVER) \
	$(M3_PLANE_QDELTA_DRIVER) \
	$(QDELTA_SYNTAX_TEST) \
	$(M3_BOUNDARY_DRIVER) $(M4_TEST) $(AC8X8_TEST) \
	$(AC8X8_COLOR_SYNTAX_DRIVER) \
	$(AC8X8_COLOR_GATE_DRIVER) \
	$(PLATFORM_MEMORY_TEST) $(IMAGE_INPUT_TEST) $(IMAGE_INPUT_FREESTANDING) \
	$(INPUT_PLANAR_TEST) audit
	$(call run-tests,ENCODER_TEST_UNIT_ONLY=1)

chroma-ac-gate: all
	$(PYTHON) tools/chroma_ac_gate.py \
		--avmdec "$(AVMDEC)" \
		--project-decoder "$(PROJECT_DECODER)"

full-directional-gate: all
	$(PYTHON) tools/full_directional_gate.py \
		--avmdec "$(AVMDEC)" \
		--project-decoder "$(PROJECT_DECODER)"

clean:
	rm -rf "$(BUILD)"

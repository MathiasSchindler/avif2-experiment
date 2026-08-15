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
M4_TEST := $(BUILD)/m4-test

CPPFLAGS := -Iinclude
COMMON_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
	-ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie \
	-fno-asynchronous-unwind-tables -fno-unwind-tables
NOLIBC_LDFLAGS := -nostdlib -static -no-pie -Wl,--build-id=none

CORE_SOURCES := src/memory.c src/writer.c src/range.c src/cdf.c src/obu.c \
	src/syntax_cdf.c src/syntax.c src/m1.c src/m3.c src/m4.c
ENCODER_SOURCES := src/main.c src/input.c $(CORE_SOURCES) platform/linux.c \
	platform/x86_64/start.S
ENCODER_OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(filter %.c,$(ENCODER_SOURCES))) \
	$(patsubst %.S,$(BUILD)/%.o,$(filter %.S,$(ENCODER_SOURCES)))

.PHONY: all clean test test-unit check-validators audit audit-dependencies \
	check-generated

all: $(TARGET)

$(TARGET): $(ENCODER_OBJECTS)
	$(CC) $(NOLIBC_LDFLAGS) -o $@ $^

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(COMMON_CFLAGS) -c -o $@ $<

$(BUILD)/src/syntax_cdf.o: src/normative_cdfs.inc

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $<

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

$(M4_TEST): tests/m4_test.c $(CORE_SOURCES)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror \
		-O2 -fno-builtin -o $@ tests/m4_test.c $(CORE_SOURCES)

audit-dependencies:
	tests/dependency_audit.sh

check-generated:
	$(PYTHON) tools/generate_cdf_subset.py --check

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
	./$(M4_TEST)
	tests/smoke.sh "$(TARGET)" "$(BUILD)"
	tests/input_errors.sh "$(TARGET)" "$(BUILD)"
	tests/atomic_output.sh "$(TARGET)" "$(BUILD)"
	$(1) AVMDEC="$(AVMDEC)" PROJECT_DECODER="$(PROJECT_DECODER)" \
		$(PYTHON) -m unittest tests/test_encoder_oracle.py \
		tests/test_obu_inspect.py tests/test_m1_syntax.py \
		tests/test_m1_entropy.py tests/test_m1_encoder.py \
		tests/test_m2_full_stream_matrix.py \
		tests/test_cdf_subset.py tests/test_m3_dc_model.py \
		tests/test_m3_encoder.py tests/test_m3_benchmark.py \
		tests/test_m3_odd_model.py \
		tests/test_m4_ac_model.py tests/test_m4_encoder.py
endef

test: check-validators check-generated $(BIT_TEST) $(RANGE_TEST) \
	$(M2_MATRIX_DRIVER) $(M1_TEST) \
	$(M3_TEST) $(M4_TEST) audit
	$(call run-tests,)

test-unit: check-generated $(BIT_TEST) $(RANGE_TEST) $(M2_MATRIX_DRIVER) \
	$(M1_TEST) $(M3_TEST) $(M4_TEST) audit
	$(call run-tests,ENCODER_TEST_UNIT_ONLY=1)

clean:
	rm -rf "$(BUILD)"

ifeq ($(origin CC),default)
CC := clang
endif
AR ?= ar
NM ?= nm
JOBS ?= $(or $(NUMBER_OF_PROCESSORS),$(shell nproc 2>/dev/null),$(shell getconf _NPROCESSORS_ONLN 2>/dev/null),$(shell sysctl -n hw.ncpu 2>/dev/null),1)
ifeq ($(filter -j% --jobs%,$(MAKEFLAGS)),)
MAKEFLAGS += -j$(JOBS)
endif

USE_CCACHE ?= 1
CCACHE ?= $(shell command -v ccache 2>/dev/null)
COMPILE_CC = $(if $(filter 1,$(USE_CCACHE)),$(if $(CCACHE),$(CCACHE) ))$(CC)

CORE_SOURCES := \
	src/base.c \
	src/carriage.c \
	src/bmff.c \
	src/container.c \
	src/container_write.c \
	src/query.c \
	src/decode.c \
	src/output_layout.c \
	src/pixel_format.c \
	src/preview.c \
	src/shared/png_writer.c \
	src/decoder/cdf.c \
	src/decoder/chroma_prediction.c \
	src/decoder/chroma_prediction_cfl.c \
	src/decoder/chroma_prediction_common.c \
	src/decoder/chroma_prediction_mhccp.c \
	src/decoder/coeff.c \
	src/decoder/coeff_context.c \
	src/decoder/coeff_dequant.c \
	src/decoder/coeff_level.c \
	src/decoder/coeff_level_cdf.c \
	src/decoder/content_interpretation.c \
	src/decoder/ccso.c \
	src/decoder/cdef.c \
	src/decoder/deblock.c \
	src/decoder/filter.c \
	src/decoder/filter_entropy.c \
	src/decoder/frame_workspace.c \
	src/decoder/frame_copy.c \
	src/decoder/frame_filter_metadata.c \
	src/decoder/frame.c \
	src/decoder/frame_output.c \
	src/decoder/gdf.c \
	src/decoder/intrabc.c \
	src/decoder/intrabc_prediction.c \
	src/decoder/intra_prediction.c \
	src/decoder/intra_prediction_basic.c \
	src/decoder/intra_prediction_directional.c \
	src/decoder/intra_prediction_edges.c \
	src/decoder/intra_prediction_dip.c \
	src/decoder/inverse_transform.c \
	src/decoder/leaf.c \
	src/decoder/leaf_context.c \
	src/decoder/leaf_cdf.c \
	src/decoder/obu.c \
	src/decoder/palette.c \
	src/decoder/partition.c \
	src/decoder/partition_context.c \
	src/decoder/partition_cdf.c \
	src/decoder/qmatrix.c \
	src/decoder/range.c \
	src/decoder/reconstruct.c \
	src/decoder/reconstruct_block.c \
	src/decoder/restoration.c \
	src/decoder/sample_math.c \
	src/decoder/secondary_transform.c \
	src/decoder/sequence.c \
	src/decoder/tile.c \
	src/decoder/tile_chroma.c \
	src/decoder/tile_filter_handoff.c \
	src/decoder/tile_luma.c \
	src/decoder/transform.c \
	src/decoder/transform_cdf.c \
	src/decoder/transform_map.c \
	src/decoder/tx_geometry.c

CLI_SOURCES := src/shared/cli_common.c src/shared/cli_diagnostics.c

COMMON_WARNINGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -pipe
FREESTANDING_CFLAGS := $(COMMON_WARNINGS) -O2 \
	-ffreestanding -fno-builtin -fno-stack-protector \
	-fno-unwind-tables -fno-asynchronous-unwind-tables \
	-ffunction-sections -fdata-sections -nostdinc \
	-Iinclude -Isrc -Isrc/shared

ifeq ($(OS),Windows_NT)
PYTHON ?= python
ifneq ($(filter ARM64,$(PROCESSOR_ARCHITECTURE) $(PROCESSOR_ARCHITEW6432)),)
DECODER_BUILD_DIR ?= build/windows-arm64
DEFAULT_WINDOWS_TARGET := windows-arm64
else
DECODER_BUILD_DIR ?= build/windows-x86_64
DEFAULT_WINDOWS_TARGET := windows-x86_64
endif
else
PYTHON ?= python3
DECODER_BUILD_DIR ?= build/native
endif

NATIVE_BUILD_DIR ?= $(DECODER_BUILD_DIR)
NATIVE_OBJECTS := $(CORE_SOURCES:%.c=$(NATIVE_BUILD_DIR)/obj/%.o)
NATIVE_CLI_OBJECTS := $(CLI_SOURCES:%.c=$(NATIVE_BUILD_DIR)/obj/cli/%.o)
NATIVE_LIBRARY := $(NATIVE_BUILD_DIR)/libav2heif-core.a
NATIVE_DECODER := $(NATIVE_BUILD_DIR)/av2heif-decoder
NATIVE_FREESTANDING_STAMP := $(NATIVE_BUILD_DIR)/freestanding/symbols-checked

AVM_SOURCE_DIR ?= vendor/avm
AVM_BUILD_DIR ?= build/reference/avm
CENSUS_BUILD_DIR ?= build/census
CENSUS := $(CENSUS_BUILD_DIR)/av2-census$(if $(filter Windows_NT,$(OS)),.exe)
CENSUS_CFLAGS := $(COMMON_WARNINGS) -O2 -Wconversion -Wshadow \
	-isystem $(AVM_BUILD_DIR) -isystem $(AVM_SOURCE_DIR)
ifeq ($(OS),Windows_NT)
CENSUS_CFLAGS += -D_WIN32_WINNT=0x0601 -DNOMINMAX
CENSUS_LDFLAGS := -static -lpthread
else
CENSUS_LDFLAGS := -pthread -lm
endif

HOST_SYSTEM := $(shell uname -s 2>/dev/null)
HOST_ARCH := $(shell uname -m 2>/dev/null)
RUNTIME_C_SOURCES := src/runtime/concurrency.c
RUNTIME_ASM_SOURCES :=
ifeq ($(HOST_SYSTEM),Linux)
ifneq ($(filter x86_64 amd64 AMD64,$(HOST_ARCH)),)
RUNTIME_C_SOURCES += src/runtime/concurrency_linux_x86_64.c
RUNTIME_ASM_SOURCES += src/arch/x86_64/linux/concurrency.S
else
RUNTIME_C_SOURCES += src/runtime/concurrency_serial.c
endif
else
RUNTIME_C_SOURCES += src/runtime/concurrency_serial.c
endif

NEWOS_LINKER_SOURCES := \
	tools/macho_linker/newos/src/tools/linker.c \
	tools/macho_linker/newos/src/compiler/linker.c \
	tools/macho_linker/newos/src/compiler/linker_util.c \
	tools/macho_linker/newos/src/compiler/linker_elf.c \
	tools/macho_linker/newos/src/compiler/linker_object.c \
	tools/macho_linker/newos/src/compiler/linker_symbols.c \
	tools/macho_linker/newos/src/compiler/linker_gc.c \
	tools/macho_linker/newos/src/compiler/linker_merge.c \
	tools/macho_linker/newos/src/compiler/linker_icf.c \
	tools/macho_linker/newos/src/compiler/linker_reloc.c \
	tools/macho_linker/newos/src/compiler/linker_layout.c \
	tools/macho_linker/newos/src/compiler/linker_report.c \
	tools/macho_linker/newos/src/compiler/linker_lto.c \
	tools/macho_linker/newos/src/compiler/linker_macho.c \
	tools/macho_linker/newos/src/shared/crypto/sha256.c \
	tools/macho_linker/newos/src/shared/runtime/memory.c \
	tools/macho_linker/newos/src/shared/runtime/string.c \
	tools/macho_linker/newos/src/shared/runtime/parse.c \
	tools/macho_linker/newos/src/shared/runtime/io.c \
	tools/macho_linker/newos/src/shared/tool_cli.c \
	tools/macho_linker/newos/src/shared/tool_io.c \
	tools/macho_linker/newos/src/shared/tool_json.c \
	tools/macho_linker/newos/src/shared/tool_path.c \
	tools/macho_linker/newos/src/platform/posix/fs.c \
	tools/macho_linker/newos/src/platform/posix/process.c \
	tools/macho_linker/newos/src/platform/posix/identity.c
NEWOS_LINKER_CFLAGS := $(COMMON_WARNINGS) -O2 -fno-lto \
	-ffunction-sections -fdata-sections \
	-Itools/macho_linker/newos/src/compiler \
	-Itools/macho_linker/newos/src/platform/posix \
	-Itools/macho_linker/newos/src/platform/common \
	-DCOMPILER_LINKER_ENABLE_REPORTING=1

NOLIBC_OPT_CFLAGS ?= -Oz
NOLIBC_LTO ?= 1
NOLIBC_LTO_CFLAGS := $(if $(filter 1,$(NOLIBC_LTO)),-flto=full)
LLVM_NM ?= $(shell major=$$($(firstword $(CC)) -dumpversion 2>/dev/null | cut -d. -f1); \
	command -v llvm-nm-$$major 2>/dev/null || command -v llvm-nm 2>/dev/null || printf '%s' '$(NM)')
NOLIBC_NM := $(if $(filter 1,$(NOLIBC_LTO)),$(LLVM_NM),$(NM))

ifeq ($(HOST_SYSTEM),Darwin)
NOLIBC_BUILD_DIR ?= build/macos-nolibc-arm64
NOLIBC_CORE_EXTRA_CFLAGS := $(NOLIBC_OPT_CFLAGS) $(NOLIBC_LTO_CFLAGS)
NOLIBC_RUNTIME_EXTRA_CFLAGS := $(NOLIBC_OPT_CFLAGS) $(NOLIBC_LTO_CFLAGS)
NOLIBC_PLATFORM_EXTRA_CFLAGS := $(NOLIBC_OPT_CFLAGS) $(NOLIBC_LTO_CFLAGS) -fno-common -fno-jump-tables
NOLIBC_PLATFORM_C_SOURCES := src/platform/nolibc/decoder.c src/platform/macos/runtime.c
NOLIBC_PLATFORM_ASM_SOURCES := src/arch/aarch64/macos/runtime.S
NEWOS_LINKER_CFLAGS += -Itools/macho_linker/newos/src/shared
NEWOS_LINKER_LDFLAGS := -fno-lto -Wl,-dead_strip
else
NOLIBC_BUILD_DIR ?= build/linux-nolibc
NOLIBC_CORE_EXTRA_CFLAGS := $(NOLIBC_OPT_CFLAGS) $(NOLIBC_LTO_CFLAGS) -fno-jump-tables -fno-pic -fno-pie
NOLIBC_RUNTIME_EXTRA_CFLAGS := $(NOLIBC_OPT_CFLAGS) $(NOLIBC_LTO_CFLAGS) -fno-pic -fno-pie
NOLIBC_PLATFORM_EXTRA_CFLAGS := $(NOLIBC_OPT_CFLAGS) $(NOLIBC_LTO_CFLAGS) -fno-pic -fno-pie
NOLIBC_PLATFORM_C_SOURCES := src/platform/nolibc/decoder.c src/platform/linux/runtime.c
NEWOS_LINKER_CFLAGS += -idirafter tools/macho_linker/newos/src/shared
NEWOS_LINKER_LDFLAGS := -fno-lto -Wl,--gc-sections
ifneq ($(filter x86_64 amd64 AMD64,$(HOST_ARCH)),)
NOLIBC_PLATFORM_ASM_SOURCES := src/arch/x86_64/linux/runtime.S
else
NOLIBC_PLATFORM_ASM_SOURCES := src/arch/aarch64/linux/runtime.S
NOLIBC_CORE_EXTRA_CFLAGS += -mno-outline-atomics
NOLIBC_RUNTIME_EXTRA_CFLAGS += -mno-outline-atomics
NOLIBC_PLATFORM_EXTRA_CFLAGS += -mno-outline-atomics
endif
endif

NOLIBC_CORE_OBJECTS := $(CORE_SOURCES:%.c=$(NOLIBC_BUILD_DIR)/obj/core/%.o)
NOLIBC_CLI_OBJECTS := $(CLI_SOURCES:%.c=$(NOLIBC_BUILD_DIR)/obj/cli/%.o)
NOLIBC_RUNTIME_C_OBJECTS := $(RUNTIME_C_SOURCES:%.c=$(NOLIBC_BUILD_DIR)/obj/runtime/%.o)
NOLIBC_RUNTIME_ASM_OBJECTS := $(RUNTIME_ASM_SOURCES:%.S=$(NOLIBC_BUILD_DIR)/obj/runtime/%.o)
NOLIBC_RUNTIME_OBJECTS := $(NOLIBC_RUNTIME_C_OBJECTS) $(NOLIBC_RUNTIME_ASM_OBJECTS)
NOLIBC_PLATFORM_C_OBJECTS := $(NOLIBC_PLATFORM_C_SOURCES:%.c=$(NOLIBC_BUILD_DIR)/obj/platform/%.o)
NOLIBC_PLATFORM_ASM_OBJECTS := $(NOLIBC_PLATFORM_ASM_SOURCES:%.S=$(NOLIBC_BUILD_DIR)/obj/platform/%.o)
NOLIBC_PLATFORM_OBJECTS := $(NOLIBC_PLATFORM_C_OBJECTS) $(NOLIBC_PLATFORM_ASM_OBJECTS)
NOLIBC_DECODER := $(NOLIBC_BUILD_DIR)/av2heif-decoder-nolibc
NOLIBC_LINKER := $(NOLIBC_BUILD_DIR)/av2heif-newos-linker
NOLIBC_CORE_STAMP := $(NOLIBC_BUILD_DIR)/freestanding/symbols-checked
NOLIBC_RUNTIME_STAMP := $(NOLIBC_BUILD_DIR)/freestanding/runtime-symbols-checked
NOLIBC_SUBSTRATE := $(NOLIBC_BUILD_DIR)/runtime-substrate-freestanding
NOLIBC_SUBSTRATE_C_SOURCES := tests/runtime_substrate_freestanding.c src/platform/linux/runtime.c
NOLIBC_SUBSTRATE_ASM_SOURCES := src/arch/x86_64/linux/runtime.S
NOLIBC_SUBSTRATE_C_OBJECTS := $(NOLIBC_SUBSTRATE_C_SOURCES:%.c=$(NOLIBC_BUILD_DIR)/obj/substrate/%.o)
NOLIBC_SUBSTRATE_ASM_OBJECTS := $(NOLIBC_SUBSTRATE_ASM_SOURCES:%.S=$(NOLIBC_BUILD_DIR)/obj/substrate/%.o)

VIEWER_BUILD_ARGS ?= --skip-assets --jobs $(JOBS)

.PHONY: all decoder nolibc clean wasm reference-gallery census

ifeq ($(HOST_SYSTEM),Linux)
all: nolibc
else
all: decoder
endif

decoder:
ifeq ($(OS),Windows_NT)
	@$(MAKE) $(DEFAULT_WINDOWS_TARGET)
else
	@$(MAKE) $(NATIVE_DECODER) $(NATIVE_FREESTANDING_STAMP)
endif

census: $(CENSUS)

$(CENSUS): tools/census/census.c $(AVM_BUILD_DIR)/libavm.a
	@mkdir -p $(@D)
	$(CC) $(CENSUS_CFLAGS) $< $(AVM_BUILD_DIR)/libavm.a $(CENSUS_LDFLAGS) -o $@

$(NATIVE_BUILD_DIR)/obj/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(FREESTANDING_CFLAGS) -c $< -o $@

$(NATIVE_LIBRARY): $(NATIVE_OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

$(NATIVE_BUILD_DIR)/obj/cli/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(FREESTANDING_CFLAGS) -c $< -o $@

$(NATIVE_DECODER): src/platform/posix/decoder.c $(NATIVE_CLI_OBJECTS) $(NATIVE_LIBRARY)
	@mkdir -p $(@D)
	$(CC) $(COMMON_WARNINGS) -O2 -Iinclude -Isrc $< $(NATIVE_CLI_OBJECTS) $(NATIVE_LIBRARY) -o $@

$(NATIVE_FREESTANDING_STAMP): $(NATIVE_OBJECTS) $(NATIVE_CLI_OBJECTS) scripts/check-undefined-symbols.sh
	@mkdir -p $(@D)
	@sh scripts/check-undefined-symbols.sh "$(NM)" $(NATIVE_OBJECTS) $(NATIVE_CLI_OBJECTS)
	@touch $@

nolibc:
	@$(MAKE) $(NOLIBC_DECODER) $(NOLIBC_CORE_STAMP) $(NOLIBC_RUNTIME_STAMP)
ifeq ($(HOST_SYSTEM),Darwin)
	$(PYTHON) tools/build_checks.py no-import-macho \
		--input $(NOLIBC_DECODER) --nm "$(NM)"
	$(PYTHON) tools/build_checks.py parity --decoder $(NOLIBC_DECODER) \
		--input tests/fixtures/pc-wiener-restoration.obu \
		--output $(NOLIBC_BUILD_DIR)/pc-wiener-restoration.i420 \
		--expected facb6781bb64f8066feae441fcd8305022f3db4c3fd2ff70c22d2729c5042a0c
else
	$(PYTHON) tools/build_checks.py static-elf \
		--input $(NOLIBC_DECODER) --nm "$(NM)"
	$(PYTHON) tools/build_checks.py parity --decoder $(NOLIBC_DECODER) \
		--input tests/fixtures/i444-high-detail.obu \
		--output $(NOLIBC_BUILD_DIR)/i444-high-detail.i444 \
		--expected 6b8d9d464291a6cea30d9798a3fc593b020456e46c21f15372a39883277f1654
	$(PYTHON) tools/build_checks.py parity --decoder $(NOLIBC_DECODER) \
		--input tests/fixtures/aq-segmentation.obu \
		--output $(NOLIBC_BUILD_DIR)/aq-segmentation.i420 \
		--expected 230e95f06ee03d1b204bb1b913bc926238b0449e665ca1db898d05b4033d5341
ifneq ($(filter x86_64 amd64 AMD64,$(HOST_ARCH)),)
	@$(MAKE) $(NOLIBC_SUBSTRATE)
	$(NOLIBC_SUBSTRATE)
	$(PYTHON) tools/build_checks.py static-elf \
		--input $(NOLIBC_SUBSTRATE) --nm "$(NM)"
endif
endif

$(NOLIBC_BUILD_DIR)/obj/core/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(FREESTANDING_CFLAGS) $(NOLIBC_CORE_EXTRA_CFLAGS) -c $< -o $@

$(NOLIBC_BUILD_DIR)/obj/cli/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(FREESTANDING_CFLAGS) $(NOLIBC_CORE_EXTRA_CFLAGS) -c $< -o $@

$(NOLIBC_BUILD_DIR)/obj/runtime/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(COMMON_WARNINGS) -ffreestanding -fno-builtin \
		-fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables \
		-ffunction-sections -fdata-sections -nostdinc -Isrc -Isrc/shared \
		$(NOLIBC_RUNTIME_EXTRA_CFLAGS) -c $< -o $@

$(NOLIBC_BUILD_DIR)/obj/runtime/%.o: %.S
	@mkdir -p $(@D)
	$(COMPILE_CC) $(NOLIBC_RUNTIME_EXTRA_CFLAGS) -c $< -o $@

$(NOLIBC_BUILD_DIR)/obj/platform/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(COMMON_WARNINGS) -ffreestanding -fno-builtin \
		-fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables \
		-ffunction-sections -fdata-sections -nostdinc -Iinclude -Isrc -Isrc/shared \
		$(NOLIBC_PLATFORM_EXTRA_CFLAGS) -c $< -o $@

$(NOLIBC_BUILD_DIR)/obj/platform/%.o: %.S
	@mkdir -p $(@D)
	$(COMPILE_CC) $(NOLIBC_PLATFORM_EXTRA_CFLAGS) -c $< -o $@

$(NOLIBC_LINKER): $(NEWOS_LINKER_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(NEWOS_LINKER_CFLAGS) $^ $(NEWOS_LINKER_LDFLAGS) -o $@

ifeq ($(HOST_SYSTEM),Darwin)
$(NOLIBC_DECODER): $(NOLIBC_LINKER) $(NOLIBC_PLATFORM_OBJECTS) $(NOLIBC_CLI_OBJECTS) $(NOLIBC_CORE_OBJECTS) $(NOLIBC_RUNTIME_OBJECTS)
	$(NOLIBC_LINKER) --target=mach-o-arm64 --macho-compact --gc-sections \
		--lto-cc=$(CC) -o $@ $(NOLIBC_PLATFORM_OBJECTS) \
		$(NOLIBC_CLI_OBJECTS) $(NOLIBC_CORE_OBJECTS) $(NOLIBC_RUNTIME_OBJECTS)
else
ifneq ($(filter x86_64 amd64 AMD64,$(HOST_ARCH)),)
$(NOLIBC_DECODER): $(NOLIBC_LINKER) $(NOLIBC_PLATFORM_OBJECTS) $(NOLIBC_CLI_OBJECTS) $(NOLIBC_CORE_OBJECTS) $(NOLIBC_RUNTIME_OBJECTS)
	$(NOLIBC_LINKER) --target=elf64-x86_64 --gc-sections --icf=safe --entry=_start \
		$(if $(filter 1,$(NOLIBC_LTO)),--lto-cc=$(firstword $(CC))) \
		-o $@ $(NOLIBC_PLATFORM_OBJECTS) $(NOLIBC_CLI_OBJECTS) $(NOLIBC_CORE_OBJECTS) $(NOLIBC_RUNTIME_OBJECTS)
else
$(NOLIBC_DECODER): $(NOLIBC_PLATFORM_OBJECTS) $(NOLIBC_CLI_OBJECTS) $(NOLIBC_CORE_OBJECTS) $(NOLIBC_RUNTIME_OBJECTS)
	$(CC) $(NOLIBC_LTO_CFLAGS) -nostdlib -static -fuse-ld=lld -Wl,-e,_start \
		-Wl,--gc-sections,--icf=safe,--build-id=none -o $@ $^
endif
endif

$(NOLIBC_CORE_STAMP): $(NOLIBC_CORE_OBJECTS) scripts/check-undefined-symbols.sh
	@mkdir -p $(@D)
	@sh scripts/check-undefined-symbols.sh "$(NOLIBC_NM)" $(NOLIBC_CORE_OBJECTS)
	@touch $@

$(NOLIBC_RUNTIME_STAMP): $(NOLIBC_RUNTIME_OBJECTS) scripts/check-undefined-symbols.sh
	@mkdir -p $(@D)
	@sh scripts/check-undefined-symbols.sh "$(NOLIBC_NM)" $(NOLIBC_RUNTIME_OBJECTS)
	@touch $@

$(NOLIBC_BUILD_DIR)/obj/substrate/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(COMMON_WARNINGS) -Oz -ffreestanding -fno-builtin \
		-fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables \
		-ffunction-sections -fdata-sections -fno-pic -fno-pie -nostdinc \
		-Isrc -Isrc/shared -c $< -o $@

$(NOLIBC_BUILD_DIR)/obj/substrate/%.o: %.S
	@mkdir -p $(@D)
	$(COMPILE_CC) -fno-pic -fno-pie -c $< -o $@

$(NOLIBC_SUBSTRATE): $(NOLIBC_LINKER) $(NOLIBC_SUBSTRATE_C_OBJECTS) \
		$(NOLIBC_SUBSTRATE_ASM_OBJECTS) $(NOLIBC_RUNTIME_OBJECTS)
	$(NOLIBC_LINKER) --target=elf64-x86_64 --gc-sections --icf=safe --entry=_start \
		$(if $(filter 1,$(NOLIBC_LTO)),--lto-cc=$(firstword $(CC))) \
		-o $@ $(NOLIBC_SUBSTRATE_C_OBJECTS) $(NOLIBC_SUBSTRATE_ASM_OBJECTS) \
		$(NOLIBC_RUNTIME_OBJECTS)

clean:
	$(PYTHON) tools/build_checks.py clean

wasm:
	$(PYTHON) experimental/viewerdemo/build.py $(VIEWER_BUILD_ARGS)

reference-gallery:
	$(PYTHON) tools/publish_reference_gallery.py


HOST_BUILD_DIR ?= build/host-sanitized
HOST_CFLAGS := $(COMMON_WARNINGS) -O1 -g -fsanitize=address,undefined \
	-fno-omit-frame-pointer -Iinclude -Isrc
HOST_LDFLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
HOST_CORE_OBJECTS := $(CORE_SOURCES:%.c=$(HOST_BUILD_DIR)/obj/core/%.o)
HOST_CLI_OBJECTS := $(CLI_SOURCES:%.c=$(HOST_BUILD_DIR)/obj/cli/%.o)
HOST_LIBRARY := $(HOST_BUILD_DIR)/libav2heif-host-core.a
HOST_RUNTIME_C_OBJECTS := $(RUNTIME_C_SOURCES:%.c=$(HOST_BUILD_DIR)/obj/runtime/%.o)
HOST_RUNTIME_ASM_OBJECTS := $(RUNTIME_ASM_SOURCES:%.S=$(HOST_BUILD_DIR)/obj/runtime/%.o)
HOST_RUNTIME_OBJECTS := $(HOST_RUNTIME_C_OBJECTS) $(HOST_RUNTIME_ASM_OBJECTS)
HOST_RUNTIME_LIBRARY := $(HOST_BUILD_DIR)/libav2heif-runtime.a
HOST_UNIT_NAMES := substrate container range tile frame cdf partition leaf coeff \
	transform coeff_level reconstruct filter artifact_profile layout output_layout \
	output_parallel pixel_format png_writer preview sample_math sample_ops \
	workspace_accounting public_decode refactor_baseline reconstruction_block \
	reconstruction_plane frame_refactor carriage_refactor prediction_math \
	error_refactor cli
HOST_UNIT_BINARIES := $(foreach name,$(HOST_UNIT_NAMES),\
	$(HOST_BUILD_DIR)/$(subst _,-,$(name))-unit)
HOST_RUNTIME_UNIT := $(HOST_BUILD_DIR)/runtime-substrate-unit
HOST_FUZZ := $(HOST_BUILD_DIR)/public-decode-fuzz
HOST_DECODER := $(HOST_BUILD_DIR)/av2heif-decoder
HOST_ARTIFACT_TOOL := $(HOST_BUILD_DIR)/av2heif-artifact
FILTER_PARITY_DECODER ?= $(HOST_BUILD_DIR)/av2heif-decoder
WP12_DECODER ?= $(HOST_BUILD_DIR)/av2heif-decoder
WP12_WASM ?= docs/decoder.wasm
WP13_DECODER ?= $(HOST_BUILD_DIR)/av2heif-decoder
WP13_WASM ?= build/wasm-wp13/decoder.wasm
WP10_WASM ?= build/wasm-wp10/decoder.wasm
WP11_WASM ?= build/wasm-wp11/decoder.wasm
WP11_WINDOWS_TARGET ?= windows-x86_64
WP11_WINDOWS_BUILD_DIR ?= build/windows-x86_64
WP12_WINDOWS_TARGET ?= windows-x86_64
WP12_WINDOWS_BUILD_DIR ?= build/windows-x86_64
WP13_WINDOWS_TARGET ?= windows-x86_64
WP13_WINDOWS_BUILD_DIR ?= build/windows-x86_64
WP14_WASM ?= build/wasm-wp14/decoder.wasm
REFERENCE_WASM ?= docs/decoder.wasm
WP14_WINDOWS_TARGET ?= windows-x86_64
WP14_WINDOWS_BUILD_DIR ?= build/windows-x86_64
WP14_WINDOWS_EMULATOR ?=
OBJDUMP ?= $(shell command -v llvm-objdump 2>/dev/null || \
	command -v llvm-objdump-19 2>/dev/null || \
	command -v llvm-objdump-18 2>/dev/null || printf '%s' objdump)
LLVM_DLLTOOL ?= $(shell command -v llvm-dlltool 2>/dev/null || \
	command -v llvm-dlltool-19 2>/dev/null || \
	command -v llvm-dlltool-18 2>/dev/null || printf '%s' llvm-dlltool)
LLVM_AR ?= $(shell command -v llvm-ar 2>/dev/null || \
	command -v llvm-ar-19 2>/dev/null || \
	command -v llvm-ar-18 2>/dev/null || printf '%s' llvm-ar)
WINDOWS_ARCH ?= x86_64
ifeq ($(WINDOWS_ARCH),arm64)
WINDOWS_TARGET ?= aarch64-w64-windows-gnu
WINDOWS_DLLTOOL_MACHINE ?= arm64
WINDOWS_BUILD_DIR ?= build/windows-arm64
else
WINDOWS_TARGET ?= x86_64-w64-windows-gnu
WINDOWS_DLLTOOL_MACHINE ?= i386:x86-64
WINDOWS_BUILD_DIR ?= build/windows-x86_64
endif
WINDOWS_CFLAGS := $(FREESTANDING_CFLAGS) --target=$(WINDOWS_TARGET) \
	-mno-stack-arg-probe
WINDOWS_CORE_OBJECTS := $(CORE_SOURCES:%.c=$(WINDOWS_BUILD_DIR)/obj/core/%.o)
WINDOWS_CLI_OBJECTS := $(CLI_SOURCES:%.c=$(WINDOWS_BUILD_DIR)/obj/cli/%.o)
WINDOWS_LIBRARY := $(WINDOWS_BUILD_DIR)/libav2heif-core.a
WINDOWS_IMPORT_LIBRARY := $(WINDOWS_BUILD_DIR)/platform/windows/kernel32.lib
WINDOWS_DECODER_OBJECT := $(WINDOWS_BUILD_DIR)/obj/platform/src/platform/windows/decoder.o
WINDOWS_SMOKE_OBJECT := $(WINDOWS_BUILD_DIR)/obj/platform/src/platform/windows/smoke.o
WINDOWS_DECODER := $(WINDOWS_BUILD_DIR)/av2heif-decoder.exe
WINDOWS_SMOKE := $(WINDOWS_BUILD_DIR)/av2heif-windows-smoke.exe

.PHONY: windows windows-x86_64 windows-arm64

windows: $(WINDOWS_DECODER) $(WINDOWS_SMOKE)

windows-x86_64:
	@$(MAKE) windows WINDOWS_ARCH=x86_64 WINDOWS_TARGET=x86_64-w64-windows-gnu \
		WINDOWS_DLLTOOL_MACHINE=i386:x86-64 WINDOWS_BUILD_DIR=build/windows-x86_64

windows-arm64:
	@$(MAKE) windows WINDOWS_ARCH=arm64 WINDOWS_TARGET=aarch64-w64-windows-gnu \
		WINDOWS_DLLTOOL_MACHINE=arm64 WINDOWS_BUILD_DIR=build/windows-arm64

$(WINDOWS_BUILD_DIR)/obj/core/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(WINDOWS_CFLAGS) -c $< -o $@

$(WINDOWS_BUILD_DIR)/obj/cli/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(WINDOWS_CFLAGS) -c $< -o $@

$(WINDOWS_LIBRARY): $(WINDOWS_CORE_OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	$(LLVM_AR) rcs $@ $^

$(WINDOWS_IMPORT_LIBRARY): src/platform/windows/kernel32.def
	@mkdir -p $(@D)
	$(LLVM_DLLTOOL) -m $(WINDOWS_DLLTOOL_MACHINE) -d $< -l $@

$(WINDOWS_BUILD_DIR)/obj/platform/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(WINDOWS_CFLAGS) -Oz -Isrc/shared -c $< -o $@

$(WINDOWS_DECODER): $(WINDOWS_DECODER_OBJECT) $(WINDOWS_CLI_OBJECTS) $(WINDOWS_LIBRARY) $(WINDOWS_IMPORT_LIBRARY)
	$(CC) --target=$(WINDOWS_TARGET) -nostdlib -fuse-ld=lld \
		-Wl,-e,mainCRTStartup -Wl,-s,--gc-sections,--stack,8388608 \
		-o $@ $(WINDOWS_DECODER_OBJECT) $(WINDOWS_CLI_OBJECTS) $(WINDOWS_LIBRARY) $(WINDOWS_IMPORT_LIBRARY)

$(WINDOWS_SMOKE): $(WINDOWS_SMOKE_OBJECT) $(WINDOWS_LIBRARY) $(WINDOWS_IMPORT_LIBRARY)
	$(CC) --target=$(WINDOWS_TARGET) -nostdlib -fuse-ld=lld \
		-Wl,-e,mainCRTStartup -Wl,-s,--gc-sections,--stack,8388608 \
		-o $@ $(WINDOWS_SMOKE_OBJECT) $(WINDOWS_LIBRARY) $(WINDOWS_IMPORT_LIBRARY)

.PHONY: test test-host-build test-c test-corpus test-freestanding test-private-headers \
	test-runtime-substrate test-extended \
	test-reference-locks test-reference-images test-reference-images-wasm \
	spatial-still-admission spatial-still-platform-admission \
	reference-avm reference-avm-current check-avm-current reference-avm-accounting \
	reference-avm-negatives generate-10bit-sources \
	generate-10bit-avm-references generate-10bit-synthetic \
	generate-10bit-synthetic-references generate-10bit-negatives \
	10bit-wp0 10bit-wp0-reference 10bit-wp1 10bit-wp2 10bit-wp3 10bit-wp4 \
	10bit-wp5 10bit-wp6 10bit-wp7 10bit-wp8 10bit-wp9 \
	10bit-wp10 10bit-wp11 10bit-wp12 10bit-wp13 10bit-wp14 \
	10bit-wp14-fuzz \
	test-carriage-reference test-artifact-lab \
	test-decoder-evidence test-reconstruction test-filter-parity \
	intrabc-admission tcq-pilot spatial-still-frontiers

spatial-still-frontiers: $(HOST_DECODER)
	@$(PYTHON) tools/run_spatial_still_frontiers.py --decoder $(HOST_DECODER)

SPATIAL_STILL_FIXTURES := \
	tests/fixtures/monochrome-odd.obu \
	tests/fixtures/i444-odd.obu \
	tests/fixtures/high-detail-96x80.obu \
	tests/fixtures/predefined-qmatrix.obu \
	tests/fixtures/aq-segmentation.obu \
	tests/fixtures/delta-q-objective.obu \
	tests/fixtures/delta-q-perceptual.obu \
	tests/fixtures/pc-wiener-restoration.obu \
	tests/fixtures/parity-hidden-i444.obu \
	tests/fixtures/lossless-i400.obu \
	tests/fixtures/lossless-i420.obu \
	tests/fixtures/lossless-i444.obu \
	tests/fixtures/spatial-tile-uniform-2x1.obu \
	tests/fixtures/spatial-tile-uniform-1x2.obu \
	tests/fixtures/spatial-tile-filtered-disabled-2x2.obu \
	tests/fixtures/spatial-tile-filtered-nonuniform.obu

.NOTPARALLEL: test test-extended 10bit-wp0-reference

test: test-c test-corpus test-reference-locks test-freestanding test-private-headers \
	spatial-still-admission
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'

$(HOST_BUILD_DIR)/obj/core/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(HOST_CFLAGS) -c $< -o $@

$(HOST_BUILD_DIR)/obj/cli/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(HOST_CFLAGS) -c $< -o $@

$(HOST_LIBRARY): $(HOST_CORE_OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

$(HOST_BUILD_DIR)/obj/runtime/%.o: %.c
	@mkdir -p $(@D)
	$(COMPILE_CC) $(COMMON_WARNINGS) -O2 -ffreestanding -fno-builtin \
		-fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables \
		-ffunction-sections -fdata-sections -nostdinc -Isrc -Isrc/shared -c $< -o $@

$(HOST_BUILD_DIR)/obj/runtime/%.o: %.S
	@mkdir -p $(@D)
	$(COMPILE_CC) -c $< -o $@

$(HOST_RUNTIME_LIBRARY): $(HOST_RUNTIME_OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

define HOST_UNIT_RULE
$(HOST_BUILD_DIR)/$(subst _,-,$(1))-unit: tests/$(1)_unit.c $(HOST_LIBRARY)
	@mkdir -p $$(@D)
	$(CC) $(HOST_CFLAGS) -Isrc $$< $(HOST_LIBRARY) $(HOST_LDFLAGS) -o $$@
endef
$(foreach name,$(filter-out cli,$(HOST_UNIT_NAMES)),$(eval $(call HOST_UNIT_RULE,$(name))))

$(HOST_BUILD_DIR)/cli-unit: tests/cli_unit.c $(HOST_CLI_OBJECTS) $(HOST_LIBRARY)
	@mkdir -p $(@D)
	$(CC) $(HOST_CFLAGS) $< $(HOST_CLI_OBJECTS) $(HOST_LIBRARY) $(HOST_LDFLAGS) -o $@

$(HOST_RUNTIME_UNIT): tests/runtime_substrate_unit.c $(HOST_RUNTIME_LIBRARY)
	@mkdir -p $(@D)
	$(CC) $(COMMON_WARNINGS) -O1 -g -Isrc $< $(HOST_RUNTIME_LIBRARY) -o $@

$(HOST_FUZZ): tests/public_decode_fuzz.c $(HOST_LIBRARY)
	@mkdir -p $(@D)
	$(CC) $(HOST_CFLAGS) -DAV2H_FUZZ_STANDALONE=1 $< $(HOST_LIBRARY) \
		$(HOST_LDFLAGS) -o $@

$(HOST_DECODER): src/platform/posix/decoder.c $(HOST_CLI_OBJECTS) $(HOST_LIBRARY)
	@mkdir -p $(@D)
	$(CC) $(HOST_CFLAGS) $< $(HOST_CLI_OBJECTS) $(HOST_LIBRARY) $(HOST_LDFLAGS) -o $@

$(HOST_ARTIFACT_TOOL): tools/av2heif_artifact.c $(HOST_LIBRARY)
	@mkdir -p $(@D)
	$(CC) $(HOST_CFLAGS) -Isrc $< $(HOST_LIBRARY) $(HOST_LDFLAGS) -o $@

test-host-build: $(HOST_UNIT_BINARIES) $(HOST_RUNTIME_UNIT) $(HOST_FUZZ) \
		$(HOST_DECODER) $(HOST_ARTIFACT_TOOL)

run-host-tests: test-host-build
	@set -e; for test in $(TESTS); do \
		printf '%s\n' "[test] $$test"; \
		$(HOST_BUILD_DIR)/$$test; \
	done

test-private-headers:
	$(PYTHON) tools/check_private_headers.py --cc "$(CC)"
	$(PYTHON) -m unittest discover -s tests -p test_private_headers.py

test-c: test-host-build test-private-headers
	@$(MAKE) run-host-tests TESTS='$(notdir $(HOST_UNIT_BINARIES))'
	$(HOST_RUNTIME_UNIT)
	$(HOST_FUZZ) tests/fixtures/i444-tiny.obu \
		tests/fixtures/10bit/avm-reference/tiny-constrained-16x16-yuv420p10le-qp208.obu \
		tests/fixtures/10bit/avm-reference/tiny-precision-16x16-yuv444p10le-qp128.obu

	$(PYTHON) tools/build_checks.py output-policy --decoder $(HOST_DECODER) \
		--fixture-dir tests/fixtures --output-dir $(HOST_BUILD_DIR)/test-output
	$(PYTHON) tools/build_checks.py parity --decoder $(HOST_DECODER) \
		--input tests/fixtures/pc-wiener-restoration.obu \
		--output $(HOST_BUILD_DIR)/pc-wiener-restoration.i420 \
		--expected facb6781bb64f8066feae441fcd8305022f3db4c3fd2ff70c22d2729c5042a0c
	$(PYTHON) tools/build_checks.py parity --decoder $(HOST_DECODER) \
		--input tests/fixtures/predefined-qmatrix.obu \
		--output $(HOST_BUILD_DIR)/predefined-qmatrix.i420 \
		--expected 6ef34c24eaa892cff0eff2ee75306e85c3432327b53863ac8b06d6d541864b08
	$(PYTHON) tools/build_checks.py parity --decoder $(HOST_DECODER) \
		--input tests/fixtures/aq-segmentation.obu \
		--output $(HOST_BUILD_DIR)/aq-segmentation.i420 \
		--expected 230e95f06ee03d1b204bb1b913bc926238b0449e665ca1db898d05b4033d5341
	$(PYTHON) tools/build_checks.py parity --decoder $(HOST_DECODER) \
		--input tests/fixtures/delta-q-objective.obu \
		--output $(HOST_BUILD_DIR)/delta-q-objective.i420 \
		--expected 54902a83f640d0e24a3a9cc7cee3fbb230f51268226e4ee32394efe2a22a7169
	$(PYTHON) tools/build_checks.py parity --decoder $(HOST_DECODER) \
		--input tests/fixtures/delta-q-perceptual.obu \
		--output $(HOST_BUILD_DIR)/delta-q-perceptual.i420 \
		--expected 0f186b8193016e587779e588e56aa731feef0b86387b9a388da6cda187607456
	$(PYTHON) tools/build_checks.py parity --decoder $(HOST_DECODER) \
		--input tests/fixtures/baya-weaver-current-i420-qp152.obu \
		--output $(HOST_BUILD_DIR)/baya-weaver-current-i420-qp152.i420 \
		--expected 7324889ae632f9e4f75eab47dcc8e430e1df8efde8c9ba68e23884bf5f492b1f

spatial-still-admission: test-host-build
	$(PYTHON) tools/validate_spatial_still_profile.py \
		--decoder $(HOST_DECODER) --host-build $(HOST_BUILD_DIR)
	$(HOST_FUZZ) $(SPATIAL_STILL_FIXTURES)

spatial-still-platform-admission: spatial-still-admission
	@$(MAKE) nolibc
	$(PYTHON) tools/validate_spatial_still_profile.py \
		--decoder $(NOLIBC_DECODER)
	$(PYTHON) experimental/wasm/build.py \
		--output-directory build/wasm-spatial-still
	node tools/validate_spatial_still_wasm.js \
		build/wasm-spatial-still/decoder.wasm
	@$(MAKE) windows-x86_64 windows-arm64
	$(PYTHON) tools/check_windows_imports.py --objdump "$(OBJDUMP)" \
		--decoder build/windows-x86_64/av2heif-decoder.exe \
		--smoke build/windows-x86_64/av2heif-windows-smoke.exe
	$(PYTHON) tools/check_windows_imports.py --objdump "$(OBJDUMP)" \
		--decoder build/windows-arm64/av2heif-decoder.exe \
		--smoke build/windows-arm64/av2heif-windows-smoke.exe

test-corpus: test-host-build
	$(PYTHON) tools/build_checks.py artifact-parity \
		--decoder "$(FILTER_PARITY_DECODER)" --artifact-dir images/artifacts \
		--manifest tests/artifact-parity.sha256 \
		--output-dir build/test/i420-parity --expected-count 31
	$(PYTHON) tools/run_filter_parity.py \
		--decoder "$(FILTER_PARITY_DECODER)" --jobs "$(JOBS)"
	$(PYTHON) tools/validate_8bit_mono_corpus.py \
		--decoder "$(FILTER_PARITY_DECODER)" --jobs "$(JOBS)"
	$(PYTHON) tools/validate_8bit_444_corpus.py \
		--decoder "$(FILTER_PARITY_DECODER)" --jobs "$(JOBS)"
	$(PYTHON) tools/validate_8bit_444_public.py \
		--artifact-tool "$(HOST_ARTIFACT_TOOL)" --jobs "$(JOBS)"

test-reference-locks:
	$(PYTHON) tools/verify_image_references.py --all-suites

test-reference-images: test-host-build
	$(PYTHON) tools/verify_image_references.py --all-suites --require-files \
		--decoder "$(FILTER_PARITY_DECODER)" --jobs "$(JOBS)"

test-reference-images-wasm:
	node tools/validate_image_references_wasm.js "$(REFERENCE_WASM)"

test-freestanding: $(NATIVE_FREESTANDING_STAMP) $(HOST_RUNTIME_OBJECTS)
	@sh scripts/check-undefined-symbols.sh "$(NM)" $(HOST_RUNTIME_OBJECTS)

test-runtime-substrate: $(HOST_RUNTIME_UNIT)
	$(HOST_RUNTIME_UNIT)
	@$(MAKE) $(NOLIBC_SUBSTRATE) $(NOLIBC_RUNTIME_STAMP)
	$(NOLIBC_SUBSTRATE)
	$(PYTHON) tools/build_checks.py static-elf --input $(NOLIBC_SUBSTRATE) --nm "$(NM)"

intrabc-admission: test-host-build
	$(PYTHON) scripts/intrabc_admission.py --host-build $(HOST_BUILD_DIR) --jobs 4

tcq-pilot: test-host-build
	$(PYTHON) scripts/tcq_experiment.py --host-build $(HOST_BUILD_DIR)

test-extended: test test-reference-images test-reference-images-wasm \
	test-decoder-evidence \
	test-carriage-reference test-artifact-lab

reference-avm:
	sh scripts/setup-avm.sh

reference-avm-current:
	AVM_JOBS=$(JOBS) sh scripts/setup-avm.sh --current

check-avm-current: $(HOST_DECODER)
	$(PYTHON) tools/check_avm_current.py --decoder "$(HOST_DECODER)" $(ORACLE_ARGS)

reference-avm-accounting:
	sh scripts/setup-avm-accounting.sh

reference-avm-negatives:
	sh scripts/setup-avm-negatives.sh

generate-10bit-sources:
	$(PYTHON) tools/generate_10bit_sources.py

generate-10bit-avm-references:
	$(PYTHON) tools/generate_10bit_avm_references.py

generate-10bit-synthetic:
	$(PYTHON) tools/generate_10bit_synthetic.py

generate-10bit-synthetic-references: generate-10bit-synthetic reference-avm \
	reference-avm-accounting
	$(PYTHON) tools/generate_10bit_synthetic_references.py

generate-10bit-negatives: reference-avm-negatives
	$(PYTHON) tools/generate_10bit_negative_fixtures.py

10bit-wp0: test-host-build
	$(PYTHON) -m unittest tests.test_10bit_wp0_oracle
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp0-reference: reference-avm reference-avm-accounting
	$(PYTHON) tools/generate_10bit_wp0_oracle.py

10bit-wp1: test-host-build
	@$(MAKE) run-host-tests TESTS='container-unit frame-unit output-layout-unit pixel-format-unit public-decode-unit workspace-accounting-unit'
	$(PYTHON) -m unittest tests.test_10bit_negative_fixtures
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp2: test-host-build
	@$(MAKE) run-host-tests TESTS='layout-unit public-decode-unit sample-ops-unit workspace-accounting-unit'
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp3: test-host-build
	@$(MAKE) run-host-tests TESTS='coeff-level-unit filter-unit frame-unit layout-unit leaf-unit public-decode-unit sample-math-unit sample-ops-unit tile-unit'
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp4: test-host-build
	@$(MAKE) run-host-tests TESTS='leaf-unit public-decode-unit reconstruct-unit tile-unit workspace-accounting-unit'
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp5: test-host-build
	@$(MAKE) run-host-tests TESTS='frame-unit leaf-unit public-decode-unit reconstruct-unit tile-unit workspace-accounting-unit'
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp6: test-host-build
	@$(MAKE) run-host-tests TESTS='coeff-level-unit frame-unit leaf-unit public-decode-unit reconstruct-unit tile-unit workspace-accounting-unit'
	sha256sum --check tests/fixtures/10bit/wp6-unfiltered.sha256
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp7: test-host-build
	@$(MAKE) run-host-tests TESTS='filter-unit frame-unit public-decode-unit reconstruct-unit workspace-accounting-unit'
	sha256sum --check tests/fixtures/10bit/wp6-unfiltered.sha256
	sha256sum --check tests/fixtures/10bit/wp7-deblocked.sha256
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp8: test-host-build
	@$(MAKE) run-host-tests TESTS='filter-unit frame-unit public-decode-unit reconstruct-unit workspace-accounting-unit'
	sha256sum --check tests/fixtures/10bit/wp6-unfiltered.sha256
	sha256sum --check tests/fixtures/10bit/wp7-deblocked.sha256
	sha256sum --check tests/fixtures/10bit/wp8-cdef-ccso.sha256
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp9: test-host-build
	@$(MAKE) run-host-tests TESTS='filter-unit frame-unit output-layout-unit pixel-format-unit public-decode-unit reconstruct-unit workspace-accounting-unit'
	sha256sum --check tests/fixtures/10bit/wp6-unfiltered.sha256
	sha256sum --check tests/fixtures/10bit/wp7-deblocked.sha256
	sha256sum --check tests/fixtures/10bit/wp8-cdef-ccso.sha256
	sha256sum --check tests/fixtures/10bit/wp9-gdf-final.sha256
	$(PYTHON) -m unittest tests.test_10bit_wp0_oracle
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp10: test-host-build
	@$(MAKE) run-host-tests TESTS='container-unit output-layout-unit pixel-format-unit preview-unit public-decode-unit workspace-accounting-unit'
	@$(MAKE) test-freestanding
	$(PYTHON) -m unittest tests.test_10bit_negative_fixtures \
		tests.test_10bit_wp0_oracle
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"
	$(PYTHON) experimental/wasm/build.py \
		--output-directory "$(dir $(WP10_WASM))"
	node tools/validate_10bit_wasm.js "$(WP10_WASM)"

10bit-wp11: test
	$(PYTHON) tools/validate_10bit_420_corpus.py \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder" \
		--artifact-tool "$(HOST_ARTIFACT_TOOL)" --jobs "$(JOBS)"
	@$(MAKE) test-freestanding
	@$(MAKE) nolibc
	$(PYTHON) tools/validate_10bit_420_corpus.py \
		--decoder build/linux-nolibc/av2heif-decoder-nolibc \
		--native-only --jobs "$(JOBS)"
	$(PYTHON) experimental/wasm/build.py \
		--output-directory "$(dir $(WP11_WASM))"
	node tools/validate_10bit_wasm.js "$(WP11_WASM)"
	@$(MAKE) $(WP11_WINDOWS_TARGET)
	$(PYTHON) tools/check_windows_imports.py --objdump "$(OBJDUMP)" \
		--decoder "$(WP11_WINDOWS_BUILD_DIR)/av2heif-decoder.exe" \
		--smoke "$(WP11_WINDOWS_BUILD_DIR)/av2heif-windows-smoke.exe"
	$(PYTHON) tools/check_10bit_reproducibility.py

10bit-wp12: test
	$(PYTHON) tools/validate_10bit_444_wp12.py \
		--decoder "$(WP12_DECODER)"
	$(PYTHON) tools/validate_10bit_420_corpus.py \
		--decoder "$(WP12_DECODER)" \
		--artifact-tool "$(HOST_ARTIFACT_TOOL)" --jobs "$(JOBS)"
	$(PYTHON) tools/build_checks.py artifact-parity --decoder "$(WP12_DECODER)" \
		--artifact-dir images/artifacts --manifest tests/artifact-parity.sha256 \
		--output-dir build/wp12/i420-parity --expected-count 31
	$(PYTHON) tools/validate_8bit_mono_corpus.py \
		--decoder "$(WP12_DECODER)"
	$(PYTHON) tools/validate_8bit_444_corpus.py \
		--decoder "$(WP12_DECODER)"
	$(PYTHON) tools/validate_8bit_444_public.py \
		--artifact-tool "$(HOST_ARTIFACT_TOOL)"
	@$(MAKE) nolibc
	$(PYTHON) tools/validate_10bit_420_corpus.py \
		--decoder build/linux-nolibc/av2heif-decoder-nolibc \
		--native-only --jobs "$(JOBS)"
	$(PYTHON) tools/validate_10bit_444_wp12.py \
		--decoder build/linux-nolibc/av2heif-decoder-nolibc
	$(MAKE) wasm
	$(PYTHON) tools/validate_8bit_444_wasm.py --wasm "$(WP12_WASM)"
	@$(MAKE) $(WP12_WINDOWS_TARGET)
	$(PYTHON) tools/check_windows_imports.py --objdump "$(OBJDUMP)" \
		--decoder "$(WP12_WINDOWS_BUILD_DIR)/av2heif-decoder.exe" \
		--smoke "$(WP12_WINDOWS_BUILD_DIR)/av2heif-windows-smoke.exe"
	$(PYTHON) tools/check_10bit_reproducibility.py

10bit-wp13: test
	$(PYTHON) tools/validate_10bit_444_corpus.py \
		--decoder "$(WP13_DECODER)" \
		--artifact-tool "$(HOST_ARTIFACT_TOOL)" --jobs "$(JOBS)"
	@$(MAKE) nolibc
	$(PYTHON) tools/validate_10bit_444_corpus.py \
		--decoder build/linux-nolibc/av2heif-decoder-nolibc \
		--native-only --jobs "$(JOBS)"
	$(PYTHON) experimental/wasm/build.py \
		--output-directory "$(dir $(WP13_WASM))"
	node tools/validate_10bit_wasm.js "$(WP13_WASM)" yuv444p10le
	@$(MAKE) $(WP13_WINDOWS_TARGET)
	$(PYTHON) tools/check_windows_imports.py --objdump "$(OBJDUMP)" \
		--decoder "$(WP13_WINDOWS_BUILD_DIR)/av2heif-decoder.exe" \
		--smoke "$(WP13_WINDOWS_BUILD_DIR)/av2heif-windows-smoke.exe"

10bit-wp14-fuzz: test-host-build
	$(HOST_FUZZ) tests/fixtures/i444-tiny.obu \
		tests/fixtures/10bit/avm-reference/tiny-constrained-16x16-yuv420p10le-qp208.obu \
		tests/fixtures/10bit/avm-reference/tiny-precision-16x16-yuv444p10le-qp128.obu

10bit-wp14: test
	$(PYTHON) tools/validate_10bit_420_corpus.py \
		--decoder "$(WP13_DECODER)" \
		--artifact-tool "$(HOST_ARTIFACT_TOOL)" --jobs "$(JOBS)"
	$(PYTHON) tools/validate_10bit_444_corpus.py \
		--decoder "$(WP13_DECODER)" \
		--artifact-tool "$(HOST_ARTIFACT_TOOL)" --jobs "$(JOBS)"
	@$(MAKE) nolibc
	$(PYTHON) tools/validate_10bit_420_corpus.py \
		--decoder build/linux-nolibc/av2heif-decoder-nolibc \
		--native-only --jobs "$(JOBS)"
	$(PYTHON) tools/validate_10bit_444_corpus.py \
		--decoder build/linux-nolibc/av2heif-decoder-nolibc \
		--native-only --jobs "$(JOBS)"
	$(PYTHON) experimental/wasm/build.py \
		--output-directory "$(dir $(WP14_WASM))"
	node tools/validate_10bit_wasm.js "$(WP14_WASM)"
	node tools/validate_10bit_wasm.js "$(WP14_WASM)" yuv444p10le
	@$(MAKE) $(WP14_WINDOWS_TARGET)
	$(PYTHON) tools/check_windows_imports.py --objdump "$(OBJDUMP)" \
		--decoder "$(WP14_WINDOWS_BUILD_DIR)/av2heif-decoder.exe" \
		--smoke "$(WP14_WINDOWS_BUILD_DIR)/av2heif-windows-smoke.exe"
	@if test -n "$(WP14_WINDOWS_EMULATOR)"; then \
		$(WP14_WINDOWS_EMULATOR) "$(WP14_WINDOWS_BUILD_DIR)/av2heif-windows-smoke.exe"; \
	else \
		printf '%s\n' 'Windows runtime skipped: set WP14_WINDOWS_EMULATOR'; \
	fi
	$(PYTHON) tools/check_10bit_reproducibility.py

test-carriage-reference: reference-avm test
	$(PYTHON) tools/run_carriage_reference.py \
		--output-dir build/carriage-reference/matrix --jobs "$(JOBS)"

test-artifact-lab: test
	ARTIFACT_LAB_COMPILER="$(CC)" \
	ARTIFACT_LAB_BUILD_OPTIONS='$(HOST_CFLAGS)' \
	$(PYTHON) tools/run_artifact_lab.py \
		--artifact-tool "$(HOST_ARTIFACT_TOOL)" \
		--output-dir build/artifact-lab --jobs "$(JOBS)"

test-decoder-evidence:
	@mkdir -p build/decoder-evidence/evidence
	$(PYTHON) tools/generate_gdf_tables.py \
		--output build/decoder-evidence/evidence/gdf_data.inc
	@cmp src/decoder/gdf_data.inc build/decoder-evidence/evidence/gdf_data.inc
	$(PYTHON) tools/generate_normative_prediction_tables.py \
		--output-directory build/decoder-evidence/evidence
	@cmp src/decoder/intra_dip_data.inc \
		build/decoder-evidence/evidence/intra_dip_data.inc
	@cmp src/decoder/pc_wiener_data.inc \
		build/decoder-evidence/evidence/pc_wiener_data.inc
	$(PYTHON) tools/check_partition_cdfs.py
	$(PYTHON) tools/check_leaf_cdfs.py
	$(PYTHON) tools/check_coeff_cdfs.py
	$(PYTHON) tools/check_transform_cdfs.py
	$(PYTHON) tools/check_prediction.py

test-reconstruction: test-host-build
	@$(MAKE) run-host-tests TESTS='reconstruct-unit tile-unit'

test-filter-parity: test-host-build
	$(PYTHON) -m unittest discover -s tests -p 'test_filter_evidence.py'
	$(PYTHON) tools/run_filter_parity.py \
		--decoder "$(FILTER_PARITY_DECODER)" --jobs "$(JOBS)"
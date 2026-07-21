PYTHON ?= python3
CC ?= cc
NM ?= nm
HOST_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g \
	-fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc
HOST_SUBSTRATE_UNIT := build/host/substrate-unit
HOST_CONTAINER_UNIT := build/host/container-unit
HOST_UNITS := $(HOST_SUBSTRATE_UNIT) $(HOST_CONTAINER_UNIT)
HOST_ARTIFACT_TOOL := build/host/av2heif-artifact
CORE_SOURCES := src/base.c src/carriage.c src/bmff.c src/container.c src/query.c
CORE_HEADERS := include/av2heif.h src/base.h
STRICT_DIR := build/freestanding
STRICT_OBJECTS := $(patsubst src/%.c,$(STRICT_DIR)/%.o,$(CORE_SOURCES))
STRICT_CORE := $(STRICT_DIR)/av2heif-core.o
STRICT_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
	-ffreestanding -fno-builtin -fno-stack-protector \
	-fno-unwind-tables -fno-asynchronous-unwind-tables \
	-ffunction-sections -fdata-sections -nostdinc \
	-Iinclude -Isrc -Isrc/shared

.PHONY: test test-c test-freestanding reference-avm milestone-0 milestone-3 \
	milestone-4

test: test-c test-freestanding $(HOST_ARTIFACT_TOOL)
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'

test-c: $(HOST_UNITS)
	$(HOST_SUBSTRATE_UNIT)
	$(HOST_CONTAINER_UNIT)

$(HOST_SUBSTRATE_UNIT): tests/substrate_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/substrate_unit.c $(CORE_SOURCES) -o $@

$(HOST_CONTAINER_UNIT): tests/container_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/container_unit.c $(CORE_SOURCES) -o $@

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
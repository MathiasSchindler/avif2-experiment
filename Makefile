PYTHON ?= python3
CC ?= cc
NM ?= nm
HOST_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g \
	-fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc
HOST_UNIT := build/host/core-unit
CORE_SOURCES := src/base.c src/carriage.c src/bmff.c
CORE_HEADERS := include/av2heif.h src/base.h
STRICT_DIR := build/freestanding
STRICT_OBJECTS := $(patsubst src/%.c,$(STRICT_DIR)/%.o,$(CORE_SOURCES))
STRICT_CORE := $(STRICT_DIR)/av2heif-core.o
STRICT_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
	-ffreestanding -fno-builtin -fno-stack-protector \
	-fno-unwind-tables -fno-asynchronous-unwind-tables \
	-ffunction-sections -fdata-sections -nostdinc \
	-Iinclude -Isrc -Isrc/shared

.PHONY: test test-c test-freestanding reference-avm milestone-0

test: test-c test-freestanding
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'

test-c: $(HOST_UNIT)
	$(HOST_UNIT)

$(HOST_UNIT): tests/substrate_unit.c $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) tests/substrate_unit.c $(CORE_SOURCES) -o $@

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
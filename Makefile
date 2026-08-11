CC ?= cc
JOBS ?= $(or $(NUMBER_OF_PROCESSORS),$(shell getconf _NPROCESSORS_ONLN 2>/dev/null),$(shell sysctl -n hw.ncpu 2>/dev/null),1)
CMAKE_BUILD_ARGS ?= --parallel $(JOBS)
CTEST_PARALLEL_ARGS ?= --parallel $(JOBS)

ifeq ($(OS),Windows_NT)
PYTHON ?= python
ifneq ($(filter ARM64,$(PROCESSOR_ARCHITECTURE) $(PROCESSOR_ARCHITEW6432)),)
DECODER_PRESET ?= windows-arm64
DECODER_BUILD_DIR ?= build/windows-arm64
else
DECODER_PRESET ?= windows-x86_64
DECODER_BUILD_DIR ?= build/windows-x86_64
endif
else
PYTHON ?= python3
DECODER_PRESET ?= native
DECODER_BUILD_DIR ?= build/native
endif

VIEWER_BUILD_ARGS ?= --skip-assets --jobs $(JOBS)

.PHONY: all decoder nolibc clean wasm

all: decoder

decoder:
	cmake --preset $(DECODER_PRESET)
	cmake --build --preset $(DECODER_PRESET) $(CMAKE_BUILD_ARGS) --target av2heif-decoder av2heif-freestanding-check

nolibc:
	cmake --preset linux-nolibc
	cmake --build --preset linux-nolibc $(CMAKE_BUILD_ARGS) --target av2heif-decoder-nolibc av2heif-freestanding-check
	ctest --preset linux-nolibc $(CTEST_PARALLEL_ARGS)

clean:
	cmake -E rm -rf $(DECODER_BUILD_DIR)

wasm:
	$(PYTHON) experimental/viewerdemo/build.py $(VIEWER_BUILD_ARGS)

HOST_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g \
	-fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc
HOST_PRESET ?= host-sanitized
HOST_BUILD_DIR ?= build/host-sanitized
CTEST_ARGS ?= --output-on-failure
HOST_ARTIFACT_TOOL := $(HOST_BUILD_DIR)/av2heif-artifact
FILTER_PARITY_DECODER ?= $(HOST_BUILD_DIR)/av2heif-decoder
WP12_DECODER ?= $(HOST_BUILD_DIR)/av2heif-decoder
WP12_WASM ?= docs/decoder.wasm
WP12_WINDOWS_PRESET ?= windows-x86_64
WP12_WINDOWS_BUILD_DIR ?= build/windows-x86_64
OBJDUMP ?= objdump
.PHONY: test test-host-build test-c test-corpus test-freestanding test-extended \
	reference-avm reference-avm-accounting \
	reference-avm-negatives generate-10bit-sources \
	generate-10bit-avm-references generate-10bit-synthetic \
	generate-10bit-synthetic-references generate-10bit-negatives \
	10bit-wp0 10bit-wp0-reference 10bit-wp1 10bit-wp2 10bit-wp3 10bit-wp4 \
	10bit-wp5 10bit-wp6 10bit-wp7 10bit-wp8 10bit-wp9 \
	10bit-wp10 10bit-wp12 \
	test-carriage-reference test-artifact-lab \
	test-decoder-evidence test-reconstruction test-filter-parity

test: test-c test-corpus test-freestanding
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'

test-host-build:
	cmake --preset $(HOST_PRESET)
	cmake --build --preset $(HOST_PRESET) $(CMAKE_BUILD_ARGS)

test-c: test-host-build
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS)

test-corpus: test-host-build
	cmake -DDECODER="$(abspath $(FILTER_PARITY_DECODER))" \
		-DARTIFACT_DIR="$(abspath images/artifacts)" \
		-DMANIFEST="$(abspath tests/artifact-parity.sha256)" \
		-DOUTPUT_DIR="$(abspath build/test/i420-parity)" \
		-DEXPECTED_COUNT=31 \
		-P scripts/check-artifact-parity.cmake
	$(PYTHON) tools/run_filter_parity.py \
		--decoder "$(FILTER_PARITY_DECODER)" --jobs "$(JOBS)"
	$(PYTHON) tools/validate_8bit_mono_corpus.py \
		--decoder "$(FILTER_PARITY_DECODER)" --jobs "$(JOBS)"
	$(PYTHON) tools/validate_8bit_444_corpus.py \
		--decoder "$(FILTER_PARITY_DECODER)" --jobs "$(JOBS)"
	$(PYTHON) tools/validate_8bit_444_public.py \
		--artifact-tool "$(HOST_ARTIFACT_TOOL)" --jobs "$(JOBS)"

test-freestanding: test-host-build
	cmake --build --preset $(HOST_PRESET) $(CMAKE_BUILD_ARGS) --target av2heif-freestanding-check

test-extended: test test-decoder-evidence test-carriage-reference test-artifact-lab

reference-avm:
	sh scripts/setup-avm.sh

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
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS) \
		-R '^(container-unit|frame-unit|output-layout-unit|pixel-format-unit|public-decode-unit|workspace-accounting-unit)$$'
	$(PYTHON) -m unittest tests.test_10bit_negative_fixtures
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp2: test-host-build
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS) \
		-R '^(layout-unit|public-decode-unit|sample-ops-unit|workspace-accounting-unit)$$'
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp3: test-host-build
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS) \
		-R '^(coeff-level-unit|filter-unit|frame-unit|layout-unit|leaf-unit|public-decode-unit|sample-math-unit|sample-ops-unit|tile-unit)$$'
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp4: test-host-build
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS) \
		-R '^(leaf-unit|public-decode-unit|reconstruct-unit|tile-unit|workspace-accounting-unit)$$'
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp5: test-host-build
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS) \
		-R '^(frame-unit|leaf-unit|public-decode-unit|reconstruct-unit|tile-unit|workspace-accounting-unit)$$'
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp6: test-host-build
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS) \
		-R '^(coeff-level-unit|frame-unit|leaf-unit|public-decode-unit|reconstruct-unit|tile-unit|workspace-accounting-unit)$$'
	sha256sum --check tests/fixtures/10bit/wp6-unfiltered.sha256
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp7: test-host-build
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS) \
		-R '^(filter-unit|frame-unit|public-decode-unit|reconstruct-unit|workspace-accounting-unit)$$'
	sha256sum --check tests/fixtures/10bit/wp6-unfiltered.sha256
	sha256sum --check tests/fixtures/10bit/wp7-deblocked.sha256
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp8: test-host-build
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS) \
		-R '^(filter-unit|frame-unit|public-decode-unit|reconstruct-unit|workspace-accounting-unit)$$'
	sha256sum --check tests/fixtures/10bit/wp6-unfiltered.sha256
	sha256sum --check tests/fixtures/10bit/wp7-deblocked.sha256
	sha256sum --check tests/fixtures/10bit/wp8-cdef-ccso.sha256
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp9: test-host-build
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS) \
		-R '^(filter-unit|frame-unit|output-layout-unit|pixel-format-unit|public-decode-unit|reconstruct-unit|workspace-accounting-unit)$$'
	sha256sum --check tests/fixtures/10bit/wp6-unfiltered.sha256
	sha256sum --check tests/fixtures/10bit/wp7-deblocked.sha256
	sha256sum --check tests/fixtures/10bit/wp8-cdef-ccso.sha256
	sha256sum --check tests/fixtures/10bit/wp9-gdf-final.sha256
	$(PYTHON) -m unittest tests.test_10bit_wp0_oracle
	$(PYTHON) tools/check_10bit_wp0_oracle.py verify \
		--decoder "$(HOST_BUILD_DIR)/av2heif-decoder"

10bit-wp10: generate-10bit-synthetic-references generate-10bit-negatives
	$(PYTHON) -m unittest tests.test_10bit_synthetic \
		tests.test_10bit_avm_references tests.test_10bit_negative_fixtures

10bit-wp12: test
	cmake -DDECODER="$(abspath $(WP12_DECODER))" \
		-DARTIFACT_DIR="$(abspath images/artifacts)" \
		-DMANIFEST="$(abspath tests/artifact-parity.sha256)" \
		-DOUTPUT_DIR="$(abspath build/wp12/i420-parity)" \
		-DEXPECTED_COUNT=31 \
		-P scripts/check-artifact-parity.cmake
	$(PYTHON) tools/validate_8bit_mono_corpus.py \
		--decoder "$(WP12_DECODER)"
	$(PYTHON) tools/validate_8bit_444_corpus.py \
		--decoder "$(WP12_DECODER)"
	$(PYTHON) tools/validate_8bit_444_public.py \
		--artifact-tool "$(HOST_ARTIFACT_TOOL)"
	cmake --preset linux-nolibc
	cmake --build --preset linux-nolibc $(CMAKE_BUILD_ARGS)
	ctest --preset linux-nolibc $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS)
	$(MAKE) wasm
	$(PYTHON) tools/validate_8bit_444_wasm.py --wasm "$(WP12_WASM)"
	cmake --preset $(WP12_WINDOWS_PRESET)
	cmake --build --preset $(WP12_WINDOWS_PRESET) $(CMAKE_BUILD_ARGS)
	$(PYTHON) tools/check_windows_imports.py --objdump "$(OBJDUMP)" \
		--decoder "$(WP12_WINDOWS_BUILD_DIR)/av2heif-decoder.exe" \
		--smoke "$(WP12_WINDOWS_BUILD_DIR)/av2heif-windows-smoke.exe"
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
	$(PYTHON) tools/check_partition_cdfs.py
	$(PYTHON) tools/check_leaf_cdfs.py
	$(PYTHON) tools/check_coeff_cdfs.py
	$(PYTHON) tools/check_transform_cdfs.py
	$(PYTHON) tools/check_prediction.py

test-reconstruction: test-host-build
	ctest --preset $(HOST_PRESET) $(CTEST_ARGS) $(CTEST_PARALLEL_ARGS) \
		-R '^(reconstruct-unit|tile-unit)$$'

test-filter-parity: test-host-build
	$(PYTHON) -m unittest discover -s tests -p 'test_filter_evidence.py'
	$(PYTHON) tools/run_filter_parity.py \
		--decoder "$(FILTER_PARITY_DECODER)" --jobs "$(JOBS)"
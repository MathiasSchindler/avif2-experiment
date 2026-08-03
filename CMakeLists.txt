cmake_minimum_required(VERSION 3.20)

project(av2heif LANGUAGES C)

set(AV2HEIF_CORE_SOURCES
    src/base.c
    src/carriage.c
    src/bmff.c
    src/container.c
    src/query.c
    src/decoder/cdf.c
    src/decoder/coeff.c
    src/decoder/coeff_level.c
    src/decoder/filter.c
    src/decoder/frame.c
    src/decoder/frame_output.c
    src/decoder/leaf.c
    src/decoder/partition.c
    src/decoder/partition_cdf.c
    src/decoder/range.c
    src/decoder/reconstruct.c
    src/decoder/tile.c
    src/decoder/transform.c
    src/decoder/tx_geometry.c
)

add_library(av2heif-core STATIC ${AV2HEIF_CORE_SOURCES})
target_compile_features(av2heif-core PUBLIC c_std_11)
target_include_directories(av2heif-core
    PUBLIC include
    PRIVATE src src/shared
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(av2heif-core PRIVATE
        -Wall -Wextra -Wpedantic -Werror -O2
        -ffreestanding -fno-builtin -fno-stack-protector
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -nostdinc
    )
endif()

if(WIN32 AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_STANDARD_LIBRARIES "")
    find_program(LLVM_DLLTOOL llvm-dlltool REQUIRED)
    target_compile_options(av2heif-core PRIVATE -mno-stack-arg-probe)

    set(KERNEL32_IMPORT_LIBRARY
        "${CMAKE_CURRENT_BINARY_DIR}/platform/windows/kernel32.lib")
    add_custom_command(
        OUTPUT "${KERNEL32_IMPORT_LIBRARY}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/platform/windows"
        COMMAND "${LLVM_DLLTOOL}"
                -m i386:x86-64
                -d "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows/kernel32.def"
                -l "${KERNEL32_IMPORT_LIBRARY}"
        DEPENDS src/platform/windows/kernel32.def
        VERBATIM
    )
    add_custom_target(av2heif-kernel32-import
        DEPENDS "${KERNEL32_IMPORT_LIBRARY}")

    add_executable(av2heif-windows-smoke src/platform/windows/smoke.c)
    add_dependencies(av2heif-windows-smoke av2heif-kernel32-import)
    target_include_directories(av2heif-windows-smoke PRIVATE src src/shared)
    target_link_libraries(av2heif-windows-smoke PRIVATE
        av2heif-core "${KERNEL32_IMPORT_LIBRARY}")
    target_compile_options(av2heif-windows-smoke PRIVATE
        -Wall -Wextra -Wpedantic -Werror -Oz
        -ffreestanding -fno-builtin -fno-stack-protector
        -mno-stack-arg-probe
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -nostdinc
    )
    target_link_options(av2heif-windows-smoke PRIVATE
        -nostdlib -fuse-ld=lld
        -Wl,-e,mainCRTStartup
        -Wl,-s,--gc-sections,--stack,8388608
    )

    add_executable(av2heif-decoder src/platform/windows/decoder.c)
    add_dependencies(av2heif-decoder av2heif-kernel32-import)
    target_include_directories(av2heif-decoder PRIVATE src src/shared)
    target_link_libraries(av2heif-decoder PRIVATE
        av2heif-core "${KERNEL32_IMPORT_LIBRARY}")
    target_compile_options(av2heif-decoder PRIVATE
        -Wall -Wextra -Wpedantic -Werror -Oz
        -ffreestanding -fno-builtin -fno-stack-protector
        -mno-stack-arg-probe
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -nostdinc
    )
    target_link_options(av2heif-decoder PRIVATE
        -nostdlib -fuse-ld=lld
        -Wl,-e,mainCRTStartup
        -Wl,-s,--gc-sections
        "SHELL:-Xlinker --stack -Xlinker 8388608,8388608"
    )

    enable_testing()
    add_test(NAME windows-smoke COMMAND av2heif-windows-smoke)
    add_test(NAME windows-decoder-emerald-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/emerald-192x96-qp208.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/emerald-decoder.i420")
    add_test(NAME windows-decoder-emerald-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/emerald-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/emerald-192x96-qp208-decoded.i420")
    set_tests_properties(windows-decoder-emerald-i420-compare PROPERTIES
        DEPENDS windows-decoder-emerald-i420)
    add_test(NAME windows-decoder-chinnathurai-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/chinnathurai-333x333-qp224.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/chinnathurai-decoder.i420")
    add_test(NAME windows-decoder-chinnathurai-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/chinnathurai-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/chinnathurai-333x333-qp224-decoded.i420")
    set_tests_properties(windows-decoder-chinnathurai-i420-compare PROPERTIES
        DEPENDS windows-decoder-chinnathurai-i420)
    add_test(NAME windows-decoder-schwaebisch-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/schwaebisch-417x277-qp208.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/schwaebisch-decoder.i420")
    add_test(NAME windows-decoder-emerald-bmp
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/emerald-192x96-qp208.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/emerald-decoder.bmp")
    add_test(NAME windows-decoder-chalk-q128-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/chalk-320x240-qp128.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/chalk-q128-decoder.i420")
    add_test(NAME windows-decoder-cuckoo-q160-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/cuckoo-157x105-qp160.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/cuckoo-q160-decoder.i420")
    add_test(NAME windows-decoder-cuckoo-q160-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/cuckoo-q160-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/cuckoo-157x105-qp160-decoded.i420")
    set_tests_properties(windows-decoder-cuckoo-q160-i420-compare PROPERTIES
        DEPENDS windows-decoder-cuckoo-q160-i420)
    add_test(NAME windows-decoder-chimp-q064-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/chimp-93x111-qp064.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/chimp-q064-decoder.i420")
    add_test(NAME windows-decoder-chimp-q064-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/chimp-q064-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/chimp-93x111-qp064-decoded.i420")
    set_tests_properties(windows-decoder-chimp-q064-i420-compare PROPERTIES
        DEPENDS windows-decoder-chimp-q064-i420)
    add_test(NAME windows-decoder-pastell-q064-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/pastell-320x213-qp064.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/pastell-q064-decoder.i420")
    add_test(NAME windows-decoder-pastell-q064-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/pastell-q064-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/pastell-320x213-qp064-decoded.i420")
    set_tests_properties(windows-decoder-pastell-q064-i420-compare PROPERTIES
        DEPENDS windows-decoder-pastell-q064-i420)
    add_test(NAME windows-decoder-cuckoo-q080-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/cuckoo-431x287-qp080.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/cuckoo-q080-decoder.i420")
    add_test(NAME windows-decoder-cuckoo-q080-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/cuckoo-q080-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/cuckoo-431x287-qp080-decoded.i420")
    set_tests_properties(windows-decoder-cuckoo-q080-i420-compare PROPERTIES
        DEPENDS windows-decoder-cuckoo-q080-i420)
    add_test(NAME windows-decoder-flycatcher-q176-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/flycatcher-223x149-qp176.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/flycatcher-q176-decoder.i420")
    add_test(NAME windows-decoder-flycatcher-q176-i420-hash
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${CMAKE_CURRENT_BINARY_DIR}/flycatcher-q176-decoder.i420"
            "-DEXPECTED_SHA256=38abac499fc98cb79b4c1e6cf166be2c905fb549771d8b51b3161a926b53c3f2"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-file-sha256.cmake")
    set_tests_properties(windows-decoder-flycatcher-q176-i420-hash PROPERTIES
        DEPENDS windows-decoder-flycatcher-q176-i420)
    add_test(NAME windows-decoder-pastell-q160-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/pastell-512x341-qp160.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/pastell-q160-decoder.i420")
    add_test(NAME windows-decoder-pastell-q160-i420-hash
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${CMAKE_CURRENT_BINARY_DIR}/pastell-q160-decoder.i420"
            "-DEXPECTED_SHA256=7277ababbf237deffb0e28802bdf1148a8ab379d4c4bde31cecf3ac371a519a8"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-file-sha256.cmake")
    set_tests_properties(windows-decoder-pastell-q160-i420-hash PROPERTIES
        DEPENDS windows-decoder-pastell-q160-i420)
    add_test(NAME windows-decoder-louis-q224-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/louis-463x342-qp224.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/louis-q224-decoder.i420")
    add_test(NAME windows-decoder-louis-q224-i420-hash
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${CMAKE_CURRENT_BINARY_DIR}/louis-q224-decoder.i420"
            "-DEXPECTED_SHA256=cbc741b34cc55af162d99e44273a6bb01a2f6152cb10ee5884248ceef33d0427"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-file-sha256.cmake")
    set_tests_properties(windows-decoder-louis-q224-i420-hash PROPERTIES
        DEPENDS windows-decoder-louis-q224-i420)
    add_test(NAME windows-decoder-silene-q080-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/silene-384x307-qp080.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/silene-q080-decoder.i420")
    add_test(NAME windows-decoder-silene-q080-i420-hash
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${CMAKE_CURRENT_BINARY_DIR}/silene-q080-decoder.i420"
            "-DEXPECTED_SHA256=613afd94352e93f2d2a6a343bcb77c2231bb693e38995f3bfef1e7c9d7eb2b1d"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-file-sha256.cmake")
    set_tests_properties(windows-decoder-silene-q080-i420-hash PROPERTIES
        DEPENDS windows-decoder-silene-q080-i420)
    add_test(NAME windows-decoder-chimp-q192-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/chimp-240x287-qp192.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/chimp-q192-decoder.i420")
    add_test(NAME windows-decoder-chimp-q192-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/chimp-q192-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/chimp-240x287-qp192-decoded.i420")
    set_tests_properties(windows-decoder-chimp-q192-i420-compare PROPERTIES
        DEPENDS windows-decoder-chimp-q192-i420)
    add_test(NAME windows-decoder-silene-q176-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/silene-479x383-qp176.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/silene-q176-decoder.i420")
    add_test(NAME windows-decoder-silene-q176-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/silene-q176-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/silene-479x383-qp176-decoded.i420")
    set_tests_properties(windows-decoder-silene-q176-i420-compare PROPERTIES
        DEPENDS windows-decoder-silene-q176-i420)
    add_test(NAME windows-decoder-louis-q144-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/louis-301x222-qp144.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/louis-q144-decoder.i420")
    add_test(NAME windows-decoder-louis-q144-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/louis-q144-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/louis-301x222-qp144-decoded.i420")
    set_tests_properties(windows-decoder-louis-q144-i420-compare PROPERTIES
        DEPENDS windows-decoder-louis-q144-i420)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME windows-decoder-milestone-9-parity
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_m9_final_parity.py"
                --decoder "$<TARGET_FILE:av2heif-decoder>")
    endif()
    add_test(NAME windows-decoder-laarim-q096
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/laarim-256x171-qp096.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/laarim-q096-decoder.i420")
    add_test(NAME windows-decoder-flycatcher-q048
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/flycatcher-397x265-qp048.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/flycatcher-q048-decoder.i420")
endif()
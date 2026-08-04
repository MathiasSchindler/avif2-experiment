cmake_minimum_required(VERSION 3.20)

project(av2heif LANGUAGES C)

include(CTest)

option(AV2HEIF_BUILD_HOST_TESTS "Build hosted C unit tests" OFF)
option(AV2HEIF_ENABLE_SANITIZERS "Enable sanitizers for hosted tests" OFF)
option(AV2HEIF_ENABLE_LEGACY_HOST_TESTS
    "Run hosted fixture tests that require expectation updates" OFF)
option(AV2HEIF_ENABLE_PYTHON_TESTS "Enable Python development tests" OFF)
option(AV2HEIF_BUILD_WINDOWS_TOOLS "Build freestanding Windows executables" ON)

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

if(AV2HEIF_BUILD_HOST_TESTS)
    function(av2heif_configure_host_target target)
        target_compile_features(${target} PRIVATE c_std_11)
        if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target} PRIVATE
                -Wall -Wextra -Wpedantic -Werror -O1 -g
            )
            if(AV2HEIF_ENABLE_SANITIZERS)
                target_compile_options(${target} PRIVATE
                    -fsanitize=address,undefined -fno-omit-frame-pointer
                )
                target_link_options(${target} PRIVATE
                    -fsanitize=address,undefined -fno-omit-frame-pointer
                )
            endif()
        endif()
    endfunction()

    add_library(av2heif-host-core STATIC ${AV2HEIF_CORE_SOURCES})
    av2heif_configure_host_target(av2heif-host-core)
    target_include_directories(av2heif-host-core
        PUBLIC include
        PRIVATE src
    )

    set(AV2HEIF_HOST_UNIT_NAMES
        substrate
        container
        range
        tile
        frame
        cdf
        partition
        leaf
        coeff
        transform
        coeff_level
        reconstruct
        filter
        artifact_profile
    )
    foreach(unit_name IN LISTS AV2HEIF_HOST_UNIT_NAMES)
        string(REPLACE "_" "-" unit_target "${unit_name}-unit")
        add_executable(${unit_target} "tests/${unit_name}_unit.c")
        av2heif_configure_host_target(${unit_target})
        target_include_directories(${unit_target} PRIVATE src)
        target_link_libraries(${unit_target} PRIVATE av2heif-host-core)
        if(AV2HEIF_ENABLE_LEGACY_HOST_TESTS OR
           NOT unit_name MATCHES
               "^(tile|leaf|transform|reconstruct|artifact_profile)$")
            add_test(NAME ${unit_target} COMMAND ${unit_target})
            set_tests_properties(${unit_target} PROPERTIES
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            )
        endif()
    endforeach()

    add_executable(av2heif-artifact tools/av2heif_artifact.c)
    av2heif_configure_host_target(av2heif-artifact)
    target_include_directories(av2heif-artifact PRIVATE src)
    target_link_libraries(av2heif-artifact PRIVATE av2heif-host-core)
endif()

if(WIN32 AND AV2HEIF_BUILD_WINDOWS_TOOLS AND
    CMAKE_C_COMPILER_ID MATCHES "Clang")
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
        --target=x86_64-w64-windows-gnu
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
        --target=x86_64-w64-windows-gnu
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
    add_test(NAME windows-decoder-emerald-q128-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/emerald-480x240-qp128.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/emerald-q128-decoder.i420")
    add_test(NAME windows-decoder-emerald-q128-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/emerald-q128-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/emerald-480x240-qp128-decoded.i420")
    set_tests_properties(windows-decoder-emerald-q128-i420-compare PROPERTIES
        DEPENDS windows-decoder-emerald-q128-i420)
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
    add_test(NAME windows-decoder-schwaebisch-q112-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/schwaebisch-353x234-qp112.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/schwaebisch-q112-decoder.i420")
    add_test(NAME windows-decoder-schwaebisch-q112-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/schwaebisch-q112-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/schwaebisch-353x234-qp112-decoded.i420")
    set_tests_properties(windows-decoder-schwaebisch-q112-i420-compare PROPERTIES
        DEPENDS windows-decoder-schwaebisch-q112-i420)
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
            "-DEXPECTED_SHA256=797b6653768762b4c7c7cdc5c3732267e745ced793203d3d3097d6303f98f6b4"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-file-sha256.cmake")
    set_tests_properties(windows-decoder-flycatcher-q176-i420-hash PROPERTIES
        DEPENDS windows-decoder-flycatcher-q176-i420)
    add_test(NAME windows-decoder-flycatcher-q048-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/flycatcher-397x265-qp048.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/flycatcher-q048-decoder.i420")
    add_test(NAME windows-decoder-flycatcher-q048-i420-hash
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${CMAKE_CURRENT_BINARY_DIR}/flycatcher-q048-decoder.i420"
            "-DEXPECTED_SHA256=a932aafe1788f5343d71d197d5186199ef1b8491920d4e52fc6ac8a81872ff5a"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-file-sha256.cmake")
    set_tests_properties(windows-decoder-flycatcher-q048-i420-hash PROPERTIES
        DEPENDS windows-decoder-flycatcher-q048-i420)
    add_test(NAME windows-decoder-laarim-q024-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/laarim-389x259-qp024.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/laarim-q024-decoder.i420")
    add_test(NAME windows-decoder-laarim-q024-i420-hash
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${CMAKE_CURRENT_BINARY_DIR}/laarim-q024-decoder.i420"
            "-DEXPECTED_SHA256=d5e97442c5d2b2a17a5ce2c56ae179183d5cfdfdbf99e0b1cff319889319d752"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-file-sha256.cmake")
    set_tests_properties(windows-decoder-laarim-q024-i420-hash PROPERTIES
        DEPENDS windows-decoder-laarim-q024-i420)
    add_test(NAME windows-decoder-pastell-q232-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/pastell-431x287-qp232.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/pastell-q232-decoder.i420")
    add_test(NAME windows-decoder-pastell-q232-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/pastell-q232-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/pastell-431x287-qp232-decoded.i420")
    set_tests_properties(windows-decoder-pastell-q232-i420-compare PROPERTIES
        DEPENDS windows-decoder-pastell-q232-i420)
    add_test(NAME windows-decoder-pastell-q160-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/pastell-512x341-qp160.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/pastell-q160-decoder.i420")
    add_test(NAME windows-decoder-pastell-q160-i420-hash
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${CMAKE_CURRENT_BINARY_DIR}/pastell-q160-decoder.i420"
            "-DEXPECTED_SHA256=b25f82b615a0b96580d0f664dce2bd81b178083f3bf246c4836e17e01c593ab9"
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
            "-DEXPECTED_SHA256=81116e50289f0b793075c7a08de8a68db5a7a1fa43dadcdb2f6155ce5d166dd9"
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
            "-DEXPECTED_SHA256=1388a9512aa75578a0445d3ada7cdf4de1988e3c10e177bd0b71438606a96ef7"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-file-sha256.cmake")
    set_tests_properties(windows-decoder-silene-q080-i420-hash PROPERTIES
        DEPENDS windows-decoder-silene-q080-i420)
    add_test(NAME windows-decoder-silene-q120-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/silene-425x340-qp120.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/silene-q120-decoder.i420")
    add_test(NAME windows-decoder-silene-q120-i420-hash
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${CMAKE_CURRENT_BINARY_DIR}/silene-q120-decoder.i420"
            "-DEXPECTED_SHA256=1e08dd8c11593832d20175afa84b9c07f1dc3eb5355c73d4fdeca8b9a166ddf5"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-file-sha256.cmake")
    set_tests_properties(windows-decoder-silene-q120-i420-hash PROPERTIES
        DEPENDS windows-decoder-silene-q120-i420)
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
    add_test(NAME windows-decoder-louis-q184-i420
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/louis-377x279-qp184.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/louis-q184-decoder.i420")
    add_test(NAME windows-decoder-louis-q184-i420-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/louis-q184-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/louis-377x279-qp184-decoded.i420")
    set_tests_properties(windows-decoder-louis-q184-i420-compare PROPERTIES
        DEPENDS windows-decoder-louis-q184-i420)
    if(AV2HEIF_ENABLE_PYTHON_TESTS)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME windows-decoder-milestone-9-parity
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_m9_final_parity.py"
                --decoder "$<TARGET_FILE:av2heif-decoder>")
    endif()
    add_test(NAME windows-decoder-laarim-q096
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/laarim-256x171-qp096.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/laarim-q096-decoder.i420")
    add_test(NAME windows-decoder-laarim-q096-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/laarim-q096-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/laarim-256x171-qp096-decoded.i420")
    set_tests_properties(windows-decoder-laarim-q096-compare PROPERTIES
        DEPENDS windows-decoder-laarim-q096)
    add_test(NAME windows-decoder-laarim-q192
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/laarim-448x299-qp192.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/laarim-q192-decoder.i420")
    add_test(NAME windows-decoder-laarim-q192-compare
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/laarim-q192-decoder.i420"
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/laarim-448x299-qp192-decoded.i420")
    set_tests_properties(windows-decoder-laarim-q192-compare PROPERTIES
        DEPENDS windows-decoder-laarim-q192)
    add_test(NAME windows-decoder-flycatcher-q048
        COMMAND av2heif-decoder
            "${CMAKE_CURRENT_SOURCE_DIR}/images/artifacts/flycatcher-397x265-qp048.obu"
            -o "${CMAKE_CURRENT_BINARY_DIR}/flycatcher-q048-decoder.i420")
endif()
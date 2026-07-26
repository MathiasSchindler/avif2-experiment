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
    src/decoder/leaf.c
    src/decoder/partition.c
    src/decoder/partition_cdf.c
    src/decoder/range.c
    src/decoder/reconstruct.c
    src/decoder/tile.c
    src/decoder/transform.c
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
    target_include_directories(av2heif-windows-smoke PRIVATE src/shared)
    target_link_libraries(av2heif-windows-smoke PRIVATE
        av2heif-core "${KERNEL32_IMPORT_LIBRARY}")
    target_compile_options(av2heif-windows-smoke PRIVATE
        -Wall -Wextra -Wpedantic -Werror -Oz
        -ffreestanding -fno-builtin -fno-stack-protector
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -nostdinc
    )
    target_link_options(av2heif-windows-smoke PRIVATE
        -nostdlib -fuse-ld=lld
        -Wl,-e,mainCRTStartup
        -Wl,-s,--gc-sections,--stack,8388608
    )

    enable_testing()
    add_test(NAME windows-smoke COMMAND av2heif-windows-smoke)
endif()
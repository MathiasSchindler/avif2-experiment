add_executable(av2heif-macho-linker
    newos/src/tools/linker.c
    newos/src/compiler/linker.c
    newos/src/compiler/linker_util.c
    newos/src/compiler/linker_elf.c
    newos/src/compiler/linker_object.c
    newos/src/compiler/linker_symbols.c
    newos/src/compiler/linker_gc.c
    newos/src/compiler/linker_merge.c
    newos/src/compiler/linker_icf.c
    newos/src/compiler/linker_reloc.c
    newos/src/compiler/linker_layout.c
    newos/src/compiler/linker_report.c
    newos/src/compiler/linker_lto.c
    newos/src/compiler/linker_macho.c
    newos/src/shared/crypto/sha256.c
    newos/src/shared/runtime/memory.c
    newos/src/shared/runtime/string.c
    newos/src/shared/runtime/parse.c
    newos/src/shared/runtime/io.c
    newos/src/shared/tool_cli.c
    newos/src/shared/tool_io.c
    newos/src/shared/tool_json.c
    newos/src/shared/tool_path.c
    newos/src/platform/posix/fs.c
    newos/src/platform/posix/process.c
    newos/src/platform/posix/identity.c
)
target_compile_features(av2heif-macho-linker PRIVATE c_std_11)
target_compile_definitions(av2heif-macho-linker PRIVATE
    COMPILER_LINKER_ENABLE_REPORTING=1)
target_include_directories(av2heif-macho-linker PRIVATE
    newos/src/compiler
    newos/src/shared
    newos/src/platform/posix
    newos/src/platform/common
)
target_compile_options(av2heif-macho-linker PRIVATE
    -Wall -Wextra -Wpedantic -Werror -O2
    -ffunction-sections -fdata-sections -fno-lto)
target_link_options(av2heif-macho-linker PRIVATE -fno-lto -Wl,-dead_strip)

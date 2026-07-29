cmake_minimum_required(VERSION 3.20)

project(av2-census LANGUAGES C)

get_filename_component(REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.."
                       ABSOLUTE)
set(AVM_SOURCE_DIR "${REPOSITORY_ROOT}/vendor/avm" CACHE PATH
    "Pinned AVM source directory")
set(AVM_BUILD_DIR "${REPOSITORY_ROOT}/build/reference/avm" CACHE PATH
    "Pinned AVM build directory")

if(NOT EXISTS "${AVM_SOURCE_DIR}/avm/avm_decoder.h")
    message(FATAL_ERROR "AVM source not found at ${AVM_SOURCE_DIR}")
endif()
if(NOT EXISTS "${AVM_BUILD_DIR}/config/avm_config.h" OR
   NOT EXISTS "${AVM_BUILD_DIR}/libavm.a")
    message(FATAL_ERROR
        "AVM build not found at ${AVM_BUILD_DIR}; run scripts/setup-avm first")
endif()

add_executable(av2-census census.c)
target_compile_features(av2-census PRIVATE c_std_11)
target_include_directories(av2-census SYSTEM PRIVATE
    "${AVM_BUILD_DIR}"
    "${AVM_SOURCE_DIR}"
)
target_link_libraries(av2-census PRIVATE "${AVM_BUILD_DIR}/libavm.a")

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(av2-census PRIVATE
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow
    )
endif()

if(WIN32)
    target_compile_definitions(av2-census PRIVATE
        _WIN32_WINNT=0x0601 NOMINMAX
    )
    target_link_libraries(av2-census PRIVATE pthread)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_link_options(av2-census PRIVATE -static)
    endif()
else()
    find_package(Threads REQUIRED)
    target_link_libraries(av2-census PRIVATE Threads::Threads m)
endif()

enable_testing()
add_test(NAME census-emerald
    COMMAND av2-census
        "${REPOSITORY_ROOT}/images/artifacts/emerald-192x96-qp208.obu")

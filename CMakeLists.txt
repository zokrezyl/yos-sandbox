cmake_minimum_required(VERSION 3.20)
project(yos-sandbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CPM package manager
include(cmake/CPM.cmake)

# wasm3 interpreter
CPMAddPackage(
    NAME wasm3
    GITHUB_REPOSITORY wasm3/wasm3
    GIT_TAG v0.5.0
    DOWNLOAD_ONLY YES
)

if(wasm3_ADDED)
    file(GLOB WASM3_SOURCES "${wasm3_SOURCE_DIR}/source/*.c")
    add_library(wasm3 STATIC ${WASM3_SOURCES})
    target_include_directories(wasm3 PUBLIC "${wasm3_SOURCE_DIR}/source")
    target_compile_definitions(wasm3 PUBLIC d_m3HasWASI)
endif()

# wasi-sdk toolchain for compiling C to wasm32
set(WASI_SDK_VERSION "32")
set(WASI_SDK_DIR "${CMAKE_BINARY_DIR}/_deps/wasi-sdk")
if(NOT EXISTS "${WASI_SDK_DIR}/bin/clang")
    message(STATUS "Downloading wasi-sdk ${WASI_SDK_VERSION}...")
    set(WASI_SDK_URL "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${WASI_SDK_VERSION}/wasi-sdk-${WASI_SDK_VERSION}.0-x86_64-linux.tar.gz")
    file(DOWNLOAD ${WASI_SDK_URL} "${CMAKE_BINARY_DIR}/_deps/wasi-sdk.tar.gz" SHOW_PROGRESS)
    file(ARCHIVE_EXTRACT INPUT "${CMAKE_BINARY_DIR}/_deps/wasi-sdk.tar.gz" DESTINATION "${CMAKE_BINARY_DIR}/_deps")
    file(RENAME "${CMAKE_BINARY_DIR}/_deps/wasi-sdk-${WASI_SDK_VERSION}.0-x86_64-linux" "${WASI_SDK_DIR}")
endif()
set(WASI_CC "${WASI_SDK_DIR}/bin/clang")
set(WASI_SYSROOT "${WASI_SDK_DIR}/share/wasi-sysroot")

# busybox source
CPMAddPackage(
    NAME busybox
    GITHUB_REPOSITORY mirror/busybox
    GIT_TAG 1_36_1
    DOWNLOAD_ONLY YES
)

# YOS runtime (host)
add_executable(yos
    src/main.cpp
    src/process.cpp
    src/runtime.cpp
    src/syscalls.cpp
)
target_include_directories(yos PRIVATE src)
target_link_libraries(yos PRIVATE wasm3 pthread)

# Test WASM modules: compile .wat -> .wasm via wat2wasm
find_program(WAT2WASM wat2wasm REQUIRED)

file(GLOB WAT_SOURCES "${CMAKE_SOURCE_DIR}/test/*.wat")
foreach(wat_file ${WAT_SOURCES})
    get_filename_component(name ${wat_file} NAME_WE)
    set(wasm_out "${CMAKE_BINARY_DIR}/wasm/${name}.wasm")
    add_custom_command(
        OUTPUT ${wasm_out}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/wasm"
        COMMAND ${WAT2WASM} ${wat_file} -o ${wasm_out}
        DEPENDS ${wat_file}
        COMMENT "wat2wasm: ${name}.wat -> ${name}.wasm"
    )
    list(APPEND WASM_OUTPUTS ${wasm_out})
endforeach()

add_custom_target(wasm_tests ALL DEPENDS ${WASM_OUTPUTS})

# Busybox WASM build
# We compile a minimal busybox (just sh + basic applets) to wasm32-wasi
# with our yos_* syscall stubs for fork/exec
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/wasm/shell.wasm"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/wasm"
    COMMAND ${CMAKE_COMMAND}
        -DWASI_CC=${WASI_CC}
        -DWASI_SYSROOT=${WASI_SYSROOT}
        -DBUSYBOX_SOURCE_DIR=${busybox_SOURCE_DIR}
        -DYOS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DOUTPUT_DIR=${CMAKE_BINARY_DIR}/wasm
        -P ${CMAKE_SOURCE_DIR}/cmake/build-busybox.cmake
    DEPENDS
        ${CMAKE_SOURCE_DIR}/cmake/build-busybox.cmake
        ${CMAKE_SOURCE_DIR}/wasm-stubs/yos-stubs.c
        ${CMAKE_SOURCE_DIR}/wasm-stubs/yos-stubs.h
        ${CMAKE_SOURCE_DIR}/wasm-stubs/wasm-compat.h
        ${CMAKE_SOURCE_DIR}/wasm-programs/shell.c
        ${CMAKE_SOURCE_DIR}/busybox.config
    COMMENT "Building wasm programs"
)

add_custom_target(wasm_programs ALL DEPENDS "${CMAKE_BINARY_DIR}/wasm/shell.wasm")

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
    target_compile_definitions(wasm3 PUBLIC
        d_m3HasWASI
        d_m3MaxFunctionStackHeight=4000
        d_m3MaxConstantTableSize=256
        d_m3RecordBacktraces=1
    )
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

# Generate syscall glue code
set(CODEGEN_DIR "${CMAKE_BINARY_DIR}/generated")
set(SYSCALLS_YAML "${CMAKE_SOURCE_DIR}/src/codegen/syscalls.yaml")
set(CODEGEN_SCRIPT "${CMAKE_SOURCE_DIR}/src/codegen/gen-syscalls.py")
set(GENERATED_HANDLERS "${CODEGEN_DIR}/src/yos-handlers-generated.cpp")
set(GENERATED_LINK "${CODEGEN_DIR}/src/yos-link-generated.cpp")
set(GENERATED_WASM_H "${CODEGEN_DIR}/wasm-stubs/yos-generated.h")
set(GENERATED_WASM_C "${CODEGEN_DIR}/wasm-stubs/yos-generated.c")

find_program(UV_TOOL uv REQUIRED)

add_custom_command(
    OUTPUT ${GENERATED_HANDLERS} ${GENERATED_LINK} ${GENERATED_WASM_H} ${GENERATED_WASM_C}
    COMMAND ${UV_TOOL} run --with pyyaml python3 ${CODEGEN_SCRIPT} ${SYSCALLS_YAML} ${CODEGEN_DIR}
    DEPENDS ${SYSCALLS_YAML} ${CODEGEN_SCRIPT}
    COMMENT "Generating syscall glue code"
)

add_custom_target(generate_syscalls DEPENDS ${GENERATED_HANDLERS} ${GENERATED_LINK})

# Generate libc wrappers from Linux headers
set(LIBC_YAML "${CODEGEN_DIR}/libc.yaml")
set(LIBC_WRAPPERS_C "${CODEGEN_DIR}/libc-wrappers.c")
set(LIBC_WRAPPERS_H "${CODEGEN_DIR}/libc-wrappers.h")
set(EXTRACT_SCRIPT "${CMAKE_SOURCE_DIR}/tools/extract-signatures.py")
set(GENERATE_LIBC_SCRIPT "${CMAKE_SOURCE_DIR}/tools/generate-libc-calls.py")

# Step 1: Extract function signatures from headers -> libc.yaml
add_custom_command(
    OUTPUT ${LIBC_YAML}
    COMMAND ${UV_TOOL} run python3 ${EXTRACT_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${EXTRACT_SCRIPT}
    COMMENT "Extracting libc function signatures"
)

# Step 2: Generate wrappers and smoke tests from libc.yaml
add_custom_command(
    OUTPUT ${LIBC_WRAPPERS_C} ${LIBC_WRAPPERS_H}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CODEGEN_DIR}/smoke-tests
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CODEGEN_DIR}/smoke-tests
    COMMAND ${UV_TOOL} run python3 ${GENERATE_LIBC_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${LIBC_YAML} ${GENERATE_LIBC_SCRIPT}
    COMMENT "Generating libc wrappers and smoke tests"
)

add_custom_target(generate_libc DEPENDS ${LIBC_WRAPPERS_C} ${LIBC_WRAPPERS_H})

# Smoke test runner for libc wrappers - use system gcc to avoid nix
set(SMOKE_TEST_DIR "${CODEGEN_DIR}/smoke-tests")
enable_language(ASM)

# Build smoke_runner with system compiler (not nix) to get full libc
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/smoke_runner
    COMMAND /usr/bin/gcc -std=c11 -O2
        -I${wasm3_SOURCE_DIR}/source
        -I${CMAKE_SOURCE_DIR}/src
        -o ${CMAKE_BINARY_DIR}/smoke_runner
        ${CMAKE_SOURCE_DIR}/tools/smoke-runner.c
        ${LIBC_WRAPPERS_C}
        ${CMAKE_SOURCE_DIR}/src/varargs_trampoline.S
        $<TARGET_FILE:wasm3>
        -lm -lpthread -ldl -lcrypt -lresolv
    DEPENDS generate_libc wasm3
        ${CMAKE_SOURCE_DIR}/tools/smoke-runner.c
        ${CMAKE_SOURCE_DIR}/src/varargs_trampoline.S
    COMMENT "Building smoke test runner with system gcc"
)
add_custom_target(smoke_runner DEPENDS ${CMAKE_BINARY_DIR}/smoke_runner)

# Custom target to build all smoke test .wasm files and run tests
add_custom_target(smoke_tests
    COMMAND ${CMAKE_COMMAND}
        -DSMOKE_TEST_DIR=${SMOKE_TEST_DIR}
        -DRUNNER=${CMAKE_BINARY_DIR}/smoke_runner
        -P ${CMAKE_SOURCE_DIR}/cmake/run-smoke-tests.cmake
    DEPENDS smoke_runner generate_libc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running smoke tests"
)

# YOS runtime (host)
add_executable(yos
    src/main.cpp
    src/process.cpp
    src/procfs.cpp
    src/runtime.cpp
    src/syscalls.cpp
    src/vfs.cpp
    src/hostfs.cpp
)
add_dependencies(yos generate_syscalls)
target_include_directories(yos PRIVATE src ${wasm3_SOURCE_DIR}/source ${CODEGEN_DIR}/src)
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
# Compile busybox to wasm32 with -nostdlib using only our generated YOS stubs
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/wasm/shell.wasm"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/wasm"
    COMMAND ${CMAKE_COMMAND}
        -DWASI_CC=${WASI_CC}
        -DBUSYBOX_SOURCE_DIR=${busybox_SOURCE_DIR}
        -DYOS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DCODEGEN_DIR=${CODEGEN_DIR}
        -DOUTPUT_DIR=${CMAKE_BINARY_DIR}/wasm
        -P ${CMAKE_SOURCE_DIR}/cmake/build-busybox.cmake
    DEPENDS
        ${CMAKE_SOURCE_DIR}/cmake/build-busybox.cmake
        ${CMAKE_SOURCE_DIR}/wasm-stubs/yos-stubs.c
        ${CMAKE_SOURCE_DIR}/wasm-stubs/yos-stubs.h
        ${CMAKE_SOURCE_DIR}/wasm-stubs/wasm-compat.h
        ${CODEGEN_DIR}/wasm-stubs/yos-generated.c
        ${CODEGEN_DIR}/wasm-stubs/yos-generated.h
        generate_syscalls
    COMMENT "Building wasm programs"
)

add_custom_target(wasm_programs ALL DEPENDS "${CMAKE_BINARY_DIR}/wasm/shell.wasm")

# Unit tests
enable_testing()
add_subdirectory(tests/unit)

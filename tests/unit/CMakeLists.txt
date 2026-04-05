# Unit tests for YOS runtime
# Each test is a minimal WASM program testing specific functionality

cmake_minimum_required(VERSION 3.16)

# Find clang for WASM compilation
find_program(CLANG_WASM clang HINTS /usr/bin /usr/local/bin)
find_program(WASM_LD wasm-ld HINTS /usr/bin /usr/local/bin)

if(NOT CLANG_WASM OR NOT WASM_LD)
    message(WARNING "clang or wasm-ld not found, skipping unit tests")
    return()
endif()

set(WASM_TEST_OUTPUT_DIR ${CMAKE_BINARY_DIR}/tests/unit/wasm)
file(MAKE_DIRECTORY ${WASM_TEST_OUTPUT_DIR})

# Common WASM compile flags
set(WASM_CFLAGS
    --target=wasm32-unknown-unknown
    -nostdlib
    -ffreestanding
    -O2
    -Wall
)

set(WASM_LDFLAGS
    --no-entry
    --export=_start
    --allow-undefined
    -z stack-size=65536
)

# List of test source files
set(UNIT_TESTS
    test_write_exit
    test_sbrk
    test_sbrk_bounds
    test_open_read_close
    test_stat
    test_stat_offsets
    test_lstat
    test_opendir
    test_dirent_offsets
    test_printf
    test_getpid
    test_getdents
    test_getcwd
    test_isatty
    test_bump_malloc
    test_memory_layout
    test_heap_ptr_init
)

# Build each test as a separate WASM binary
foreach(TEST_NAME ${UNIT_TESTS})
    set(TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/wasm-src/${TEST_NAME}.c)
    set(TEST_OBJ ${WASM_TEST_OUTPUT_DIR}/${TEST_NAME}.o)
    set(TEST_WASM ${WASM_TEST_OUTPUT_DIR}/${TEST_NAME}.wasm)

    # Compile C to object
    add_custom_command(
        OUTPUT ${TEST_OBJ}
        COMMAND ${CLANG_WASM} ${WASM_CFLAGS} -c ${TEST_SRC} -o ${TEST_OBJ}
        DEPENDS ${TEST_SRC}
        COMMENT "Compiling ${TEST_NAME}.c to WASM object"
    )

    # Link to WASM
    add_custom_command(
        OUTPUT ${TEST_WASM}
        COMMAND ${WASM_LD} ${WASM_LDFLAGS} ${TEST_OBJ} -o ${TEST_WASM}
        DEPENDS ${TEST_OBJ}
        COMMENT "Linking ${TEST_NAME}.wasm"
    )

    list(APPEND WASM_TEST_TARGETS ${TEST_WASM})
endforeach()

# Target to build all unit test WASM files
add_custom_target(unit_test_wasm ALL DEPENDS ${WASM_TEST_TARGETS})

# Test runner executable
add_executable(run_unit_tests
    run_tests.cpp
)

target_link_libraries(run_unit_tests PRIVATE wasm3 m)
target_include_directories(run_unit_tests PRIVATE
    ${wasm3_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/src
    ${CODEGEN_DIR}
)

# Make test runner depend on WASM files
add_dependencies(run_unit_tests unit_test_wasm generate_libc)

# CTest integration (enable_testing() called from top-level CMakeLists.txt)
foreach(TEST_NAME ${UNIT_TESTS})
    add_test(
        NAME ${TEST_NAME}
        COMMAND run_unit_tests ${WASM_TEST_OUTPUT_DIR}/${TEST_NAME}.wasm
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endforeach()

# Native (host) unit tests - test components that run on the host, not in WASM
# Enable ASM language for assembly files
enable_language(ASM)

# Varargs trampoline test - validates the assembly trampoline for calling native functions
add_executable(test_varargs_trampoline
    test_varargs_trampoline.c
    ${CMAKE_SOURCE_DIR}/src/varargs_trampoline.S
)

target_compile_options(test_varargs_trampoline PRIVATE -std=c11 -O2 -Wall)

add_test(
    NAME varargs_trampoline
    COMMAND test_varargs_trampoline
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# YOS VFS unit test
add_executable(test_yos_vfs
    test_yos_vfs.c
    ${CMAKE_SOURCE_DIR}/src/yos-vfs.c
    ${CMAKE_SOURCE_DIR}/src/yos-log.c
)

target_include_directories(test_yos_vfs PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_options(test_yos_vfs PRIVATE -std=c11 -O2 -Wall)
target_link_libraries(test_yos_vfs PRIVATE pthread)

add_test(
    NAME yos_vfs
    COMMAND test_yos_vfs
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# YOS Process unit test
add_executable(test_yos_process
    test_yos_process.c
    ${CMAKE_SOURCE_DIR}/src/yos-process.c
    ${CMAKE_SOURCE_DIR}/src/yos-exec-ctx.c
    ${CMAKE_SOURCE_DIR}/src/yos-vfs.c
    ${CMAKE_SOURCE_DIR}/src/yos-runtime.c
    ${CMAKE_SOURCE_DIR}/src/yos-log.c
    ${CMAKE_SOURCE_DIR}/src/varargs_trampoline.S
    ${LIBC_WRAPPERS_C}
)

add_dependencies(test_yos_process generate_libc)
target_include_directories(test_yos_process PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${wasm3_SOURCE_DIR}/source
    ${CODEGEN_DIR}
)

target_compile_options(test_yos_process PRIVATE -std=c11 -O2 -Wall)
target_link_libraries(test_yos_process PRIVATE wasm3 pthread m)

add_test(
    NAME yos_process
    COMMAND test_yos_process
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# YOS Exec Context unit test
add_executable(test_yos_exec_ctx
    test_yos_exec_ctx.c
    ${CMAKE_SOURCE_DIR}/src/yos-exec-ctx.c
    ${CMAKE_SOURCE_DIR}/src/yos-process.c
    ${CMAKE_SOURCE_DIR}/src/yos-vfs.c
    ${CMAKE_SOURCE_DIR}/src/yos-runtime.c
    ${CMAKE_SOURCE_DIR}/src/yos-log.c
    ${CMAKE_SOURCE_DIR}/src/varargs_trampoline.S
    ${LIBC_WRAPPERS_C}
)

add_dependencies(test_yos_exec_ctx generate_libc)
target_include_directories(test_yos_exec_ctx PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${wasm3_SOURCE_DIR}/source
    ${CODEGEN_DIR}
)

target_compile_options(test_yos_exec_ctx PRIVATE -std=c11 -O2 -Wall)
target_link_libraries(test_yos_exec_ctx PRIVATE wasm3 pthread m)

add_test(
    NAME yos_exec_ctx
    COMMAND test_yos_exec_ctx
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

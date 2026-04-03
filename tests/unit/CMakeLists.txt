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

target_link_libraries(run_unit_tests PRIVATE wasm3)
target_include_directories(run_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/external/wasm3/source
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

# Make test runner depend on WASM files
add_dependencies(run_unit_tests unit_test_wasm generate_syscalls)

# CTest integration
enable_testing()

foreach(TEST_NAME ${UNIT_TESTS})
    add_test(
        NAME ${TEST_NAME}
        COMMAND run_unit_tests ${WASM_TEST_OUTPUT_DIR}/${TEST_NAME}.wasm
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endforeach()

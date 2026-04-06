# Integration tests - WASM programs run through actual yos runtime
# These test the full stack: WASM -> wasm3 -> libc-wrappers -> yos_* functions

cmake_minimum_required(VERSION 3.16)

find_program(CLANG_WASM clang HINTS /usr/bin /usr/local/bin)
find_program(WASM_LD wasm-ld HINTS /usr/bin /usr/local/bin)

if(NOT CLANG_WASM OR NOT WASM_LD)
    message(WARNING "clang or wasm-ld not found, skipping integration tests")
    return()
endif()

set(INTEGRATION_WASM_DIR ${CMAKE_BINARY_DIR}/tests/integration/wasm)
file(MAKE_DIRECTORY ${INTEGRATION_WASM_DIR})

# Compile flags for integration tests (uses "env" module like real programs)
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
    --export=__heap_base
    --allow-undefined
    --stack-first
    -z stack-size=65536
)

# Integration test sources
set(INTEGRATION_TESTS
    test_basic
    test_file_io
    test_strings
)

# Build each integration test
foreach(TEST_NAME ${INTEGRATION_TESTS})
    set(TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.c)
    set(TEST_OBJ ${INTEGRATION_WASM_DIR}/${TEST_NAME}.o)
    set(TEST_WASM ${INTEGRATION_WASM_DIR}/${TEST_NAME}.wasm)

    add_custom_command(
        OUTPUT ${TEST_OBJ}
        COMMAND ${CLANG_WASM} ${WASM_CFLAGS} -c ${TEST_SRC} -o ${TEST_OBJ}
        DEPENDS ${TEST_SRC}
        COMMENT "Compiling integration test: ${TEST_NAME}.c"
    )

    add_custom_command(
        OUTPUT ${TEST_WASM}
        COMMAND ${WASM_LD} ${WASM_LDFLAGS} ${TEST_OBJ} -o ${TEST_WASM}
        DEPENDS ${TEST_OBJ}
        COMMENT "Linking integration test: ${TEST_NAME}.wasm"
    )

    list(APPEND INTEGRATION_WASM_TARGETS ${TEST_WASM})

    # Add ctest that runs through yos
    add_test(
        NAME integration_${TEST_NAME}
        COMMAND $<TARGET_FILE:yos> ${TEST_WASM}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endforeach()

add_custom_target(integration_tests ALL DEPENDS ${INTEGRATION_WASM_TARGETS})
add_dependencies(integration_tests yos)

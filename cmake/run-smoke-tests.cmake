# CMake script to compile and run smoke tests
# Variables: SMOKE_TEST_DIR, RUNNER

find_program(CLANG clang REQUIRED)

# Find all test_*.c files
file(GLOB TEST_SOURCES "${SMOKE_TEST_DIR}/test_*.c")

set(passed 0)
set(failed 0)
set(failed_tests "")

foreach(src ${TEST_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    set(wasm "${SMOKE_TEST_DIR}/${name}.wasm")

    # Compile to WASM
    execute_process(
        COMMAND ${CLANG} --target=wasm32 -nostdlib
            -Wl,--no-entry -Wl,--export=_start -Wl,--allow-undefined
            -o ${wasm} ${src}
        RESULT_VARIABLE compile_result
        ERROR_VARIABLE compile_error
        OUTPUT_QUIET
    )

    if(NOT compile_result EQUAL 0)
        math(EXPR failed "${failed} + 1")
        list(APPEND failed_tests "${name} (compile)")
        continue()
    endif()

    # Run test with timeout
    execute_process(
        COMMAND ${RUNNER} ${wasm}
        RESULT_VARIABLE run_result
        TIMEOUT 2
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(run_result EQUAL 0)
        math(EXPR passed "${passed} + 1")
    else()
        math(EXPR failed "${failed} + 1")
        list(APPEND failed_tests "${name}")
    endif()
endforeach()

message(STATUS "")
message(STATUS "Smoke test results: ${passed} passed, ${failed} failed")

if(failed GREATER 0)
    message(STATUS "Failed tests:")
    foreach(t ${failed_tests})
        message(STATUS "  - ${t}")
    endforeach()
endif()

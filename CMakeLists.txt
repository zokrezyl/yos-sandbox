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
endif()

# YOS runtime
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
    set(wasm_out "${CMAKE_BINARY_DIR}/test/${name}.wasm")
    add_custom_command(
        OUTPUT ${wasm_out}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/test"
        COMMAND ${WAT2WASM} ${wat_file} -o ${wasm_out}
        DEPENDS ${wat_file}
        COMMENT "wat2wasm: ${name}.wat -> ${name}.wasm"
    )
    list(APPEND WASM_OUTPUTS ${wasm_out})
endforeach()

add_custom_target(wasm_tests ALL DEPENDS ${WASM_OUTPUTS})

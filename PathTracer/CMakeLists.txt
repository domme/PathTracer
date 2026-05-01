cmake_minimum_required(VERSION 4.3)
project(PathTracer)

file(GLOB PATHTRACER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# Organize source files into filters based on directory structure
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${PATHTRACER_SOURCES})

add_executable(PathTracer WIN32 ${PATHTRACER_SOURCES})

target_include_directories(PathTracer
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${FANCY_EXTERNAL_DIR}"
)

target_link_libraries(PathTracer
    PRIVATE
        fancy_core
        fancy_imgui
)

target_precompile_headers(PathTracer REUSE_FROM fancy_core)
fancy_apply_compile_options(PathTracer)

set_target_properties(PathTracer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/win64/$<CONFIG>/PathTracer"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/win64/$<CONFIG>/PathTracer"
)

fancy_copy_runtime_dlls(PathTracer)

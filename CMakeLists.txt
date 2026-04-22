cmake_minimum_required(VERSION 4.3)

# Set vcpkg toolchain before the first project() call.
# CMakePresets.json handles this for preset-based builds; this fallback
# covers manual cmake invocations.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/FANCY/external/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain"
    )
endif()
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet")
endif()

project(PathTracerSolution CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(FANCY)
add_subdirectory(PathTracer)

# ---------------------------------------------------------------------------
# Top-level format target (formats all project sources via clang-format)
# ---------------------------------------------------------------------------
find_program(CLANG_FORMAT_EXE
    NAMES clang-format
    HINTS
        "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin"
        "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Professional/VC/Tools/Llvm/x64/bin"
        "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/x64/bin"
)

if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE _ALL_PROJECT_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/PathTracer/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/PathTracer/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/FANCY/fancy_core/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/FANCY/fancy_core/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/FANCY/fancy_imgui/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/FANCY/fancy_imgui/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/FANCY/Tests/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/FANCY/Tests/*.h"
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${_ALL_PROJECT_SOURCES}
        COMMENT "Running clang-format on all project sources"
        VERBATIM
    )
    unset(_ALL_PROJECT_SOURCES)
else()
    message(STATUS "clang-format not found — format target unavailable")
endif()


# =============================================================================
# stream-canvas benchmark harness  [INSTRUMENTATION — SAFE TO DELETE]
#
# Standalone CMake project. Reuses the engine submodule's own CMake/CPM setup
# (lua, sol2, nlohmann_json, stb, pangocairo) via add_subdirectory, then builds
# a single offscreen benchmark executable that links the engine static lib.
#
# It does NOT touch the OBS plugin build or CI: configure it out-of-tree.
#
#   cmake -B build-bench bench -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-bench -j
#   ./build-bench/stream-canvas-bench
#
# To strip entirely: delete the bench/ directory.
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(stream-canvas-bench CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Pull in the engine target (and its CPM dependencies). Defaults to the engine/
# submodule, but ENGINE_DIR can point at a working clone (e.g. the canonical
# ~/Projects/obs-graphics-engine) to validate a change BEFORE it is pushed.
set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../engine" CACHE PATH "Path to engine sources")
add_subdirectory(${ENGINE_DIR} ${CMAKE_BINARY_DIR}/engine-build)

add_executable(stream-canvas-bench bench.cpp)
target_link_libraries(stream-canvas-bench PRIVATE engine)
# Include engine headers directly (works whether ENGINE_DIR is the `engine`
# submodule or a differently-named working clone like obs-graphics-engine).
target_include_directories(stream-canvas-bench PRIVATE ${ENGINE_DIR})

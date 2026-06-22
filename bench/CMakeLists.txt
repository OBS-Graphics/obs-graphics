# =============================================================================
# obs-graphics benchmark harness  [INSTRUMENTATION — SAFE TO DELETE]
#
# Standalone CMake project. Reuses the engine submodule's own CMake/CPM setup
# (lua, sol2, nlohmann_json, stb, pangocairo) via add_subdirectory, then builds
# a single offscreen benchmark executable that links the engine static lib.
#
# It does NOT touch the OBS plugin build or CI: configure it out-of-tree.
#
#   cmake -B build-bench bench -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-bench -j
#   ./build-bench/obs-graphics-bench
#
# To strip entirely: delete the bench/ directory.
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(obs-graphics-bench CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Pull in the engine target (and its CPM dependencies) from the submodule.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../engine ${CMAKE_BINARY_DIR}/engine-build)

add_executable(obs-graphics-bench bench.cpp)
target_link_libraries(obs-graphics-bench PRIVATE engine)

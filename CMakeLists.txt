cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --match "[0-9]*.[0-9]*.[0-9]*" --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE _git_tag_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  string(REGEX REPLACE "^v" "" _git_tag_version "${_git_tag_version}")
  string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" _git_tag_version "${_git_tag_version}")
endif()
if(NOT _git_tag_version)
  set(_git_tag_version "1.0.0")
endif()

project(stream-canvas VERSION ${_git_tag_version})

# Headless unit tests for the plugin logic that is pure/host-free (AppConfig
# JSON load/save): no OBS, no Qt. Default OFF so it never affects the normal
# plugin build; see the gated block at the bottom of this file.
#
# Deliberately NOT named BUILD_TESTS: engine/CMakeLists.txt already declares its
# own option by that name, and CMake options are process-wide cache variables, so
# a shared name would make -D<opt>=ON here silently also build the engine's suite
# (pulling in SDL3/curl/OpenSSL). The two are independent — set both if you want
# both.
option(PLUGIN_BUILD_TESTS "Build headless plugin unit tests (no OBS, no Qt)" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

# Must come after include(defaults) so Qt6 is in CMAKE_PREFIX_PATH
# (on macOS/Windows, defaults downloads the prebuilt OBS deps that contain Qt6)
CPMAddPackage(
    NAME qlementine-icons
    GITHUB_REPOSITORY oclero/qlementine-icons
    GIT_TAG master
    EXCLUDE_FROM_ALL TRUE
)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# plugin-support.c is compiled as its own standalone translation unit (see
# cmake/common/helpers_common.cmake) and plugin-support.h now includes OBS's
# own <util/base.h> for blogva's declaration (rather than hand-declaring it,
# which used to conflict with util/base.h's own declaration under MSVC —
# error C2375, different linkage) — so it needs OBS's include directories too.
if(TARGET plugin-support)
  target_link_libraries(plugin-support PUBLIC OBS::libobs)
endif()

find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

find_package(Qt6 COMPONENTS Widgets Core Svg Concurrent REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Svg Qt6::Concurrent qlementine-icons)
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
          $<$<CXX_COMPILER_ID:MSVC>:/wd4100>
          $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>
)
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
)

add_subdirectory(engine)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_20)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    src/plugin-main.cpp
    src/shared-title.cpp
    src/app-config.cpp
    src/app-settings.cpp
    src/ui-util.h
    src/ui-util.cpp
    src/settings-dialog.cpp
    src/title-settings-dialog.cpp
    src/manual-source-dialog.h
    src/manual-source-dialog.cpp
    src/graphics-dock.cpp
    src/graphics-source.cpp
    src/icons.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE engine)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME})

# ── Headless unit tests (no OBS, no Qt) ─────────────────────────────────────
# Only src/app-config.cpp (JSON config load/save) is pure/host-free;
# everything else in src/ needs a live OBS host or a Qt event loop. Mirrors
# engine/tests/: plain assert-style console binaries whose main() returns 0
# on success / 1 on failure, registered with ctest.
# `nlohmann_json` already exists as an INTERFACE target from
# add_subdirectory(engine) above regardless of the engine's own BUILD_TESTS --
# reused here rather than fetched a second time.
if(PLUGIN_BUILD_TESTS)
    enable_testing()

    # engine/CMakeLists.txt already CPMAddPackage()s nlohmann_json, but as an
    # IMPORTED (non-GLOBAL) target it's only visible inside that subdirectory
    # scope, not here. Re-invoking CPMAddPackage with the same NAME/URL does
    # not re-download (CPM caches by name in a CACHE INTERNAL var) -- it just
    # hands back the already-populated source dir, which is wrapped in a
    # target scoped to this directory instead.
    CPMAddPackage(
        NAME nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip
        DOWNLOAD_ONLY YES
    )
    add_library(plugin_test_json INTERFACE)
    target_include_directories(plugin_test_json INTERFACE ${nlohmann_json_SOURCE_DIR}/include)

    # app-config.cpp now depends on uuid::GenerateV4() (engine/uuid.h); pull
    # in engine/uuid.cpp directly rather than the full `engine` target, since
    # this test lib deliberately stays OBS/Qt-free. uuid.cpp is verified
    # stdlib-only (<array>, <cstdint>, <random>), so it doesn't drag in any
    # of engine's own heavier dependencies (Lua, curl, etc).
    add_library(plugin_testable STATIC
        src/app-config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/uuid.cpp
    )
    target_include_directories(plugin_testable PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(plugin_testable PUBLIC plugin_test_json)
    target_compile_features(plugin_testable PUBLIC cxx_std_20)

    function(add_plugin_test name)
        add_executable(${name} tests/${name}.cpp)
        target_link_libraries(${name} PRIVATE plugin_testable)
        target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    add_plugin_test(test_app_config)
endif()


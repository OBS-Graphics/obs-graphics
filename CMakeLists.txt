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

project(obs-graphics VERSION ${_git_tag_version})

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

find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

find_package(Qt6 COMPONENTS Widgets Core Svg REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Svg qlementine-icons)
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
    src/graphics-dock.cpp
    src/graphics-source.cpp
    src/icons.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE engine)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME})


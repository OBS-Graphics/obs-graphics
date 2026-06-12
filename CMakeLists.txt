cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

project(obs-graphics VERSION 1.0.0)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

find_package(Qt6 COMPONENTS Widgets Core)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
)
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
)

add_subdirectory(engine)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_20)

find_package(Qt6 COMPONENTS Widgets Core REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
)
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    src/plugin-main.cpp
    src/shared-scene.cpp
    src/app-config.cpp
    src/graphics-dock.cpp
    src/graphics-source.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE engine)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME})


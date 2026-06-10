cmake_minimum_required(VERSION 3.16)
project(engine)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/CPM.cmake)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PANGOCAIRO REQUIRED pangocairo)
pkg_check_modules(CAIRO REQUIRED cairo)

find_package(Lua REQUIRED)

CPMAddPackage(
    NAME sol2
    GITHUB_REPOSITORY ThePhD/sol2
    GIT_TAG v3.3.0
    OPTIONS "SOL2_ENABLE_INSTALL OFF"
)

add_library(engine STATIC
    element.cpp
    animation.cpp
    graphic.cpp
    scene.cpp
    data-source.cpp
    script.cpp
)

target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PANGOCAIRO_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIR}
    ${sol2_SOURCE_DIR}/include
)

target_link_libraries(engine PUBLIC
    ${PANGOCAIRO_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${LUA_LIBRARIES}
)

target_compile_options(engine PRIVATE
    ${PANGOCAIRO_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
    -fPIC
)

target_compile_definitions(engine PRIVATE SOL_NO_THREAD_LOCAL)

target_compile_features(engine PUBLIC cxx_std_20)

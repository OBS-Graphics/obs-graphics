cmake_minimum_required(VERSION 3.16)
project(engine)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PANGOCAIRO REQUIRED pangocairo)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(engine STATIC
    element.cpp
    animation.cpp
    graphic.cpp
    scene.cpp
    data-source.cpp
)

target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PANGOCAIRO_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)

target_link_libraries(engine PUBLIC
    ${PANGOCAIRO_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

target_compile_options(engine PRIVATE
    ${PANGOCAIRO_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)

target_compile_features(engine PUBLIC cxx_std_20)

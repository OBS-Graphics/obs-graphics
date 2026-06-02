cmake_minimum_required(VERSION 3.28)

project(obs-graphics-editor)

find_package(Qt6 COMPONENTS Core Widgets REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PANGOCAIRO REQUIRED pangocairo)
pkg_check_modules(CAIRO REQUIRED cairo)

add_executable(obs-graphics-editor
    main.cpp
    MainWindow.cpp
    model/SceneDocument.cpp
    model/EditorScene.cpp
    model/UndoCommands.cpp
    ui/CanvasWidget.cpp
    ui/SelectionHandles.cpp
    ui/SceneTreeModel.cpp
    ui/SceneTreeView.cpp
    ui/PropertiesPanel.cpp
    ui/properties/ScenePropertiesWidget.cpp
    ui/properties/GraphicPropertiesWidget.cpp
    ui/properties/ElementPropertiesWidget.cpp
    ui/properties/RectanglePropertiesWidget.cpp
    ui/properties/TextPropertiesWidget.cpp
    ui/properties/AnimationWidget.cpp
    ui/widgets/PaintEditorWidget.cpp
    ui/widgets/GradientStopListWidget.cpp
    ui/widgets/ColorPickerButton.cpp
    ui/widgets/CornerRadiusWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/element.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/animation.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/graphic.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/data-source.cpp
)

set_target_properties(obs-graphics-editor PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(obs-graphics-editor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PANGOCAIRO_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)

target_link_libraries(obs-graphics-editor PRIVATE
    Qt6::Core
    Qt6::Widgets
    ${PANGOCAIRO_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

target_compile_options(obs-graphics-editor PRIVATE
    ${PANGOCAIRO_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)

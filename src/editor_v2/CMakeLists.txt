cmake_minimum_required(VERSION 3.16)
project(editor_v2 VERSION 0.1 LANGUAGES C CXX)

include(cmake/CPM.cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../engine engine)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        model/EditorScene.h
        model/EditorScene.cpp
        model/SceneDocument.h
        model/SceneDocument.cpp
        model/UndoCommands.h
        model/UndoCommands.cpp
        ui/adaptivestack.h ui/adaptivestack.cpp
        ui/sceneproperties.h ui/sceneproperties.cpp
        ui/elementproperties.h ui/elementproperties.cpp
        ui/transformeditor.h ui/transformeditor.cpp
        ui/animationeditor.h ui/animationeditor.cpp
        ui/widgets/ColorUtils.h     ui/widgets/ColorUtils.cpp
        ui/widgets/HueSlider.h      ui/widgets/HueSlider.cpp
        ui/widgets/GradientSlider.h ui/widgets/GradientSlider.cpp
        ui/widgets/ColorPreview.h   ui/widgets/ColorPreview.cpp
        ui/widgets/ColorLineEdit.h  ui/widgets/ColorLineEdit.cpp
        ui/widgets/GradientEditor.h ui/widgets/GradientEditor.cpp
        ui/widgets/GradientHandlePainter.h
        ui/widgets/LinearGradientEditor.h ui/widgets/LinearGradientEditor.cpp
        ui/widgets/RadialGradientEditor.h ui/widgets/RadialGradientEditor.cpp
        ui/widgets/ColorWheel.h     ui/widgets/ColorWheel.cpp
        ui/widgets/ColorPicker.h    ui/widgets/ColorPicker.cpp
        ui/cornerradiuseditor.h ui/cornerradiuseditor.cpp
        icons.h icons.cpp
        ui/painteditor.h ui/painteditor.cpp
        ui/styleproperties.h ui/styleproperties.cpp
        ui/widgets/CanvasWidget.h ui/widgets/CanvasWidget.cpp
        ui/widgets/SceneTreeModel.h ui/widgets/SceneTreeModel.cpp
        ui/widgets/SceneTreeView.h ui/widgets/SceneTreeView.cpp
        ui/widgets/SelectionHandles.h ui/widgets/SelectionHandles.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(editor_v2
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET editor_v2 APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(editor_v2 SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(editor_v2
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# Get and Link QtAwesome
CPMAddPackage(
    NAME QtAwesome
    GITHUB_REPOSITORY gamecreature/QtAwesome
    GIT_TAG font-awesome-7.0.0
    DOWNLOAD_ONLY YES
)

# Patch out CPack inclusion — it only runs when QtAwesome is the top-level project
set(_qtawesome_cmake_patched)
file(READ "${QtAwesome_SOURCE_DIR}/CMakeLists.txt" _qtawesome_cmake)
string(REPLACE "include(CPack)" "# include(CPack)  # disabled by consumer" _qtawesome_cmake_patched "${_qtawesome_cmake}")
file(WRITE "${QtAwesome_SOURCE_DIR}/CMakeLists.txt" "${_qtawesome_cmake_patched}")

add_subdirectory(${QtAwesome_SOURCE_DIR} ${QtAwesome_BINARY_DIR})

target_include_directories(editor_v2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(editor_v2 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets engine QtAwesome)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.editor_v2)
endif()
set_target_properties(editor_v2 PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS editor_v2
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(editor_v2)
endif()

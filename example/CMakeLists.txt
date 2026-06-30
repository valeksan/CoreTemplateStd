cmake_minimum_required(VERSION 3.16)

project(CoreTemplateExample LANGUAGES CXX)

if(CORETEMPLATE_BUILD_EXAMPLE)
    add_executable(ExampleConsoleApp
        console_main.cpp
        ../core.h
    )

    target_include_directories(ExampleConsoleApp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_compile_features(ExampleConsoleApp PRIVATE cxx_std_17)

    if(TARGET CoreTemplateStdSanitizers)
        target_link_libraries(ExampleConsoleApp PRIVATE CoreTemplateStdSanitizers)
    endif()

    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../core.h")
        message(FATAL_ERROR "core.h not found at expected path: ${CMAKE_CURRENT_SOURCE_DIR}/../core.h")
    endif()
endif()

if(CORETEMPLATE_BUILD_QT_GUI_EXAMPLE)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

    add_executable(ExampleQtGuiApp
        qt_gui/main.cpp
        qt_gui/mainwindow.cpp
        qt_gui/mainwindow.h
        qt_gui/mainwindow.ui
    )

    target_link_libraries(ExampleQtGuiApp PRIVATE
        CoreTemplateStd::QtAdapter
        Qt${QT_VERSION_MAJOR}::Widgets
    )

    target_include_directories(ExampleQtGuiApp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/qt_gui
        ${CMAKE_CURRENT_SOURCE_DIR}/../qt_adapter
    )

    target_compile_features(ExampleQtGuiApp PRIVATE cxx_std_17)

    set_target_properties(ExampleQtGuiApp PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
    )
endif()

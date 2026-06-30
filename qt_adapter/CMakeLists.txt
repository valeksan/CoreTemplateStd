cmake_minimum_required(VERSION 3.16)

project(CoreTemplateQtAdapter LANGUAGES CXX)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_library(CoreTemplateQtAdapter STATIC
    CoreQtAdapter.cpp
    CoreQtAdapter.h
)

add_library(CoreTemplateStd::QtAdapter ALIAS CoreTemplateQtAdapter)

target_link_libraries(CoreTemplateQtAdapter PUBLIC
    CoreTemplateStd::CoreTemplateStd
    Qt${QT_VERSION_MAJOR}::Core
)

target_include_directories(CoreTemplateQtAdapter PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_compile_features(CoreTemplateQtAdapter PUBLIC cxx_std_17)

set_target_properties(CoreTemplateQtAdapter PROPERTIES
    AUTOMOC ON
)

if(CORETEMPLATE_BUILD_TESTS)
    add_executable(CoreTemplateQtAdapterSmoke
        smoke_main.cpp
    )

    target_link_libraries(CoreTemplateQtAdapterSmoke PRIVATE
        CoreTemplateStd::QtAdapter
    )

    target_compile_features(CoreTemplateQtAdapterSmoke PRIVATE cxx_std_17)

    add_test(NAME CoreTemplateQtAdapterSmoke COMMAND CoreTemplateQtAdapterSmoke)
endif()

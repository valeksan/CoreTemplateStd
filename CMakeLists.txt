cmake_minimum_required(VERSION 3.16)

project(CoreTemplateStd VERSION 0.4.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(CoreTemplateStd INTERFACE)
add_library(CoreTemplateStd::CoreTemplateStd ALIAS CoreTemplateStd)
add_library(CoreTemplate::CoreTemplate ALIAS CoreTemplateStd)

target_compile_features(CoreTemplateStd INTERFACE cxx_std_17)
target_include_directories(CoreTemplateStd INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/CoreTemplateStd>
)
target_sources(CoreTemplateStd INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core.h>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/CoreTemplateStd/core.h>
)

option(CORETEMPLATE_BUILD_EXAMPLE "Build the example application" ON)
option(CORETEMPLATE_BUILD_TESTS "Build the test suite" OFF)
option(CORETEMPLATE_BUILD_QT_ADAPTER "Build the optional Qt adapter target" OFF)
option(CORETEMPLATE_BUILD_QT_GUI_EXAMPLE "Build the optional Qt Widgets GUI example" OFF)
option(CORETEMPLATE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer for local test and example targets" OFF)

if(CORETEMPLATE_ENABLE_SANITIZERS)
    if(MSVC)
        message(WARNING "CORETEMPLATE_ENABLE_SANITIZERS is not configured for MSVC")
    else()
        add_library(CoreTemplateStdSanitizers INTERFACE)
        target_compile_options(CoreTemplateStdSanitizers INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(CoreTemplateStdSanitizers INTERFACE
            -fsanitize=address,undefined
        )
    endif()
endif()

if(CORETEMPLATE_BUILD_TESTS)
    enable_testing()
endif()

if(CORETEMPLATE_BUILD_QT_ADAPTER OR CORETEMPLATE_BUILD_QT_GUI_EXAMPLE)
    add_subdirectory(qt_adapter)
endif()

if(CORETEMPLATE_BUILD_EXAMPLE OR CORETEMPLATE_BUILD_QT_GUI_EXAMPLE)
    add_subdirectory(example)
endif()

if(CORETEMPLATE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

install(FILES core.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CoreTemplateStd
)

install(TARGETS CoreTemplateStd
    EXPORT CoreTemplateStdTargets
)

install(EXPORT CoreTemplateStdTargets
    FILE CoreTemplateStdTargets.cmake
    NAMESPACE CoreTemplateStd::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CoreTemplateStd
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/CoreTemplateStdConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CoreTemplateStdConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CoreTemplateStdConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CoreTemplateStd
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CoreTemplateStdConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CoreTemplateStdConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CoreTemplateStd
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/CoreTemplateConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CoreTemplateConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CoreTemplateConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CoreTemplate
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CoreTemplateConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CoreTemplateConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CoreTemplate
)

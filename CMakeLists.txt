cmake_minimum_required(VERSION 3.16)

project(CoreTemplate VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(CoreTemplate INTERFACE)
add_library(CoreTemplate::CoreTemplate ALIAS CoreTemplate)

target_compile_features(CoreTemplate INTERFACE cxx_std_17)
target_include_directories(CoreTemplate INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/CoreTemplate>
)
target_sources(CoreTemplate INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core.h>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/CoreTemplate/core.h>
)

option(CORETEMPLATE_BUILD_EXAMPLE "Build the example application" OFF)
option(CORETEMPLATE_BUILD_TESTS "Build the test suite" OFF)

if(CORETEMPLATE_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

if(CORETEMPLATE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

install(FILES core.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CoreTemplate
)

install(TARGETS CoreTemplate
    EXPORT CoreTemplateTargets
)

install(EXPORT CoreTemplateTargets
    FILE CoreTemplateTargets.cmake
    NAMESPACE CoreTemplate::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CoreTemplate
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

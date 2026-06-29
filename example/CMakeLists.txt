cmake_minimum_required(VERSION 3.16)

project(CoreTemplateExample LANGUAGES CXX)

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

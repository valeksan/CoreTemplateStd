cmake_minimum_required(VERSION 3.16)

project(CoreTemplateTests LANGUAGES CXX)

enable_testing()

add_executable(CoreTemplateTests
    core_tests.cpp
    ../core.h
)

target_include_directories(CoreTemplateTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_compile_features(CoreTemplateTests PRIVATE cxx_std_17)

add_test(NAME CoreTemplateTests COMMAND CoreTemplateTests)

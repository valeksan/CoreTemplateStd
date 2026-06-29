cmake_minimum_required(VERSION 3.16)

project(CoreTemplateTests LANGUAGES CXX)

add_executable(CoreTemplateTests
    core_tests.cpp
    ../core.h
)

target_include_directories(CoreTemplateTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_compile_features(CoreTemplateTests PRIVATE cxx_std_17)

if(TARGET CoreTemplateStdSanitizers)
    target_link_libraries(CoreTemplateTests PRIVATE CoreTemplateStdSanitizers)
endif()

add_test(NAME CoreTemplateTests COMMAND CoreTemplateTests)

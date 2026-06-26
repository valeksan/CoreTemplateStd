cmake_minimum_required(VERSION 3.16)

project(CoreTemplatePackageSmoke LANGUAGES CXX)
enable_testing()

find_package(CoreTemplate REQUIRED)

add_executable(CoreTemplatePackageSmoke
    main.cpp
)

target_link_libraries(CoreTemplatePackageSmoke PRIVATE
    CoreTemplate::CoreTemplate
)

target_compile_features(CoreTemplatePackageSmoke PRIVATE cxx_std_17)

add_test(NAME CoreTemplatePackageSmoke COMMAND CoreTemplatePackageSmoke)

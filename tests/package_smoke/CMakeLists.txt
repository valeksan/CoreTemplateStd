cmake_minimum_required(VERSION 3.16)

project(CoreTemplateStdPackageSmoke LANGUAGES CXX)
enable_testing()

find_package(CoreTemplateStd REQUIRED)
find_package(CoreTemplate REQUIRED)

add_executable(CoreTemplateStdPackageSmoke
    main.cpp
)

target_link_libraries(CoreTemplateStdPackageSmoke PRIVATE
    CoreTemplateStd::CoreTemplateStd
)

target_compile_features(CoreTemplateStdPackageSmoke PRIVATE cxx_std_17)

add_executable(CoreTemplateCompatPackageSmoke
    main.cpp
)

target_link_libraries(CoreTemplateCompatPackageSmoke PRIVATE
    CoreTemplate::CoreTemplate
)

target_compile_features(CoreTemplateCompatPackageSmoke PRIVATE cxx_std_17)

add_test(NAME CoreTemplateStdPackageSmoke COMMAND CoreTemplateStdPackageSmoke)
add_test(NAME CoreTemplateCompatPackageSmoke COMMAND CoreTemplateCompatPackageSmoke)

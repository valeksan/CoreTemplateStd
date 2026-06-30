cmake_minimum_required(VERSION 3.16)

project(CoreTemplateStdPackageSmoke LANGUAGES CXX)
enable_testing()

option(CORETEMPLATE_PACKAGE_SMOKE_QT_ADAPTER "Build package smoke test for installed Qt adapter component" OFF)

find_package(CoreTemplateStd REQUIRED)
find_package(CoreTemplate REQUIRED)

add_executable(CoreTemplateStdPackageSmoke
    main.cpp
)

target_link_libraries(CoreTemplateStdPackageSmoke PRIVATE
    CoreTemplateStd::CoreTemplateStd
)

target_compile_features(CoreTemplateStdPackageSmoke PRIVATE cxx_std_17)

add_executable(CoreTemplateStdPrefixedIncludeSmoke
    prefixed_include_main.cpp
)

target_link_libraries(CoreTemplateStdPrefixedIncludeSmoke PRIVATE
    CoreTemplateStd::CoreTemplateStd
)

target_compile_features(CoreTemplateStdPrefixedIncludeSmoke PRIVATE cxx_std_17)

add_executable(CoreTemplateCompatPackageSmoke
    main.cpp
)

target_link_libraries(CoreTemplateCompatPackageSmoke PRIVATE
    CoreTemplate::CoreTemplate
)

target_compile_features(CoreTemplateCompatPackageSmoke PRIVATE cxx_std_17)

add_test(NAME CoreTemplateStdPackageSmoke COMMAND CoreTemplateStdPackageSmoke)
add_test(NAME CoreTemplateStdPrefixedIncludeSmoke COMMAND CoreTemplateStdPrefixedIncludeSmoke)
add_test(NAME CoreTemplateCompatPackageSmoke COMMAND CoreTemplateCompatPackageSmoke)

if(CORETEMPLATE_PACKAGE_SMOKE_QT_ADAPTER)
    find_package(CoreTemplateStd REQUIRED COMPONENTS QtAdapter)

    add_executable(CoreTemplateStdQtAdapterPackageSmoke
        qt_adapter_main.cpp
    )

    target_link_libraries(CoreTemplateStdQtAdapterPackageSmoke PRIVATE
        CoreTemplateStd::QtAdapter
    )

    target_compile_features(CoreTemplateStdQtAdapterPackageSmoke PRIVATE cxx_std_17)

    add_test(NAME CoreTemplateStdQtAdapterPackageSmoke COMMAND CoreTemplateStdQtAdapterPackageSmoke)
endif()

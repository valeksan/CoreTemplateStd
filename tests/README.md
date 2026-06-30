# CoreTemplateStd Tests

## Structure

- `core_tests.cpp` - std-only test executable for the `Core` API.
- `CMakeLists.txt` - CMake build file.
- `package_smoke/` - installed-package smoke test using `find_package(CoreTemplateStd)` and the compatibility `find_package(CoreTemplate)`.
- `package_smoke/qt_adapter_main.cpp` - installed-package smoke test for `find_package(CoreTemplateStd COMPONENTS QtAdapter)`.
- `CoreTemplateQtAdapterSmoke` - optional Qt adapter smoke test, enabled with `CORETEMPLATE_BUILD_QT_ADAPTER=ON`.

## Run with CMake

```bash
cmake -S .. -B ../build/std_only_tests -DCORETEMPLATE_BUILD_TESTS=ON -DCORETEMPLATE_BUILD_EXAMPLE=OFF
cmake --build ../build/std_only_tests --target CoreTemplateTests
ctest --test-dir ../build/std_only_tests --output-on-failure
```

Optional Qt adapter and GUI example build:

```bash
cmake -S .. -B ../build/qt_adapter_tests -DCORETEMPLATE_BUILD_TESTS=ON -DCORETEMPLATE_BUILD_EXAMPLE=OFF -DCORETEMPLATE_BUILD_QT_ADAPTER=ON -DCORETEMPLATE_BUILD_QT_GUI_EXAMPLE=ON
cmake --build ../build/qt_adapter_tests
ctest --test-dir ../build/qt_adapter_tests --output-on-failure
```

Installed Qt adapter package smoke:

```bash
cmake --install ../build/qt_adapter_tests --prefix ../build/qt_adapter_prefix
cmake -S package_smoke -B ../build/qt_adapter_package_smoke -DCMAKE_PREFIX_PATH=$PWD/../build/qt_adapter_prefix -DCORETEMPLATE_PACKAGE_SMOKE_QT_ADAPTER=ON
cmake --build ../build/qt_adapter_package_smoke
ctest --test-dir ../build/qt_adapter_package_smoke --output-on-failure
```

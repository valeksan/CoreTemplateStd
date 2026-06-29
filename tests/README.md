# CoreTemplateStd Tests

## Structure

- `core_tests.cpp` - std-only test executable for the `Core` API.
- `CMakeLists.txt` - CMake build file.
- `package_smoke/` - installed-package smoke test using `find_package(CoreTemplateStd)` and the compatibility `find_package(CoreTemplate)`.

## Run with CMake

```bash
cmake -S .. -B ../build/std_only_tests -DCORETEMPLATE_BUILD_TESTS=ON -DCORETEMPLATE_BUILD_EXAMPLE=OFF
cmake --build ../build/std_only_tests --target CoreTemplateTests
ctest --test-dir ../build/std_only_tests --output-on-failure
```

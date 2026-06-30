<!-- markdownlint-disable MD033 -->

# Contributing to CoreTemplateStd

<div align="center">

[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]

</div>

<div align="center">
  <sub>Thank you for considering contributing to CoreTemplateStd!</sub>
</div>

<br />

We welcome bug reports, fixes, documentation improvements, and focused feature proposals.

## How to Contribute

- **Report bugs:** open an issue and include the reproduction steps, expected behavior, actual behavior, OS, compiler, and CMake version.
- **Suggest improvements:** open an issue first for larger changes so the design can be discussed before implementation.
- **Submit code:** fork the repository, keep the change focused, add or update tests where useful, and open a pull request.

## Development Setup

### Prerequisites

- A C++ compiler supporting C++17 or later.
- CMake 3.16 or higher.
- A compatible build system such as Ninja, GNU Make, MSBuild, or an IDE that can open CMake projects.

Qt is not required for the core library, tests, or current console example.
Qt is required only when `CORETEMPLATE_BUILD_QT_ADAPTER=ON`.

### Clone

```bash
git clone https://github.com/valeksan/CoreTemplateStd.git
cd CoreTemplateStd
```

### Build and Test

```bash
cmake -S . -B build/dev -DCORETEMPLATE_BUILD_TESTS=ON -DCORETEMPLATE_BUILD_EXAMPLE=ON
cmake --build build/dev
ctest --test-dir build/dev --output-on-failure
./build/dev/example/ExampleConsoleApp
```

### Package Smoke Test

```bash
cmake -S . -B build/install_check -DCORETEMPLATE_BUILD_TESTS=OFF -DCORETEMPLATE_BUILD_EXAMPLE=OFF -DCMAKE_INSTALL_PREFIX=$PWD/build/install_prefix
cmake --build build/install_check --target install
cmake -S tests/package_smoke -B build/package_smoke -DCMAKE_PREFIX_PATH=$PWD/build/install_prefix
cmake --build build/package_smoke
ctest --test-dir build/package_smoke --output-on-failure
```

### Sanitizer Check

```bash
cmake -S . -B build/sanitizers -DCMAKE_BUILD_TYPE=Debug -DCORETEMPLATE_BUILD_TESTS=ON -DCORETEMPLATE_BUILD_EXAMPLE=ON -DCORETEMPLATE_ENABLE_SANITIZERS=ON
cmake --build build/sanitizers
ctest --test-dir build/sanitizers --output-on-failure
./build/sanitizers/example/ExampleConsoleApp
```

### Optional Qt Adapter Check

```bash
cmake -S . -B build/qt_adapter -DCORETEMPLATE_BUILD_TESTS=ON -DCORETEMPLATE_BUILD_EXAMPLE=OFF -DCORETEMPLATE_BUILD_QT_ADAPTER=ON -DCORETEMPLATE_BUILD_QT_GUI_EXAMPLE=ON
cmake --build build/qt_adapter
ctest --test-dir build/qt_adapter --output-on-failure
```

### Release Checklist

```bash
cmake -S . -B build/release_check -DCORETEMPLATE_BUILD_TESTS=ON -DCORETEMPLATE_BUILD_EXAMPLE=ON
cmake --build build/release_check
ctest --test-dir build/release_check --output-on-failure
./build/release_check/example/ExampleConsoleApp

cmake -S . -B build/release_install -DCORETEMPLATE_BUILD_TESTS=OFF -DCORETEMPLATE_BUILD_EXAMPLE=OFF -DCMAKE_INSTALL_PREFIX=$PWD/build/release_prefix
cmake --build build/release_install --target install
cmake -S tests/package_smoke -B build/release_package_smoke -DCMAKE_PREFIX_PATH=$PWD/build/release_prefix
cmake --build build/release_package_smoke
ctest --test-dir build/release_package_smoke --output-on-failure
```

Release tags use the `vMAJOR.MINOR.PATCH` format:

```bash
git tag -a v0.3.0 -F docs/releases/v0.3.0.md
git push std_origin v0.3.0
```

## Pull Request Guidelines

- **One PR, one topic:** keep unrelated changes separate.
- **Preserve the std-only core boundary:** `core.h`, the root package target, and core tests should not require Qt or other runtime dependencies.
- **Update documentation:** adjust README, tests documentation, or this file when behavior or workflows change.
- **Include tests:** cover new behavior and regression fixes where practical.
- **Verify locally:** run the focused CMake build and test commands before submitting.
- **Explain the change:** use a clear PR title and a short description of the behavior changed.

## Code Style

- Prefer standard C++17 facilities already used in the codebase.
- Keep `core.h` self-contained and header-only unless there is a strong reason to split implementation.
- Use `camelCase` for functions and variables.
- Use `PascalCase` for classes and structs.
- Use `UPPER_SNAKE_CASE` only for preprocessor constants when one is truly needed.
- Add comments for non-obvious design decisions, not for code that is already self-explanatory.
- Keep public API changes deliberate and documented because this is a header-only library.

## Questions

If something is unclear, open an issue or discussion in the repository.

<!-- MARKDOWN LINKS -->
[contributors-shield]: https://img.shields.io/github/contributors/valeksan/CoreTemplateStd.svg?style=for-the-badge
[contributors-url]: https://github.com/valeksan/CoreTemplateStd/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/valeksan/CoreTemplateStd.svg?style=for-the-badge
[forks-url]: https://github.com/valeksan/CoreTemplateStd/network/members
[stars-shield]: https://img.shields.io/github/stars/valeksan/CoreTemplateStd.svg?style=for-the-badge
[stars-url]: https://github.com/valeksan/CoreTemplateStd/stargazers
[issues-shield]: https://img.shields.io/github/issues/valeksan/CoreTemplateStd.svg?style=for-the-badge
[issues-url]: https://github.com/valeksan/CoreTemplateStd/issues

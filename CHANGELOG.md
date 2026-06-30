# Changelog

All notable changes to CoreTemplateStd are documented in this file.

## [0.5.1] - 2026-06-30

### Fixed

- `ExampleQtGuiApp` now auto-fills task ID/type/group fields from the selected active task.
- `Cancel By ID` in the GUI example now reports when the entered ID is not an active task ID.

## [0.5.0] - 2026-06-30

### Added

- Installed package support for the optional `CoreTemplateStd::QtAdapter` target.
- `find_package(CoreTemplateStd COMPONENTS QtAdapter)` for consumers that want the installed Qt adapter.
- Package smoke coverage for the installed Qt adapter component.

## [0.4.0] - 2026-06-30

### Added

- `Core::setWakeCallback` and `Core::clearWakeCallback` for event-loop integrations.

### Changed

- `CoreQtAdapter` now uses the wake callback with queued Qt delivery and one-shot timers.
- `ExampleQtGuiApp` no longer uses a polling `QTimer` to call `processEvents()`.

## [0.3.0] - 2026-06-30

### Added

- Optional Qt Widgets GUI example `ExampleQtGuiApp`.
- `CORETEMPLATE_BUILD_QT_GUI_EXAMPLE` CMake option.

### Notes

- The GUI example uses `CoreTemplateStd::QtAdapter`; `core.h` remains std-only.

## [0.2.0] - 2026-06-30

### Added

- Optional Qt adapter target `CoreTemplateStd::QtAdapter`.
- `CoreQtAdapter`, a `QObject` bridge that owns a std-only `Core` and re-emits core callbacks as Qt signals.
- Qt adapter smoke test and CI job.

### Notes

- The core target and `core.h` remain Qt-free.
- The Qt adapter is built only when `CORETEMPLATE_BUILD_QT_ADAPTER=ON`.

## [0.1.0] - 2026-06-30

Initial std-only release.

### Added

- Header-only C++17 `CoreTemplateStd` interface target.
- `CoreTemplateStd::CoreTemplateStd` CMake target and compatibility `CoreTemplate::CoreTemplate` alias.
- Std-only task execution with `std::thread`.
- Callback/event API for started, finished, terminated, stop-requested, and stop-timeout events.
- `std::any`/`std::vector<std::any>` task result and argument payloads.
- Cooperative cancellation and stop timeout reporting.
- Configurable std-only log handler.
- Console example application.
- Std-only test suite, package smoke tests, and CI sanitizer checks.

### Changed

- Core no longer depends on Qt libraries.
- Qt signals are replaced by explicit callback setters.
- Qt event-loop delivery is replaced by explicit `Core::processEvents()` calls.
- Force termination is kept as a compatibility switch, but the `std::thread` backend does not kill running threads.

### Migration Notes

- Prefer `find_package(CoreTemplateStd)` and `CoreTemplateStd::CoreTemplateStd` in new CMake consumers.
- Installed package consumers can include `#include <CoreTemplateStd/core.h>`.
- The old `find_package(CoreTemplate)` and `CoreTemplate::CoreTemplate` names remain available as compatibility aliases.

[0.5.1]: https://github.com/valeksan/CoreTemplateStd/releases/tag/v0.5.1
[0.5.0]: https://github.com/valeksan/CoreTemplateStd/releases/tag/v0.5.0
[0.4.0]: https://github.com/valeksan/CoreTemplateStd/releases/tag/v0.4.0
[0.3.0]: https://github.com/valeksan/CoreTemplateStd/releases/tag/v0.3.0
[0.2.0]: https://github.com/valeksan/CoreTemplateStd/releases/tag/v0.2.0
[0.1.0]: https://github.com/valeksan/CoreTemplateStd/releases/tag/v0.1.0

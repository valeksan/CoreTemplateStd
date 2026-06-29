# CoreTemplateStd

A modern, header-only C++17 library for running registered tasks in separate threads, with grouping, cooperative cancellation, stop timeouts, and a small callback/event API.

[![Build Status](https://github.com/valeksan/CoreTemplateStd/actions/workflows/ci.yml/badge.svg)](https://github.com/valeksan/CoreTemplateStd/actions)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-C++17-blue.svg)](https://en.cppreference.com/w/cpp/17)

*[Русская версия](README_RU.md)*

## Features

- **Header-only core**: copy `core.h` or link the exported CMake interface target.
- **No required Qt dependency in core**: public API uses standard C++ types.
- **Type-safe registration**: register free functions, lambdas, functors, non-const member functions, and const member functions.
- **Task grouping**: only one task per group runs at a time, while tasks from different groups can run concurrently.
- **Cooperative cancellation**: tasks can check `stopTaskFlag()` and exit gracefully.
- **Event callbacks**: observe started, finished, terminated, stop-requested, and stop-timeout events.
- **C++17 payloads**: task arguments and results are exposed as `std::vector<std::any>` and `std::any`.

## Getting Started

CoreTemplateStd requires a C++17 compiler. No Qt package is required to build or consume the core target.

Copy `core.h` into your project, or use CMake from this repository:

```cmake
add_subdirectory(CoreTemplateStd)
target_link_libraries(your_target PRIVATE CoreTemplateStd::CoreTemplateStd)
```

Install and consume via `find_package`:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --build build
cmake --install build
```

```cmake
find_package(CoreTemplateStd REQUIRED)
target_link_libraries(your_target PRIVATE CoreTemplateStd::CoreTemplateStd)
```

The old CMake package name remains available as a compatibility layer:

```cmake
find_package(CoreTemplate REQUIRED)
target_link_libraries(your_target PRIVATE CoreTemplate::CoreTemplate)
```

## Quick Start

```cpp
#include "core.h"

#include <any>
#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    Core core;
    bool finished = false;

    core.registerTask(1, [](int a, int b) -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return a + b;
    });

    core.onFinished([&](const FinishedEvent& event) {
        std::cout << "Result: " << std::any_cast<int>(event.result) << '\n';
        finished = true;
    });

    core.addTask(1, 10, 20);

    while (!finished) {
        core.processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
```

See `example/console_main.cpp` for a minimal runnable example.

## Public API

The complete API is defined in `core.h`. The primary methods are:

- `registerTask`: registers a function, lambda, functor, or member function by task type.
- `addTask`: queues a registered task with arguments.
- `unregisterTask`: removes a registered task type.
- `onStarted`, `onFinished`, `onTerminated`, `onStopRequested`, `onStopTimedOut`: set one callback per event kind.
- `processEvents`: delivers queued owner-thread events and should be called by the managing thread.
- `cancelTaskById`, `cancelTaskByType`, `cancelTaskByGroup`, `cancelTasks`, `cancelAllTasks`, `cancelTasksByGroup`: request cooperative cancellation.
- `stopTaskById`, `stopTaskByType`, `stopTaskByGroup`, `stopTasks`, `stopAllTasks`, `stopTasksByGroup`: compatibility names for the cancellation API.
- `terminateTaskById`: requests stop for an active task and reports a timeout if it does not stop cooperatively.
- `setAllowForceTermination`, `allowForceTermination`: retained compatibility switches; the current `std::thread` backend does not forcibly kill threads.
- `isTaskRegistered`, `groupByTask`, `isIdle`, `isTaskAddedByType`, `isTaskAddedByGroup`: query task state.
- `stopTaskFlag`: returns the thread-local stop flag for the currently running task.

## Threading Model

`Core` is designed for one managing thread.

- Create and use a `Core` instance from one thread.
- Call public methods such as `registerTask`, `addTask`, cancellation, and query methods from that same managing thread.
- Registered task functions run in worker threads managed by the library.
- Worker completion is queued back to the managing side; call `processEvents()` regularly to deliver callbacks and start queued follow-up tasks.
- Code running inside a task should not directly call public `Core` methods. Use your application-level message passing to communicate back to the managing thread.

## Cancellation And Termination

Cancellation is cooperative. A long-running task should periodically check:

```cpp
if (auto* stop = core.stopTaskFlag(); stop != nullptr && stop->load()) {
    return;
}
```

Force termination is disabled by default:

```cpp
Core core;
core.setAllowForceTermination(true);
```

The current `std::thread` backend has no safe standard way to kill a running thread. Enabling force termination keeps the API compatible, but non-cooperative tasks still receive a stop request and then a stop-timeout event if they keep running.

## Grouping Example

```cpp
#include "core.h"

#include <any>
#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    Core core;
    int finishedCount = 0;

    auto work = [](int value) -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return value * 10;
    };

    core.registerTask(1, work, 1);
    core.registerTask(2, work, 1);
    core.registerTask(3, work, 2);

    core.onFinished([&](const FinishedEvent& event) {
        std::cout << "Task " << event.type
                  << " result " << std::any_cast<int>(event.result) << '\n';
        ++finishedCount;
    });

    core.addTask(1, 10);
    core.addTask(2, 20);
    core.addTask(3, 30);

    while (finishedCount < 3) {
        core.processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
```

Tasks `1` and `2` share group `1`, so they run sequentially. Task `3` belongs to group `2`, so it can run concurrently with group `1`.

## Tests

```bash
cmake -S . -B build/std_only_check -DCORETEMPLATE_BUILD_TESTS=ON -DCORETEMPLATE_BUILD_EXAMPLE=ON
cmake --build build/std_only_check
ctest --test-dir build/std_only_check --output-on-failure
./build/std_only_check/example/ExampleConsoleApp
```

## Important Notes

- The core is header-only and implemented in `core.h`.
- `TaskArgs` is `std::vector<std::any>`.
- `TaskResult` is `std::any`; a `void` task produces an empty `std::any`.
- `std::any_cast<T>` is the caller's responsibility when reading callback payloads.
- The current backend uses `std::thread`; non-cooperative tasks cannot be forcibly killed by standard C++.

## Support The Project

If you find this library helpful and wish to support its development, feel free to use the Sponsor button. Any support is voluntary and entirely optional. The library remains free and open-source.

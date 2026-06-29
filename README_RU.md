# CoreTemplate

Современная header-only библиотека на C++17 для запуска зарегистрированных задач в отдельных потоках, с группировкой, кооперативной отменой, таймаутами остановки и небольшим callback/event API.

[![Build Status](https://github.com/valeksan/CoreTemplate/actions/workflows/ci.yml/badge.svg)](https://github.com/valeksan/CoreTemplate/actions)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-C++17-blue.svg)](https://en.cppreference.com/w/cpp/17)

*[English version](README.md)*

## Возможности

- **Header-only core**: можно скопировать `core.h` или подключить CMake interface target.
- **Без обязательной Qt-зависимости в core**: публичный API использует типы стандартной библиотеки.
- **Типобезопасная регистрация**: поддерживаются free functions, lambda, functor, non-const member functions и const member functions.
- **Группировка задач**: одновременно выполняется только одна задача в группе, а задачи из разных групп могут выполняться параллельно.
- **Кооперативная отмена**: задачи могут проверять `stopTaskFlag()` и завершаться аккуратно.
- **Callback events**: можно подписаться на started, finished, terminated, stop-requested и stop-timeout события.
- **C++17 payloads**: аргументы и результаты задач доступны как `std::vector<std::any>` и `std::any`.

## Начало работы

CoreTemplate требует компилятор с поддержкой C++17. Для сборки и использования core target пакет Qt не нужен.

Можно скопировать `core.h` в проект или подключить репозиторий через CMake:

```cmake
add_subdirectory(CoreTemplate)
target_link_libraries(your_target PRIVATE CoreTemplate::CoreTemplate)
```

Установка и подключение через `find_package`:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --build build
cmake --install build
```

```cmake
find_package(CoreTemplate REQUIRED)
target_link_libraries(your_target PRIVATE CoreTemplate::CoreTemplate)
```

## Быстрый старт

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

Минимальный запускаемый пример находится в `example/console_main.cpp`.

## Публичный API

Полный API определён в `core.h`. Основные методы:

- `registerTask`: регистрирует функцию, lambda, functor или member function по типу задачи.
- `addTask`: ставит зарегистрированную задачу в очередь с аргументами.
- `unregisterTask`: удаляет регистрацию типа задачи.
- `onStarted`, `onFinished`, `onTerminated`, `onStopRequested`, `onStopTimedOut`: задают по одному callback на каждый вид события.
- `processEvents`: доставляет события управляющему потоку; его нужно регулярно вызывать из управляющего потока.
- `cancelTaskById`, `cancelTaskByType`, `cancelTaskByGroup`, `cancelTasks`, `cancelAllTasks`, `cancelTasksByGroup`: запрашивают кооперативную отмену.
- `stopTaskById`, `stopTaskByType`, `stopTaskByGroup`, `stopTasks`, `stopAllTasks`, `stopTasksByGroup`: совместимые имена для cancellation API.
- `terminateTaskById`: запрашивает остановку активной задачи и сообщает timeout, если задача не остановилась кооперативно.
- `setAllowForceTermination`, `allowForceTermination`: сохранены как совместимые переключатели; текущий `std::thread` backend не убивает потоки принудительно.
- `isTaskRegistered`, `groupByTask`, `isIdle`, `isTaskAddedByType`, `isTaskAddedByGroup`: запрашивают состояние задач.
- `stopTaskFlag`: возвращает thread-local флаг остановки для текущей выполняющейся задачи.

## Модель потоков

`Core` рассчитан на один управляющий поток.

- Создавайте и используйте объект `Core` из одного потока.
- Вызывайте публичные методы `registerTask`, `addTask`, методы отмены и запросы состояния из этого же управляющего потока.
- Зарегистрированные функции выполняются в worker threads, которыми управляет библиотека.
- Завершение worker-задач ставится в очередь управляющей стороны; регулярно вызывайте `processEvents()`, чтобы доставлять callbacks и запускать ожидающие задачи.
- Код внутри задачи не должен напрямую вызывать публичные методы `Core`. Для связи с управляющим потоком используйте механизм сообщений вашего приложения.

## Отмена и принудительное завершение

Отмена является кооперативной. Долгая задача должна периодически проверять:

```cpp
if (auto* stop = core.stopTaskFlag(); stop != nullptr && stop->load()) {
    return;
}
```

Force termination по умолчанию отключён:

```cpp
Core core;
core.setAllowForceTermination(true);
```

В текущем `std::thread` backend нет безопасного стандартного способа убить выполняющийся поток. Включение force termination сохраняет совместимость API, но non-cooperative задача всё равно получает stop request, а затем stop-timeout event, если продолжает выполняться.

## Пример группировки

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

Задачи `1` и `2` находятся в группе `1`, поэтому выполняются последовательно. Задача `3` находится в группе `2`, поэтому может выполняться параллельно с группой `1`.

## Тесты

```bash
cmake -S . -B build/std_only_check -DCORETEMPLATE_BUILD_TESTS=ON -DCORETEMPLATE_BUILD_EXAMPLE=ON
cmake --build build/std_only_check
ctest --test-dir build/std_only_check --output-on-failure
./build/std_only_check/example/ExampleConsoleApp
```

## Важные замечания

- Core является header-only и реализован в `core.h`.
- `TaskArgs` это `std::vector<std::any>`.
- `TaskResult` это `std::any`; задача с `void` результатом создаёт пустой `std::any`.
- Чтение payload через `std::any_cast<T>` остаётся ответственностью вызывающего кода.
- Текущий backend использует `std::thread`; non-cooperative задачи нельзя принудительно убить средствами стандартного C++.

## Поддержка проекта

Если библиотека полезна, можно поддержать разработку через Sponsor. Любая поддержка добровольна и полностью опциональна. Библиотека остаётся бесплатной и открытой.

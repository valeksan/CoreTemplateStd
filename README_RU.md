# CoreTemplateStd

Современная header-only библиотека на C++17 для запуска зарегистрированных задач в отдельных потоках, с группировкой, кооперативной отменой, таймаутами остановки и небольшим callback/event API.

[![Build Status](https://github.com/valeksan/CoreTemplateStd/actions/workflows/ci.yml/badge.svg)](https://github.com/valeksan/CoreTemplateStd/actions)
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
- **Настраиваемое логирование**: debug и warning сообщения core можно перенаправить через callback стандартной библиотеки.
- **Опциональный Qt adapter**: отдельный QObject/signal мост без добавления Qt в `core.h`.
- **C++17 payloads**: аргументы и результаты задач доступны как `std::vector<std::any>` и `std::any`.

## Начало работы

CoreTemplateStd требует компилятор с поддержкой C++17. Для сборки и использования core target пакет Qt не нужен.

Можно скопировать `core.h` в проект или подключить репозиторий через CMake:

```cmake
add_subdirectory(CoreTemplateStd)
target_link_libraries(your_target PRIVATE CoreTemplateStd::CoreTemplateStd)
```

Подключение напрямую из GitHub через `FetchContent`:

```cmake
include(FetchContent)

FetchContent_Declare(
    CoreTemplateStd
    GIT_REPOSITORY https://github.com/valeksan/CoreTemplateStd.git
    GIT_TAG v0.3.0
)

FetchContent_MakeAvailable(CoreTemplateStd)

target_link_libraries(your_target PRIVATE CoreTemplateStd::CoreTemplateStd)
```

Установка и подключение через `find_package`:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --build build
cmake --install build
```

```cmake
find_package(CoreTemplateStd REQUIRED)
target_link_libraries(your_target PRIVATE CoreTemplateStd::CoreTemplateStd)
```

Потребители установленного пакета могут подключать заголовок явно:

```cpp
#include <CoreTemplateStd/core.h>
```

Старое имя CMake package остаётся доступным как слой совместимости:

```cmake
find_package(CoreTemplate REQUIRED)
target_link_libraries(your_target PRIVATE CoreTemplate::CoreTemplate)
```

## Опциональный Qt adapter

Core target остаётся без Qt. Если Qt-приложению нужна интеграция с signal/slot, включите отдельный adapter target при подключении исходников:

```cmake
set(CORETEMPLATE_BUILD_QT_ADAPTER ON)
add_subdirectory(CoreTemplateStd)

target_link_libraries(your_qt_target PRIVATE
    CoreTemplateStd::QtAdapter
)
```

`CoreQtAdapter` владеет std-only объектом `Core`, отдаёт доступ к нему через `core()` и переизлучает callbacks как Qt-сигналы с payload на `QVariantList`/`QVariant`. Adapter конвертирует базовые числовые типы и строки; неподдержанные значения `std::any` становятся invalid `QVariant`.

Qt Widgets GUI пример тоже включается отдельно:

```cmake
cmake -S . -B build/qt_gui \
  -DCORETEMPLATE_BUILD_EXAMPLE=OFF \
  -DCORETEMPLATE_BUILD_QT_GUI_EXAMPLE=ON
cmake --build build/qt_gui --target ExampleQtGuiApp
```

## Миграция с Qt CoreTemplate

CoreTemplateStd - это breaking std-only ветка исходного Qt-ориентированного API:

- Qt-сигналы вроде `finishedTask` заменены callback setters вроде `onFinished`.
- Payload на `QVariant` и `QVariantList` заменён на `std::any` и `std::vector<std::any>`.
- Доставка через Qt event loop заменена явным вызовом `processEvents()` из управляющего потока.
- `QObject` ownership удалён; `Core` создаётся напрямую без parent object.
- Force termination больше не убивает worker threads в `std::thread` backend; non-cooperative задачи получают stop request и timeout events.
- Старые CMake-имена остаются compatibility aliases, но для нового кода лучше использовать `CoreTemplateStd` и `CoreTemplateStd::CoreTemplateStd`.

## Быстрый старт

```cpp
#include <CoreTemplateStd/core.h>

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
- `setLogHandler`, `clearLogHandler`: настраивают или сбрасывают глобальный std-only обработчик логов core.
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
#include <CoreTemplateStd/core.h>

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
- Debug и warning сообщения core по умолчанию пишутся в `std::clog` и `std::cerr`, либо в обработчик, заданный через `Core::setLogHandler`.
- `TaskArgs` это `std::vector<std::any>`.
- `TaskResult` это `std::any`; задача с `void` результатом создаёт пустой `std::any`.
- Чтение payload через `std::any_cast<T>` остаётся ответственностью вызывающего кода.
- Текущий backend использует `std::thread`; non-cooperative задачи нельзя принудительно убить средствами стандартного C++.

## Поддержка проекта

Если библиотека полезна, можно поддержать разработку через Sponsor. Любая поддержка добровольна и полностью опциональна. Библиотека остаётся бесплатной и открытой.

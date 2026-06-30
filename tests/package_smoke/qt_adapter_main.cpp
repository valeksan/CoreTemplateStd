#include <CoreTemplateStd/qt_adapter/CoreQtAdapter.h>

#include <QCoreApplication>

#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    CoreQtAdapter adapter;
    int result = 0;
    bool finished = false;

    QObject::connect(&adapter, &CoreQtAdapter::finishedTask,
                     [&result, &finished](TaskId, TaskType, const QVariantList&, const QVariant& value) {
        result = value.toInt();
        finished = true;
    });

    adapter.core().registerTask(3, [](int value) -> int {
        return value + 39;
    });

    adapter.core().addTask(3, 3);

    const auto startedAt = std::chrono::steady_clock::now();
    while (!finished) {
        QCoreApplication::processEvents();
        if (std::chrono::steady_clock::now() - startedAt > std::chrono::seconds(2)) {
            std::cerr << "Qt adapter package smoke timed out\n";
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (result != 42) {
        std::cerr << "Unexpected Qt adapter package smoke result: " << result << '\n';
        return 1;
    }

    std::cout << "Qt adapter package smoke result: " << result << '\n';
    return 0;
}

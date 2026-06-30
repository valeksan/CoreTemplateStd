#include "CoreQtAdapter.h"

#include <QCoreApplication>
#include <QVariantList>

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
                     [&result, &finished](TaskId, TaskType type, const QVariantList& args, const QVariant& value) {
        if (type != 1 || args.size() != 1 || args.at(0).toInt() != 21) {
            return;
        }
        result = value.toInt();
        finished = true;
    });

    adapter.core().registerTask(1, [](int value) -> int {
        return value * 2;
    });

    adapter.core().addTask(1, 21);

    const auto startedAt = std::chrono::steady_clock::now();
    while (!finished) {
        QCoreApplication::processEvents();
        if (std::chrono::steady_clock::now() - startedAt > std::chrono::seconds(2)) {
            std::cerr << "Qt adapter smoke timed out\n";
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (result != 42) {
        std::cerr << "Unexpected Qt adapter smoke result: " << result << '\n';
        return 1;
    }

    std::cout << "Qt adapter smoke result: " << result << '\n';
    return 0;
}

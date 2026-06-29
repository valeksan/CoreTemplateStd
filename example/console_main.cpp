#include "../core.h"

#include <any>
#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    Core core;
    bool finished = false;

    core.registerTask(1, [](int value) -> int {
        return value * 2;
    });

    core.onFinished([&finished](const FinishedEvent& event) {
        std::cout << "Console example result: "
                  << std::any_cast<int>(event.result) << '\n';
        finished = true;
    });

    core.addTask(1, 21);

    const auto startedAt = std::chrono::steady_clock::now();
    while (!finished) {
        core.processEvents();
        if (std::chrono::steady_clock::now() - startedAt > std::chrono::seconds(2)) {
            std::cerr << "Console example timed out\n";
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}

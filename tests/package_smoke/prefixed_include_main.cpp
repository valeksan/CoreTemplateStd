#include <CoreTemplateStd/core.h>

#include <any>
#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    Core core;
    int result = 0;
    bool finished = false;

    core.registerTask(2, [](int value) -> int {
        return value * 2;
    });

    core.onFinished([&result, &finished](const FinishedEvent& event) {
        result = std::any_cast<int>(event.result);
        finished = true;
    });

    core.addTask(2, 21);

    const auto startedAt = std::chrono::steady_clock::now();
    while (!finished) {
        core.processEvents();
        if (std::chrono::steady_clock::now() - startedAt > std::chrono::seconds(2)) {
            std::cerr << "Prefixed include package smoke timed out\n";
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (result != 42) {
        std::cerr << "Unexpected prefixed include package smoke result: " << result << '\n';
        return 1;
    }

    std::cout << "Prefixed include package smoke result: " << result << '\n';
    return 0;
}

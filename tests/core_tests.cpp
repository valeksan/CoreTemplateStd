#include "../core.h"

#include <any>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define REQUIRE(condition) \
    do { \
        if (!(condition)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #condition); \
        } \
    } while (false)

#define REQUIRE_EQ(actual, expected) \
    do { \
        const auto actualValue = (actual); \
        const auto expectedValue = (expected); \
        if (!(actualValue == expectedValue)) { \
            throw std::runtime_error(std::string("Comparison failed: ") + #actual + " == " + #expected); \
        } \
    } while (false)

namespace {
void sleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

struct CoreEventRecorder {
    std::vector<StartedEvent> started;
    std::vector<FinishedEvent> finished;
    std::vector<TerminatedEvent> terminated;
    std::vector<StopRequestedEvent> stopRequested;
    std::vector<StopTimedOutEvent> stopTimedOut;

    void attach(Core& core) {
        core.onStarted([this](const StartedEvent& event) {
            started.push_back(event);
        });
        core.onFinished([this](const FinishedEvent& event) {
            finished.push_back(event);
        });
        core.onTerminated([this](const TerminatedEvent& event) {
            terminated.push_back(event);
        });
        core.onStopRequested([this](const StopRequestedEvent& event) {
            stopRequested.push_back(event);
        });
        core.onStopTimedOut([this](const StopTimedOutEvent& event) {
            stopTimedOut.push_back(event);
        });
    }
};

template <typename Predicate>
bool waitUntil(Core& core, Predicate predicate, int timeoutMs) {
    const auto startedAt = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - startedAt < std::chrono::milliseconds(timeoutMs)) {
        core.processEvents();
        if (predicate()) {
            return true;
        }
        sleepMs(1);
    }
    core.processEvents();
    return predicate();
}

int firstArgAsInt(const TaskArgs& args) {
    return std::any_cast<int>(args.at(0));
}

struct TestCase {
    const char* name;
    std::function<void()> function;
};
}

class CoreTests final {
public:
    void executesRegisteredTaskAndEmitsFinished();
    void invokesStdCallbacksWithAnyPayload();
    void serializesTasksWithinSameGroup();
    void cancelTaskByIdStopsCooperatively();
    void stopsTaskCooperativelyByFlag();
    void stopAllTasksStopsQueuedAndActive();
    void stopTasksBlocksQueueDuringStopAndResumesAfterActiveStops();
    void stopTasksByGroupWithQueuedOnlyAffectsSelectedGroup();
    void cancelTasksByGroupAliasWorks();
    void cancelAllTasksAliasWorks();
    void unregisterTaskFailsForActiveAndQueued();
    void registerTaskWithNullObjectThrows();
    void destroyingCoreRequestsStopAndWaitsForActiveTask();
    void terminateTaskByIdWhenForceDisabledRequestsCooperativeStopOnly();
    void terminateTaskByIdForceReportsTimeoutForNonCooperativeTask();
};

void CoreTests::executesRegisteredTaskAndEmitsFinished() {
    Core core;
    CoreEventRecorder events;
    events.attach(core);

    core.registerTask(1, [](int value) -> int {
        return value * 2;
    });

    core.addTask(1, 21);

    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() == 1; }, 2000));

    const FinishedEvent& event = events.finished.front();
    REQUIRE_EQ(event.type, 1);
    REQUIRE_EQ(std::any_cast<int>(event.result), 42);
}

void CoreTests::invokesStdCallbacksWithAnyPayload() {
    Core core;
    core.registerTask(2, [](int value) -> int {
        return value + 5;
    });

    bool startedSeen = false;
    bool finishedSeen = false;

    core.onStarted([&startedSeen](const StartedEvent& event) {
        startedSeen = true;
        REQUIRE_EQ(event.type, 2);
        REQUIRE_EQ(event.args.size(), static_cast<std::size_t>(1));
        REQUIRE_EQ(std::any_cast<int>(event.args.at(0)), 37);
    });

    core.onFinished([&finishedSeen](const FinishedEvent& event) {
        finishedSeen = true;
        REQUIRE_EQ(event.type, 2);
        REQUIRE_EQ(event.args.size(), static_cast<std::size_t>(1));
        REQUIRE_EQ(std::any_cast<int>(event.args.at(0)), 37);
        REQUIRE_EQ(std::any_cast<int>(event.result), 42);
    });

    core.addTask(2, 37);

    REQUIRE(startedSeen);
    REQUIRE(waitUntil(core, [&finishedSeen]() { return finishedSeen; }, 2000));
}

void CoreTests::serializesTasksWithinSameGroup() {
    Core core;
    std::atomic_int inGroup{0};
    std::atomic_int maxInGroup{0};

    auto groupedTask = [&inGroup, &maxInGroup]() -> int {
        const int now = ++inGroup;
        int observedMax = maxInGroup.load();
        while (now > observedMax && !maxInGroup.compare_exchange_weak(observedMax, now)) {
        }
        sleepMs(120);
        --inGroup;
        return 0;
    };

    core.registerTask(10, groupedTask, 7);
    core.registerTask(11, groupedTask, 7);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(10);
    core.addTask(11);

    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() == 2; }, 5000));
    REQUIRE_EQ(maxInGroup.load(), 1);
}

void CoreTests::cancelTaskByIdStopsCooperatively() {
    Core core;

    core.registerTask(14, [&core]() -> int {
        for (int i = 0; i < 500; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -14;
            }
            sleepMs(2);
        }
        return 14;
    }, 14, 100);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(14);
    REQUIRE(waitUntil(core, [&events]() { return events.started.size() == 1; }, 2000));

    const auto id = events.started.front().id;
    core.cancelTaskById(id);

    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() == 1; }, 5000));
    const FinishedEvent& event = events.finished.front();
    REQUIRE_EQ(event.type, 14);
    REQUIRE_EQ(std::any_cast<int>(event.result), -14);
}

void CoreTests::stopsTaskCooperativelyByFlag() {
    Core core;

    core.registerTask(20, [&core]() -> int {
        for (int i = 0; i < 400; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -1;
            }
            sleepMs(5);
        }
        return 1;
    }, 20, 80);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(20);
    sleepMs(30);
    core.stopTaskByType(20);

    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() == 1; }, 5000));
    REQUIRE_EQ(events.terminated.size(), static_cast<std::size_t>(0));

    const FinishedEvent& event = events.finished.front();
    REQUIRE_EQ(std::any_cast<int>(event.result), -1);
}

void CoreTests::stopAllTasksStopsQueuedAndActive() {
    Core core;

    core.registerTask(30, [&core](int tag) -> int {
        for (int i = 0; i < 600; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            sleepMs(2);
        }
        return tag;
    }, 30, 200);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(30, 1);
    core.addTask(30, 2);

    sleepMs(25);
    core.stopAllTasks();

    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() >= 1; }, 5000));
    REQUIRE(waitUntil(core, [&events]() { return events.terminated.size() >= 1; }, 5000));

    bool queuedTerminated = false;
    for (const TerminatedEvent& event : events.terminated) {
        if (!event.args.empty() && firstArgAsInt(event.args) == 2) {
            queuedTerminated = true;
            break;
        }
    }
    REQUIRE(queuedTerminated);

    bool activeStoppedCooperatively = false;
    for (const FinishedEvent& event : events.finished) {
        const int result = std::any_cast<int>(event.result);
        if (!event.args.empty() && firstArgAsInt(event.args) == 1 && result == -1) {
            activeStoppedCooperatively = true;
            break;
        }
    }
    REQUIRE(activeStoppedCooperatively);
}

void CoreTests::stopTasksBlocksQueueDuringStopAndResumesAfterActiveStops() {
    Core core;

    core.registerTask(33, [&core](int tag) -> int {
        for (int i = 0; i < 600; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            sleepMs(2);
        }
        return tag;
    }, 33, 120);

    core.registerTask(34, [](int tag) -> int {
        return tag * 10;
    }, 33, 120);

    CoreEventRecorder events;
    events.attach(core);

    bool stopTimerStarted = false;
    auto sinceStop = std::chrono::steady_clock::time_point{};
    long long queuedStartedAfterStopMs = -1;
    core.onStarted([&events, &stopTimerStarted, &sinceStop, &queuedStartedAfterStopMs](const StartedEvent& event) {
        events.started.push_back(event);
        if (event.args.empty() || firstArgAsInt(event.args) != 2) {
            return;
        }
        if (!stopTimerStarted) {
            return;
        }
        if (queuedStartedAfterStopMs < 0) {
            queuedStartedAfterStopMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - sinceStop
            ).count();
        }
    });

    core.addTask(33, 1); // active
    core.addTask(34, 2); // queued in same group

    sleepMs(20);
    sinceStop = std::chrono::steady_clock::now();
    stopTimerStarted = true;
    core.stopTasks();

    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() == 2; }, 5000));
    REQUIRE(queuedStartedAfterStopMs >= 100);

    bool activeStopped = false;
    bool queuedResumed = false;
    for (const FinishedEvent& event : events.finished) {
        if (event.args.empty()) {
            continue;
        }
        const int tag = firstArgAsInt(event.args);
        const int result = std::any_cast<int>(event.result);
        if (tag == 1 && result == -1) {
            activeStopped = true;
        }
        if (tag == 2 && result == 20) {
            queuedResumed = true;
        }
    }
    REQUIRE(activeStopped);
    REQUIRE(queuedResumed);
}

void CoreTests::stopTasksByGroupWithQueuedOnlyAffectsSelectedGroup() {
    Core core;

    core.registerTask(40, [&core](int tag) -> int {
        for (int i = 0; i < 600; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            sleepMs(2);
        }
        return tag;
    }, 1, 200);

    core.registerTask(41, [&core](int tag) -> int {
        for (int i = 0; i < 600; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            sleepMs(2);
        }
        return tag;
    }, 1, 200);

    core.registerTask(42, [](int tag) -> int {
        sleepMs(80);
        return tag * 10;
    }, 2, 200);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(40, 1);
    core.addTask(41, 2);
    core.addTask(42, 3);

    sleepMs(25);
    core.stopTasksByGroup(1, true);

    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() >= 2; }, 5000));
    REQUIRE(waitUntil(core, [&events]() { return events.terminated.size() >= 1; }, 5000));

    bool group1QueuedTerminated = false;
    for (const TerminatedEvent& event : events.terminated) {
        if (!event.args.empty() && firstArgAsInt(event.args) == 2) {
            group1QueuedTerminated = true;
            break;
        }
    }
    REQUIRE(group1QueuedTerminated);

    bool group1ActiveStopped = false;
    bool group2Completed = false;
    for (const FinishedEvent& event : events.finished) {
        const int result = std::any_cast<int>(event.result);
        if (!event.args.empty() && firstArgAsInt(event.args) == 1 && result == -1) {
            group1ActiveStopped = true;
        }
        if (!event.args.empty() && firstArgAsInt(event.args) == 3 && result == 30) {
            group2Completed = true;
        }
    }
    REQUIRE(group1ActiveStopped);
    REQUIRE(group2Completed);
}

void CoreTests::cancelTasksByGroupAliasWorks() {
    Core core;

    core.registerTask(45, [&core](int tag) -> int {
        for (int i = 0; i < 500; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            sleepMs(2);
        }
        return tag;
    }, 9, 150);

    core.registerTask(46, [&core](int tag) -> int {
        for (int i = 0; i < 500; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            sleepMs(2);
        }
        return tag;
    }, 9, 150);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(45, 1);
    core.addTask(46, 2);
    sleepMs(20);
    core.cancelTasksByGroup(9, true);

    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() >= 1; }, 5000));
    REQUIRE(waitUntil(core, [&events]() { return events.terminated.size() >= 1; }, 5000));

    bool queuedCancelled = false;
    for (const TerminatedEvent& event : events.terminated) {
        if (!event.args.empty() && firstArgAsInt(event.args) == 2) {
            queuedCancelled = true;
            break;
        }
    }
    REQUIRE(queuedCancelled);
}


void CoreTests::cancelAllTasksAliasWorks() {
    Core core;

    core.registerTask(47, [&core](int tag) -> int {
        for (int i = 0; i < 500; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            sleepMs(2);
        }
        return tag;
    }, 10, 150);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(47, 1);
    core.addTask(47, 2); // queued in same group
    sleepMs(20);
    core.cancelAllTasks();

    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() >= 1; }, 5000));
    REQUIRE(waitUntil(core, [&events]() { return events.terminated.size() >= 1; }, 5000));
}
void CoreTests::unregisterTaskFailsForActiveAndQueued() {
    Core core;

    core.registerTask(50, [&core](int tag) -> int {
        for (int i = 0; i < 800; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            sleepMs(2);
        }
        return tag;
    }, 50, 150);

    core.addTask(50, 1);
    sleepMs(20);
    REQUIRE(!core.unregisterTask(50));

    core.registerTask(51, [&core](int tag) -> int {
        for (int i = 0; i < 800; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            sleepMs(2);
        }
        return tag;
    }, 60, 150);

    core.registerTask(52, [&core](int tag) -> int {
        for (int i = 0; i < 800; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            sleepMs(2);
        }
        return tag;
    }, 60, 150);

    core.addTask(51, 2);
    core.addTask(52, 3);
    sleepMs(20);
    REQUIRE(!core.unregisterTask(52));

    CoreEventRecorder events;
    events.attach(core);
    core.stopAllTasks();
    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() >= 2; }, 5000));
}

void CoreTests::registerTaskWithNullObjectThrows() {
    class LocalCalculator {
    public:
        int add(int a, int b) { return a + b; }
    };

    Core core;
    LocalCalculator* pNullObj = nullptr;

    bool thrown = false;
    try {
        core.registerTask(70, &LocalCalculator::add, pNullObj);
    } catch (const std::logic_error&) {
        thrown = true;
    }

    REQUIRE(thrown);
    REQUIRE(!core.isTaskRegistered(70));
}

void CoreTests::destroyingCoreRequestsStopAndWaitsForActiveTask() {
    std::atomic_bool taskFinished{false};

    {
        Core core;
        core.registerTask(80, [&core, &taskFinished]() -> int {
            for (int i = 0; i < 3000; ++i) {
                if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                    taskFinished.store(true);
                    return -80;
                }
                sleepMs(1);
            }
            taskFinished.store(true);
            return 80;
        }, 80, 100);

        core.addTask(80);
        sleepMs(20);
    }

    REQUIRE(taskFinished.load());
}

void CoreTests::terminateTaskByIdWhenForceDisabledRequestsCooperativeStopOnly() {
    Core core;

    core.registerTask(82, []() -> int {
        for (int i = 0; i < 80; ++i) {
            sleepMs(2);
        }
        return 82;
    }, 82, 80);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(82);
    REQUIRE(waitUntil(core, [&events]() { return events.started.size() == 1; }, 2000));
    const auto id = events.started.front().id;

    core.terminateTaskById(id);

    REQUIRE(waitUntil(core, [&events]() { return events.stopTimedOut.size() == 1; }, 3000));
    REQUIRE_EQ(events.terminated.size(), static_cast<std::size_t>(0));
    REQUIRE(!core.isIdle());

    core.setAllowForceTermination(true);
    core.terminateTaskById(id);
    REQUIRE(waitUntil(core, [&events]() { return events.stopTimedOut.size() >= 2; }, 3000));
    REQUIRE_EQ(events.terminated.size(), static_cast<std::size_t>(0));
    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() == 1; }, 5000));
    REQUIRE(core.isIdle());
}

void CoreTests::terminateTaskByIdForceReportsTimeoutForNonCooperativeTask() {
    Core core;
    core.setAllowForceTermination(true);

    core.registerTask(81, []() -> int {
        for (int i = 0; i < 60; ++i) {
            sleepMs(10);
        }
        return 81;
    }, 81, 80);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(81);
    REQUIRE(waitUntil(core, [&events]() { return events.started.size() == 1; }, 2000));

    const auto id = events.started.front().id;

    sleepMs(30);
    core.terminateTaskById(id);

    REQUIRE(waitUntil(core, [&events]() { return events.stopTimedOut.size() == 1; }, 3000));
    REQUIRE_EQ(events.terminated.size(), static_cast<std::size_t>(0));
    REQUIRE(waitUntil(core, [&events]() { return events.finished.size() == 1; }, 3000));
    REQUIRE(core.isIdle());
}

int main()
{
    CoreTests tests;
    const TestCase testCases[] = {
        {"executesRegisteredTaskAndEmitsFinished", [&tests]() { tests.executesRegisteredTaskAndEmitsFinished(); }},
        {"invokesStdCallbacksWithAnyPayload", [&tests]() { tests.invokesStdCallbacksWithAnyPayload(); }},
        {"serializesTasksWithinSameGroup", [&tests]() { tests.serializesTasksWithinSameGroup(); }},
        {"cancelTaskByIdStopsCooperatively", [&tests]() { tests.cancelTaskByIdStopsCooperatively(); }},
        {"stopsTaskCooperativelyByFlag", [&tests]() { tests.stopsTaskCooperativelyByFlag(); }},
        {"stopAllTasksStopsQueuedAndActive", [&tests]() { tests.stopAllTasksStopsQueuedAndActive(); }},
        {"stopTasksBlocksQueueDuringStopAndResumesAfterActiveStops", [&tests]() { tests.stopTasksBlocksQueueDuringStopAndResumesAfterActiveStops(); }},
        {"stopTasksByGroupWithQueuedOnlyAffectsSelectedGroup", [&tests]() { tests.stopTasksByGroupWithQueuedOnlyAffectsSelectedGroup(); }},
        {"cancelTasksByGroupAliasWorks", [&tests]() { tests.cancelTasksByGroupAliasWorks(); }},
        {"cancelAllTasksAliasWorks", [&tests]() { tests.cancelAllTasksAliasWorks(); }},
        {"unregisterTaskFailsForActiveAndQueued", [&tests]() { tests.unregisterTaskFailsForActiveAndQueued(); }},
        {"registerTaskWithNullObjectThrows", [&tests]() { tests.registerTaskWithNullObjectThrows(); }},
        {"destroyingCoreRequestsStopAndWaitsForActiveTask", [&tests]() { tests.destroyingCoreRequestsStopAndWaitsForActiveTask(); }},
        {"terminateTaskByIdWhenForceDisabledRequestsCooperativeStopOnly", [&tests]() { tests.terminateTaskByIdWhenForceDisabledRequestsCooperativeStopOnly(); }},
        {"terminateTaskByIdForceReportsTimeoutForNonCooperativeTask", [&tests]() { tests.terminateTaskByIdForceReportsTimeoutForNonCooperativeTask(); }},
    };

    for (const TestCase& testCase : testCases) {
        try {
            testCase.function();
            std::cout << "[PASS] " << testCase.name << '\n';
        } catch (const std::exception& e) {
            std::cerr << "[FAIL] " << testCase.name << ": " << e.what() << '\n';
            return 1;
        } catch (...) {
            std::cerr << "[FAIL] " << testCase.name << ": unknown exception\n";
            return 1;
        }
    }

    return 0;
}

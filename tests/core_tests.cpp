#include <QtTest/QtTest>
#include <QThread>
#include <QElapsedTimer>
#include <atomic>
#include <vector>

#include "../core.h"

namespace {
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
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        core.processEvents();
        if (predicate()) {
            return true;
        }
        QTest::qWait(1);
    }
    core.processEvents();
    return predicate();
}

int firstArgAsInt(const TaskArgs& args) {
    return std::any_cast<int>(args.at(0));
}
}

class CoreTests final : public QObject {
    Q_OBJECT

private slots:
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
    void terminateTaskByIdForceStopsNonCooperativeTask();
};

void CoreTests::executesRegisteredTaskAndEmitsFinished() {
    Core core;
    CoreEventRecorder events;
    events.attach(core);

    core.registerTask(1, [](int value) -> int {
        return value * 2;
    });

    core.addTask(1, 21);

    QVERIFY(waitUntil(core, [&events]() { return events.finished.size() == 1; }, 2000));

    const FinishedEvent& event = events.finished.front();
    QCOMPARE(event.type, 1);
    QCOMPARE(std::any_cast<int>(event.result), 42);
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
        QCOMPARE(event.type, 2);
        QCOMPARE(event.args.size(), static_cast<std::size_t>(1));
        QCOMPARE(std::any_cast<int>(event.args.at(0)), 37);
    });

    core.onFinished([&finishedSeen](const FinishedEvent& event) {
        finishedSeen = true;
        QCOMPARE(event.type, 2);
        QCOMPARE(event.args.size(), static_cast<std::size_t>(1));
        QCOMPARE(std::any_cast<int>(event.args.at(0)), 37);
        QCOMPARE(std::any_cast<int>(event.result), 42);
    });

    core.addTask(2, 37);

    QVERIFY(startedSeen);
    QVERIFY(waitUntil(core, [&finishedSeen]() { return finishedSeen; }, 2000));
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
        QThread::msleep(120);
        --inGroup;
        return 0;
    };

    core.registerTask(10, groupedTask, 7);
    core.registerTask(11, groupedTask, 7);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(10);
    core.addTask(11);

    QVERIFY(waitUntil(core, [&events]() { return events.finished.size() == 2; }, 5000));
    QCOMPARE(maxInGroup.load(), 1);
}

void CoreTests::cancelTaskByIdStopsCooperatively() {
    Core core;

    core.registerTask(14, [&core]() -> int {
        for (int i = 0; i < 500; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -14;
            }
            QThread::msleep(2);
        }
        return 14;
    }, 14, 100);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(14);
    QVERIFY(waitUntil(core, [&events]() { return events.started.size() == 1; }, 2000));

    const auto id = events.started.front().id;
    core.cancelTaskById(id);

    QVERIFY(waitUntil(core, [&events]() { return events.finished.size() == 1; }, 5000));
    const FinishedEvent& event = events.finished.front();
    QCOMPARE(event.type, 14);
    QCOMPARE(std::any_cast<int>(event.result), -14);
}

void CoreTests::stopsTaskCooperativelyByFlag() {
    Core core;

    core.registerTask(20, [&core]() -> int {
        for (int i = 0; i < 400; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -1;
            }
            QThread::msleep(5);
        }
        return 1;
    }, 20, 80);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(20);
    QTest::qWait(30);
    core.stopTaskByType(20);

    QVERIFY(waitUntil(core, [&events]() { return events.finished.size() == 1; }, 5000));
    QCOMPARE(events.terminated.size(), static_cast<std::size_t>(0));

    const FinishedEvent& event = events.finished.front();
    QCOMPARE(std::any_cast<int>(event.result), -1);
}

void CoreTests::stopAllTasksStopsQueuedAndActive() {
    Core core;

    core.registerTask(30, [&core](int tag) -> int {
        for (int i = 0; i < 600; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            QThread::msleep(2);
        }
        return tag;
    }, 30, 200);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(30, 1);
    core.addTask(30, 2);

    QTest::qWait(25);
    core.stopAllTasks();

    QVERIFY(waitUntil(core, [&events]() { return events.finished.size() >= 1; }, 5000));
    QVERIFY(waitUntil(core, [&events]() { return events.terminated.size() >= 1; }, 5000));

    bool queuedTerminated = false;
    for (const TerminatedEvent& event : events.terminated) {
        if (!event.args.empty() && firstArgAsInt(event.args) == 2) {
            queuedTerminated = true;
            break;
        }
    }
    QVERIFY(queuedTerminated);

    bool activeStoppedCooperatively = false;
    for (const FinishedEvent& event : events.finished) {
        const int result = std::any_cast<int>(event.result);
        if (!event.args.empty() && firstArgAsInt(event.args) == 1 && result == -1) {
            activeStoppedCooperatively = true;
            break;
        }
    }
    QVERIFY(activeStoppedCooperatively);
}

void CoreTests::stopTasksBlocksQueueDuringStopAndResumesAfterActiveStops() {
    Core core;

    core.registerTask(33, [&core](int tag) -> int {
        for (int i = 0; i < 600; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            QThread::msleep(2);
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

    QTest::qWait(20);
    sinceStop = std::chrono::steady_clock::now();
    stopTimerStarted = true;
    core.stopTasks();

    QVERIFY(waitUntil(core, [&events]() { return events.finished.size() == 2; }, 5000));
    QVERIFY(queuedStartedAfterStopMs >= 100);

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
    QVERIFY(activeStopped);
    QVERIFY(queuedResumed);
}

void CoreTests::stopTasksByGroupWithQueuedOnlyAffectsSelectedGroup() {
    Core core;

    core.registerTask(40, [&core](int tag) -> int {
        for (int i = 0; i < 600; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            QThread::msleep(2);
        }
        return tag;
    }, 1, 200);

    core.registerTask(41, [&core](int tag) -> int {
        for (int i = 0; i < 600; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            QThread::msleep(2);
        }
        return tag;
    }, 1, 200);

    core.registerTask(42, [](int tag) -> int {
        QThread::msleep(80);
        return tag * 10;
    }, 2, 200);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(40, 1);
    core.addTask(41, 2);
    core.addTask(42, 3);

    QTest::qWait(25);
    core.stopTasksByGroup(1, true);

    QVERIFY(waitUntil(core, [&events]() { return events.finished.size() >= 2; }, 5000));
    QVERIFY(waitUntil(core, [&events]() { return events.terminated.size() >= 1; }, 5000));

    bool group1QueuedTerminated = false;
    for (const TerminatedEvent& event : events.terminated) {
        if (!event.args.empty() && firstArgAsInt(event.args) == 2) {
            group1QueuedTerminated = true;
            break;
        }
    }
    QVERIFY(group1QueuedTerminated);

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
    QVERIFY(group1ActiveStopped);
    QVERIFY(group2Completed);
}

void CoreTests::cancelTasksByGroupAliasWorks() {
    Core core;

    core.registerTask(45, [&core](int tag) -> int {
        for (int i = 0; i < 500; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            QThread::msleep(2);
        }
        return tag;
    }, 9, 150);

    core.registerTask(46, [&core](int tag) -> int {
        for (int i = 0; i < 500; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            QThread::msleep(2);
        }
        return tag;
    }, 9, 150);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(45, 1);
    core.addTask(46, 2);
    QTest::qWait(20);
    core.cancelTasksByGroup(9, true);

    QVERIFY(waitUntil(core, [&events]() { return events.finished.size() >= 1; }, 5000));
    QVERIFY(waitUntil(core, [&events]() { return events.terminated.size() >= 1; }, 5000));

    bool queuedCancelled = false;
    for (const TerminatedEvent& event : events.terminated) {
        if (!event.args.empty() && firstArgAsInt(event.args) == 2) {
            queuedCancelled = true;
            break;
        }
    }
    QVERIFY(queuedCancelled);
}


void CoreTests::cancelAllTasksAliasWorks() {
    Core core;

    core.registerTask(47, [&core](int tag) -> int {
        for (int i = 0; i < 500; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            QThread::msleep(2);
        }
        return tag;
    }, 10, 150);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(47, 1);
    core.addTask(47, 2); // queued in same group
    QTest::qWait(20);
    core.cancelAllTasks();

    QVERIFY(waitUntil(core, [&events]() { return events.finished.size() >= 1; }, 5000));
    QVERIFY(waitUntil(core, [&events]() { return events.terminated.size() >= 1; }, 5000));
}
void CoreTests::unregisterTaskFailsForActiveAndQueued() {
    Core core;

    core.registerTask(50, [&core](int tag) -> int {
        for (int i = 0; i < 800; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            QThread::msleep(2);
        }
        return tag;
    }, 50, 150);

    core.addTask(50, 1);
    QTest::qWait(20);
    QVERIFY(!core.unregisterTask(50));

    core.registerTask(51, [&core](int tag) -> int {
        for (int i = 0; i < 800; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            QThread::msleep(2);
        }
        return tag;
    }, 60, 150);

    core.registerTask(52, [&core](int tag) -> int {
        for (int i = 0; i < 800; ++i) {
            if (auto* stop = core.stopTaskFlag(); stop && stop->load()) {
                return -tag;
            }
            QThread::msleep(2);
        }
        return tag;
    }, 60, 150);

    core.addTask(51, 2);
    core.addTask(52, 3);
    QTest::qWait(20);
    QVERIFY(!core.unregisterTask(52));

    CoreEventRecorder events;
    events.attach(core);
    core.stopAllTasks();
    QVERIFY(waitUntil(core, [&events]() { return events.finished.size() >= 2; }, 5000));
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

    QVERIFY(thrown);
    QVERIFY(!core.isTaskRegistered(70));
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
                QThread::msleep(1);
            }
            taskFinished.store(true);
            return 80;
        }, 80, 100);

        core.addTask(80);
        QTest::qWait(20);
    }

    QVERIFY(taskFinished.load());
}

void CoreTests::terminateTaskByIdWhenForceDisabledRequestsCooperativeStopOnly() {
    Core core;

    core.registerTask(82, []() -> int {
        for (int i = 0; i < 2000; ++i) {
            QThread::msleep(2);
        }
        return 82;
    }, 82, 80);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(82);
    QVERIFY(waitUntil(core, [&events]() { return events.started.size() == 1; }, 2000));
    const auto id = events.started.front().id;

    core.terminateTaskById(id);

    QVERIFY(waitUntil(core, [&events]() { return events.stopTimedOut.size() == 1; }, 3000));
    QCOMPARE(events.terminated.size(), static_cast<std::size_t>(0));
    QVERIFY(!core.isIdle());

    core.setAllowForceTermination(true);
    core.terminateTaskById(id);
    QVERIFY(waitUntil(core, [&events]() { return events.terminated.size() == 1; }, 3000));
    QVERIFY(core.isIdle());
}

void CoreTests::terminateTaskByIdForceStopsNonCooperativeTask() {
    Core core;
    core.setAllowForceTermination(true);

    core.registerTask(81, []() -> int {
        for (int i = 0; i < 600; ++i) {
            QThread::msleep(10);
        }
        return 81;
    }, 81, 200);

    CoreEventRecorder events;
    events.attach(core);

    core.addTask(81);
    QVERIFY(waitUntil(core, [&events]() { return events.started.size() == 1; }, 2000));

    const auto id = events.started.front().id;

    QTest::qWait(30);
    core.terminateTaskById(id);

    QVERIFY(waitUntil(core, [&events]() { return events.terminated.size() == 1; }, 3000));
    QVERIFY(events.finished.size() == 0 || events.finished.size() == 1);
    QVERIFY(core.isIdle());
}

QTEST_MAIN(CoreTests)
#include "core_tests.moc"

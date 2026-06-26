#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QThread>
#include <QElapsedTimer>
#include <atomic>

#include "../core.h"

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
    core.registerTask(1, [](int value) -> int {
        return value * 2;
    });

    QSignalSpy finishedSpy(&core, &Core::finishedTask);
    QVERIFY(finishedSpy.isValid());

    core.addTask(1, 21);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 2000);

    const QList<QVariant> event = finishedSpy.takeFirst();
    QCOMPARE(event.at(1).toInt(), 1);
    QCOMPARE(event.at(3).toInt(), 42);
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
    QTRY_VERIFY_WITH_TIMEOUT(finishedSeen, 2000);
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

    QSignalSpy finishedSpy(&core, &Core::finishedTask);
    QVERIFY(finishedSpy.isValid());

    core.addTask(10);
    core.addTask(11);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 2, 5000);
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

    QSignalSpy startedSpy(&core, &Core::startedTask);
    QSignalSpy finishedSpy(&core, &Core::finishedTask);
    QVERIFY(startedSpy.isValid());
    QVERIFY(finishedSpy.isValid());

    core.addTask(14);
    QTRY_COMPARE_WITH_TIMEOUT(startedSpy.count(), 1, 2000);

    const QList<QVariant> startedEvent = startedSpy.takeFirst();
    const auto id = static_cast<TaskId>(startedEvent.at(0).toLongLong());
    core.cancelTaskById(id);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    const QList<QVariant> event = finishedSpy.takeFirst();
    QCOMPARE(event.at(1).toInt(), 14);
    QCOMPARE(event.at(3).toInt(), -14);
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

    QSignalSpy finishedSpy(&core, &Core::finishedTask);
    QSignalSpy terminatedSpy(&core, &Core::terminatedTask);
    QVERIFY(finishedSpy.isValid());
    QVERIFY(terminatedSpy.isValid());

    core.addTask(20);
    QTest::qWait(30);
    core.stopTaskByType(20);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QCOMPARE(terminatedSpy.count(), 0);

    const QList<QVariant> event = finishedSpy.takeFirst();
    QCOMPARE(event.at(3).toInt(), -1);
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

    QSignalSpy finishedSpy(&core, &Core::finishedTask);
    QSignalSpy terminatedSpy(&core, &Core::terminatedTask);
    QVERIFY(finishedSpy.isValid());
    QVERIFY(terminatedSpy.isValid());

    core.addTask(30, 1);
    core.addTask(30, 2);

    QTest::qWait(25);
    core.stopAllTasks();

    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(terminatedSpy.count() >= 1, 5000);

    bool queuedTerminated = false;
    for (const QList<QVariant>& event : terminatedSpy) {
        const QVariantList args = event.at(2).toList();
        if (!args.isEmpty() && args.first().toInt() == 2) {
            queuedTerminated = true;
            break;
        }
    }
    QVERIFY(queuedTerminated);

    bool activeStoppedCooperatively = false;
    for (const QList<QVariant>& event : finishedSpy) {
        const QVariantList args = event.at(2).toList();
        const int result = event.at(3).toInt();
        if (!args.isEmpty() && args.first().toInt() == 1 && result == -1) {
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

    QSignalSpy finishedSpy(&core, &Core::finishedTask);
    QSignalSpy startedSpy(&core, &Core::startedTask);
    QVERIFY(finishedSpy.isValid());
    QVERIFY(startedSpy.isValid());

    QElapsedTimer sinceStop;
    qint64 queuedStartedAfterStopMs = -1;
    QObject::connect(&core, &Core::startedTask, &core, [&sinceStop, &queuedStartedAfterStopMs](TaskId, TaskType, const QVariantList& argsList) {
        if (argsList.isEmpty() || argsList.first().toInt() != 2) {
            return;
        }
        if (!sinceStop.isValid()) {
            return;
        }
        if (queuedStartedAfterStopMs < 0) {
            queuedStartedAfterStopMs = sinceStop.elapsed();
        }
    });

    core.addTask(33, 1); // active
    core.addTask(34, 2); // queued in same group

    QTest::qWait(20);
    sinceStop.start();
    core.stopTasks();

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 2, 5000);
    QVERIFY(queuedStartedAfterStopMs >= 100);

    bool activeStopped = false;
    bool queuedResumed = false;
    for (const QList<QVariant>& event : finishedSpy) {
        const QVariantList args = event.at(2).toList();
        if (args.isEmpty()) {
            continue;
        }
        const int tag = args.first().toInt();
        const int result = event.at(3).toInt();
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

    QSignalSpy finishedSpy(&core, &Core::finishedTask);
    QSignalSpy terminatedSpy(&core, &Core::terminatedTask);
    QVERIFY(finishedSpy.isValid());
    QVERIFY(terminatedSpy.isValid());

    core.addTask(40, 1);
    core.addTask(41, 2);
    core.addTask(42, 3);

    QTest::qWait(25);
    core.stopTasksByGroup(1, true);

    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 2, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(terminatedSpy.count() >= 1, 5000);

    bool group1QueuedTerminated = false;
    for (const QList<QVariant>& event : terminatedSpy) {
        const QVariantList args = event.at(2).toList();
        if (!args.isEmpty() && args.first().toInt() == 2) {
            group1QueuedTerminated = true;
            break;
        }
    }
    QVERIFY(group1QueuedTerminated);

    bool group1ActiveStopped = false;
    bool group2Completed = false;
    for (const QList<QVariant>& event : finishedSpy) {
        const QVariantList args = event.at(2).toList();
        const int result = event.at(3).toInt();
        if (!args.isEmpty() && args.first().toInt() == 1 && result == -1) {
            group1ActiveStopped = true;
        }
        if (!args.isEmpty() && args.first().toInt() == 3 && result == 30) {
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

    QSignalSpy finishedSpy(&core, &Core::finishedTask);
    QSignalSpy terminatedSpy(&core, &Core::terminatedTask);
    QVERIFY(finishedSpy.isValid());
    QVERIFY(terminatedSpy.isValid());

    core.addTask(45, 1);
    core.addTask(46, 2);
    QTest::qWait(20);
    core.cancelTasksByGroup(9, true);

    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(terminatedSpy.count() >= 1, 5000);

    bool queuedCancelled = false;
    for (const QList<QVariant>& event : terminatedSpy) {
        const QVariantList args = event.at(2).toList();
        if (!args.isEmpty() && args.first().toInt() == 2) {
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

    QSignalSpy finishedSpy(&core, &Core::finishedTask);
    QSignalSpy terminatedSpy(&core, &Core::terminatedTask);
    QVERIFY(finishedSpy.isValid());
    QVERIFY(terminatedSpy.isValid());

    core.addTask(47, 1);
    core.addTask(47, 2); // queued in same group
    QTest::qWait(20);
    core.cancelAllTasks();

    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(terminatedSpy.count() >= 1, 5000);
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

    QSignalSpy finishedSpy(&core, &Core::finishedTask);
    QVERIFY(finishedSpy.isValid());
    core.stopAllTasks();
    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 2, 5000);
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

    QSignalSpy startedSpy(&core, &Core::startedTask);
    QSignalSpy stopTimedOutSpy(&core, &Core::stopTimedOutTask);
    QSignalSpy terminatedSpy(&core, &Core::terminatedTask);
    QVERIFY(startedSpy.isValid());
    QVERIFY(stopTimedOutSpy.isValid());
    QVERIFY(terminatedSpy.isValid());

    core.addTask(82);
    QTRY_COMPARE_WITH_TIMEOUT(startedSpy.count(), 1, 2000);
    const QList<QVariant> startedEvent = startedSpy.takeFirst();
    const auto id = static_cast<TaskId>(startedEvent.at(0).toLongLong());

    core.terminateTaskById(id);

    QTRY_COMPARE_WITH_TIMEOUT(stopTimedOutSpy.count(), 1, 3000);
    QCOMPARE(terminatedSpy.count(), 0);
    QVERIFY(!core.isIdle());

    core.setAllowForceTermination(true);
    core.terminateTaskById(id);
    QTRY_COMPARE_WITH_TIMEOUT(terminatedSpy.count(), 1, 3000);
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

    QSignalSpy startedSpy(&core, &Core::startedTask);
    QSignalSpy finishedSpy(&core, &Core::finishedTask);
    QSignalSpy terminatedSpy(&core, &Core::terminatedTask);
    QVERIFY(startedSpy.isValid());
    QVERIFY(finishedSpy.isValid());
    QVERIFY(terminatedSpy.isValid());

    core.addTask(81);
    QTRY_COMPARE_WITH_TIMEOUT(startedSpy.count(), 1, 2000);

    const QList<QVariant> startedEvent = startedSpy.takeFirst();
    const auto id = static_cast<TaskId>(startedEvent.at(0).toLongLong());

    QTest::qWait(30);
    core.terminateTaskById(id);

    QTRY_COMPARE_WITH_TIMEOUT(terminatedSpy.count(), 1, 3000);
    QVERIFY(finishedSpy.count() == 0 || finishedSpy.count() == 1);
    QVERIFY(core.isIdle());
}

QTEST_MAIN(CoreTests)
#include "core_tests.moc"

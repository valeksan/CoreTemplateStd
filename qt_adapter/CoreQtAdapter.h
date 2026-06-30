#ifndef CORE_QT_ADAPTER_H
#define CORE_QT_ADAPTER_H

#include "../core.h"

#include <QMetaType>
#include <QObject>
#include <QVariant>
#include <QVariantList>

class CoreQtAdapter : public QObject {
    Q_OBJECT

public:
    explicit CoreQtAdapter(QObject* parent = nullptr);

    Core& core();
    const Core& core() const;

    void processEvents();

    static QVariant toVariant(const TaskResult& value);
    static QVariantList toVariantList(const TaskArgs& args);

signals:
    void startedTask(TaskId id, TaskType type, QVariantList args);
    void finishedTask(TaskId id, TaskType type, QVariantList args, QVariant result);
    void terminatedTask(TaskId id, TaskType type, QVariantList args);
    void stopRequestedTask(TaskId id, TaskType type, QVariantList args);
    void stopTimedOutTask(TaskId id, TaskType type, QVariantList args, TaskStopTimeout timeout);

private:
    Core m_core;
};

#endif // CORE_QT_ADAPTER_H

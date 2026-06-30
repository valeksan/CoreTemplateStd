#include "CoreQtAdapter.h"

#include <QMetaObject>
#include <QString>
#include <QTimer>

#include <any>
#include <algorithm>
#include <string>

namespace {
template <typename T>
bool anyIs(const TaskResult& value) {
    return value.type() == typeid(T);
}

template <typename T>
QVariant anyToVariant(const TaskResult& value) {
    return QVariant::fromValue(std::any_cast<T>(value));
}
}

CoreQtAdapter::CoreQtAdapter(QObject* parent)
    : QObject(parent)
{
    m_core.setWakeCallback([this](TaskStopTimeout delayMs) {
        wakeCore(delayMs);
    });

    m_core.onStarted([this](const StartedEvent& event) {
        emit startedTask(event.id, event.type, toVariantList(event.args));
    });

    m_core.onFinished([this](const FinishedEvent& event) {
        emit finishedTask(event.id, event.type, toVariantList(event.args), toVariant(event.result));
    });

    m_core.onTerminated([this](const TerminatedEvent& event) {
        emit terminatedTask(event.id, event.type, toVariantList(event.args));
    });

    m_core.onStopRequested([this](const StopRequestedEvent& event) {
        emit stopRequestedTask(event.id, event.type, toVariantList(event.args));
    });

    m_core.onStopTimedOut([this](const StopTimedOutEvent& event) {
        emit stopTimedOutTask(event.id, event.type, toVariantList(event.args), event.timeout);
    });
}

CoreQtAdapter::~CoreQtAdapter()
{
    m_core.clearWakeCallback();
}

Core& CoreQtAdapter::core()
{
    return m_core;
}

const Core& CoreQtAdapter::core() const
{
    return m_core;
}

void CoreQtAdapter::processEvents()
{
    m_core.processEvents();
}

void CoreQtAdapter::wakeCore(TaskStopTimeout delayMs)
{
    const int normalizedDelayMs = static_cast<int>(std::max(delayMs, static_cast<TaskStopTimeout>(0)));
    if (normalizedDelayMs == 0) {
        QMetaObject::invokeMethod(this, [this]() {
            processEvents();
        }, Qt::QueuedConnection);
        return;
    }

    QMetaObject::invokeMethod(this, [this, normalizedDelayMs]() {
        QTimer::singleShot(normalizedDelayMs, this, [this]() {
            processEvents();
        });
    }, Qt::QueuedConnection);
}

QVariant CoreQtAdapter::toVariant(const TaskResult& value)
{
    if (!value.has_value()) {
        return QVariant{};
    }

    if (anyIs<bool>(value)) return anyToVariant<bool>(value);
    if (anyIs<int>(value)) return anyToVariant<int>(value);
    if (anyIs<unsigned int>(value)) return anyToVariant<unsigned int>(value);
    if (anyIs<long>(value)) return anyToVariant<long>(value);
    if (anyIs<unsigned long>(value)) return anyToVariant<unsigned long>(value);
    if (anyIs<long long>(value)) return anyToVariant<long long>(value);
    if (anyIs<unsigned long long>(value)) return anyToVariant<unsigned long long>(value);
    if (anyIs<float>(value)) return anyToVariant<float>(value);
    if (anyIs<double>(value)) return anyToVariant<double>(value);
    if (anyIs<QString>(value)) return anyToVariant<QString>(value);
    if (anyIs<std::string>(value)) {
        return QString::fromStdString(std::any_cast<std::string>(value));
    }
    if (anyIs<const char*>(value)) {
        return QString::fromUtf8(std::any_cast<const char*>(value));
    }
    if (anyIs<char*>(value)) {
        return QString::fromUtf8(std::any_cast<char*>(value));
    }

    return QVariant{};
}

QVariantList CoreQtAdapter::toVariantList(const TaskArgs& args)
{
    QVariantList values;
    values.reserve(static_cast<int>(args.size()));
    for (const auto& arg : args) {
        values.append(toVariant(arg));
    }
    return values;
}

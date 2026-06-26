// core.h
#ifndef CORE_H
#define CORE_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L   // POSIX.1-2001 (includes pthread_kill)
#endif

// --- Import the headers of the standard library ---
#include <atomic>
#include <functional>
#include <any>
#include <type_traits>
#include <algorithm>
#include <utility>
#include <stdexcept>
#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>

// --- Import Qt headers ---
#include <QObject>
#include <QVariant>
#include <QList>
#include <QMetaType>
#include <QDebug>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QCoreApplication>
#include <QEventLoop>
#include <QMetaObject>

// --- Threading/Multiprocessing API Headers ---
#ifdef Q_OS_WIN
    #include <windows.h>
#else
    #include <pthread.h>
    #include <signal.h>
#endif

// --- Using aliases to improve readability ---
using TaskId = long;
using TaskType = int;
using TaskGroup = int;
using TaskStopTimeout = int; // ms
using TaskArgs = std::vector<std::any>;
using TaskResult = std::any;

namespace core_detail {
inline thread_local std::atomic_bool* g_currentStopFlag = nullptr;
}

// --- Declaring constants ---
inline constexpr TaskStopTimeout kDefaultStopTimeout = 1000;

struct StartedEvent {
    TaskId id;
    TaskType type;
    TaskArgs args;
};

struct FinishedEvent {
    TaskId id;
    TaskType type;
    TaskArgs args;
    TaskResult result;
};

struct TerminatedEvent {
    TaskId id;
    TaskType type;
    TaskArgs args;
};

struct StopRequestedEvent {
    TaskId id;
    TaskType type;
    TaskArgs args;
};

struct StopTimedOutEvent {
    TaskId id;
    TaskType type;
    TaskArgs args;
    TaskStopTimeout timeout = kDefaultStopTimeout;
};

namespace core_detail {
template <typename... Args>
TaskArgs makeTaskArgs(Args&&... args) {
    TaskArgs taskArgs;
    taskArgs.reserve(sizeof...(Args));
    (taskArgs.emplace_back(std::forward<Args>(args)), ...);
    return taskArgs;
}
}

// --- Templates for checking convertibility ---
template<typename T>
struct all_convertible_to {
    template<typename... Args>
    static constexpr bool check() {
        return std::conjunction_v<std::is_convertible<Args, T>...>;
    }
};

template<>
struct all_convertible_to<QVariant> {
    template<typename... Args>
    static constexpr bool check() {
        return std::conjunction_v<std::disjunction<std::is_convertible<Args, QVariant>, std::bool_constant<QMetaTypeId<Args>::Defined>>...>;
    }
};

template<int N>
struct placeholder {};

/// @cond INTERNAL_DOCS
namespace std {
template<int N>
struct is_placeholder<::placeholder<N>> : public std::integral_constant<int, N> {};
}
/// @endcond

// --- Helper functions for binding member methods ---
template<typename R, typename... Args, typename Class, std::size_t... N>
auto bind_placeholders(R (Class::*taskFunction)(Args...), Class* taskObj, std::index_sequence<N...>) {
    return std::bind(taskFunction, taskObj, placeholder<N + 1>()...);
}

template<typename R, typename... Args, typename Class, std::size_t... N>
auto bind_placeholders(R (Class::*taskFunction)(Args...) const, Class* taskObj, std::index_sequence<N...>) {
    return std::bind(taskFunction, taskObj, placeholder<N + 1>()...);
}

// --- Classes ---
class TaskHelper final {
public:
    using FinishedHandler = std::function<void(TaskResult)>;

    explicit TaskHelper(std::function<TaskResult()> function, std::atomic_bool* pStopFlag, std::atomic_bool* pThreadExited, FinishedHandler finishedHandler);

#ifdef Q_OS_WIN
    static DWORD WINAPI functionWrapper(void* pTaskHelper);
#else
    static void* functionWrapper(void* pTaskHelper);
    static void cleanupThreadExit(void* pTaskHelper) noexcept;
#endif

private:
    std::function<TaskResult()> m_function;
    FinishedHandler m_finishedHandler;
    std::atomic_bool* m_pStopFlag = nullptr;
    std::atomic_bool* m_pThreadExited = nullptr;
    void execute(); // Method declaration
    void markThreadExited() noexcept;
};

/**
 * @brief The Core class manages task execution in separate threads.
 *
 * IMPORTANT: All public methods of this class (e.g., addTask, stopTaskById, etc.)
 * must be called from the same thread where the Core object lives (typically the main GUI thread).
 * Calling these methods from task threads concurrently may lead to undefined behavior.
 */
class Core : public QObject {
    Q_OBJECT

public:
    using StartedCallback = std::function<void(const StartedEvent&)>;
    using FinishedCallback = std::function<void(const FinishedEvent&)>;
    using TerminatedCallback = std::function<void(const TerminatedEvent&)>;
    using StopRequestedCallback = std::function<void(const StopRequestedEvent&)>;
    using StopTimedOutCallback = std::function<void(const StopTimedOutEvent&)>;

    explicit Core(QObject* parent = nullptr);
    ~Core() override;

    void onStarted(StartedCallback callback);
    void onFinished(FinishedCallback callback);
    void onTerminated(TerminatedCallback callback);
    void onStopRequested(StopRequestedCallback callback);
    void onStopTimedOut(StopTimedOutCallback callback);

    template <typename R, typename... Args>
    void registerTask(TaskType taskType, std::function<R(Args...)> taskFunction, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    template <typename R, typename... Args>
    void registerTask(TaskType taskType, R (*taskFunction)(Args...), TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    template <typename Class, typename R, typename... Args>
    void registerTask(TaskType taskType, R (Class::*taskMethod)(Args...), Class* taskObj, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    template <typename Class, typename R, typename... Args>
    void registerTask(TaskType taskType, R (Class::*taskMethod)(Args...) const, Class* taskObj, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    // More generic overload (for lambdas, functors, results of std::bind)
    template <typename F>
    void registerTask(TaskType taskType, F&& taskFunction, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    bool unregisterTask(TaskType taskType);

    template <typename... Args>
    void addTask(TaskType taskType, Args... args);

    std::atomic_bool* stopTaskFlag();
    void cancelTaskById(TaskId id);
    void cancelTaskByType(TaskType type);
    void cancelTaskByGroup(TaskGroup group);
    void cancelTasks();
    void cancelAllTasks();
    void cancelTasksByGroup(TaskGroup group, bool includeQueued);
    void terminateTaskById(TaskId id);
    void setAllowForceTermination(bool allow);
    bool allowForceTermination() const;
    void stopTaskById(TaskId id);
    void stopTaskByType(TaskType type);
    void stopTaskByGroup(TaskGroup group);
    void stopTasks();
    void stopAllTasks();
    void stopTasksByGroup(TaskGroup group, bool includeQueued);
    bool isTaskRegistered(TaskType type);
    TaskGroup groupByTask(TaskType type, bool* ok = nullptr);
    bool isIdle();
    bool isTaskAddedByType(TaskType type, bool* isActive = nullptr);
    bool isTaskAddedByGroup(TaskGroup group, bool* isActive = nullptr);

private:
    enum class TaskState {
        Inactive,
        Active,
        StopRequested,
        StopTimedOut,
        Finished,
        Terminated
    };

    struct TaskInfo {
        std::any m_function;
        TaskGroup m_group;
        TaskStopTimeout m_stopTimeout;
        std::function<QVariant(const TaskResult&)> m_resultToVariant;
    };

    struct Task {
        Task(std::function<TaskResult()> functionBound, TaskType type, TaskGroup group, TaskArgs stdArgs = {}, std::function<QList<QVariant>()> argsToVariantList = {}, std::function<QVariant(const TaskResult&)> resultToVariant = {})
            : m_functionBound(std::move(functionBound))
            , m_type(type)
            , m_group(group)
            , m_stdArgs(std::move(stdArgs))
            , m_argsToVariantList(std::move(argsToVariantList))
            , m_resultToVariant(std::move(resultToVariant))
            , m_state(TaskState::Inactive) {

            static TaskId id_counter = 0;
            m_id = id_counter++; // m_id cannot be initialized in the list, because it depends on the counter
        }

        TaskId m_id;
        std::function<TaskResult()> m_functionBound;
        TaskType m_type;
        TaskGroup m_group;
        TaskArgs m_stdArgs;
        std::function<QList<QVariant>()> m_argsToVariantList;
        std::function<QVariant(const TaskResult&)> m_resultToVariant;
    #ifdef Q_OS_WIN
        HANDLE m_threadHandle = nullptr;
        DWORD m_threadId = 0;
    #else
        pthread_t m_threadHandle = 0;
    #endif
        std::atomic_bool m_stopFlag{false};
        std::atomic_bool m_threadExited{false};
        TaskState m_state;
    };

    std::shared_ptr<Task> activeTaskById(TaskId id);
    std::shared_ptr<Task> activeTaskByType(TaskType type);
    std::shared_ptr<Task> activeTaskByGroup(TaskGroup group);

    void terminateTask(std::shared_ptr<Task> pTask);
    void stopTask(std::shared_ptr<Task> pTask);
    void startTask(std::shared_ptr<Task> pTask);
    void startQueuedTask();
    void removeActiveTask(const std::shared_ptr<Task>& pTask);
    bool ensureCalledFromOwnerThread(const char* method) const;
    static StartedEvent makeStartedEvent(const Task& task);
    static FinishedEvent makeFinishedEvent(const Task& task, TaskResult result);
    static TerminatedEvent makeTerminatedEvent(const Task& task);
    static StopRequestedEvent makeStopRequestedEvent(const Task& task);
    static StopTimedOutEvent makeStopTimedOutEvent(const Task& task, TaskStopTimeout timeout);
    void publishStarted(const Task& task);
    void publishFinished(const Task& task, TaskResult result);
    void publishTerminated(const Task& task);
    void publishStopRequested(const Task& task);
    void publishStopTimedOut(const Task& task, TaskStopTimeout timeout);

    template <typename... Args>
    void insertToTaskHash(TaskType taskType, std::function<TaskResult(Args...)> taskFunction, std::function<QVariant(const TaskResult&)> resultToVariant, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    std::unordered_map<TaskType, TaskInfo> m_taskHash;
    std::vector<std::shared_ptr<Task>> m_activeTaskList;
    std::vector<std::shared_ptr<Task>> m_queuedTaskList;
    std::atomic_bool m_blockStartTask{false};
    bool m_allowForceTermination = false;
    std::thread::id m_ownerThreadId;
    StartedCallback m_startedCallback;
    FinishedCallback m_finishedCallback;
    TerminatedCallback m_terminatedCallback;
    StopRequestedCallback m_stopRequestedCallback;
    StopTimedOutCallback m_stopTimedOutCallback;

signals:
    void finishedTask(TaskId id, TaskType type, QList<QVariant> argsList = {}, QVariant result = QVariant());
    void startedTask(TaskId id, TaskType type, QList<QVariant> argsList = {});
    void terminatedTask(TaskId id, TaskType type, QList<QVariant> argsList = {});
    void stopRequestedTask(TaskId id, TaskType type, QList<QVariant> argsList = {});
    void stopTimedOutTask(TaskId id, TaskType type, QList<QVariant> argsList = {}, TaskStopTimeout timeout = kDefaultStopTimeout);
};

// --- Class method implementations *after* class declarations ---

// TaskHelper Implementation
inline TaskHelper::TaskHelper(std::function<TaskResult()> function, std::atomic_bool* pStopFlag, std::atomic_bool* pThreadExited, FinishedHandler finishedHandler)
    : m_function(function), m_finishedHandler(std::move(finishedHandler)), m_pStopFlag(pStopFlag), m_pThreadExited(pThreadExited) {}

inline void TaskHelper::markThreadExited() noexcept {
    if (m_pThreadExited) {
        m_pThreadExited->store(true);
    }
}

inline void TaskHelper::execute() {
    core_detail::g_currentStopFlag = m_pStopFlag;
    TaskResult result;
    try {
        result = m_function();
    } catch (...) {
        core_detail::g_currentStopFlag = nullptr;
        markThreadExited();
        throw;
    }
    core_detail::g_currentStopFlag = nullptr;
    markThreadExited();
    if (m_finishedHandler) {
        m_finishedHandler(std::move(result));
    }
}

#ifdef Q_OS_WIN
inline DWORD TaskHelper::functionWrapper(void* pTaskHelper) {
    TaskHelper *pThisTaskHelper = reinterpret_cast<TaskHelper *>(pTaskHelper);
    if (pThisTaskHelper) {
        pThisTaskHelper->execute();
        delete pThisTaskHelper;
    }
    return 0;
}
#else
inline void TaskHelper::cleanupThreadExit(void* pTaskHelper) noexcept {
    TaskHelper *pThisTaskHelper = reinterpret_cast<TaskHelper *>(pTaskHelper);
    if (pThisTaskHelper) {
        pThisTaskHelper->markThreadExited();
        delete pThisTaskHelper;
    }
}

inline void* TaskHelper::functionWrapper(void* pTaskHelper) {
#if defined(PTHREAD_CANCEL_ENABLE) && defined(PTHREAD_CANCEL_ASYNCHRONOUS)
    // Terminate path uses pthread_cancel; asynchronous cancellation makes forced stop predictable
    // for non-cooperative tasks (terminateTaskById), while cooperative stop remains unchanged.
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, nullptr);
#endif
    TaskHelper *pThisTaskHelper = reinterpret_cast<TaskHelper *>(pTaskHelper);
    if (pThisTaskHelper) {
        pthread_cleanup_push(&TaskHelper::cleanupThreadExit, pThisTaskHelper);
        pThisTaskHelper->execute();
        pthread_cleanup_pop(1);
    }
    return nullptr;
}
#endif

// Core Implementation
inline Core::Core(QObject* parent)
    : QObject(parent), m_ownerThreadId(std::this_thread::get_id()) {}

inline void Core::onStarted(StartedCallback callback) {
    m_startedCallback = std::move(callback);
}

inline void Core::onFinished(FinishedCallback callback) {
    m_finishedCallback = std::move(callback);
}

inline void Core::onTerminated(TerminatedCallback callback) {
    m_terminatedCallback = std::move(callback);
}

inline void Core::onStopRequested(StopRequestedCallback callback) {
    m_stopRequestedCallback = std::move(callback);
}

inline void Core::onStopTimedOut(StopTimedOutCallback callback) {
    m_stopTimedOutCallback = std::move(callback);
}

inline Core::~Core() {
    // Best-effort synchronous shutdown to avoid destroying QObject children while worker threads are still running.
    if (std::this_thread::get_id() != m_ownerThreadId) {
        qWarning() << "Core::~Core - called from non-owner thread. Forcing stop flags only.";
        for (const auto& pTask : std::as_const(m_activeTaskList)) {
            pTask->m_stopFlag.store(true);
        }
        return;
    }

    // Remove queued tasks first: they never started.
    for (const auto& pQueuedTask : std::as_const(m_queuedTaskList)) {
        publishTerminated(*pQueuedTask);
    }
    m_queuedTaskList.clear();

    if (m_activeTaskList.empty()) {
        return;
    }

    // Block new starts and request cooperative stop for all active tasks.
    m_blockStartTask.store(true);
    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        pTask->m_stopFlag.store(true);
        if (pTask->m_state == TaskState::Active) {
            pTask->m_state = TaskState::StopRequested;
            publishStopRequested(*pTask);
        }
    }

    QElapsedTimer waitTimer;
    waitTimer.start();
    constexpr TaskStopTimeout kDtorWaitMs = 2000;

    while (!m_activeTaskList.empty() && waitTimer.elapsed() < kDtorWaitMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }

    if (m_activeTaskList.empty()) {
        return;
    }

    // Escalate only for stubborn tasks that ignored cooperative stop and only if force termination is allowed.
    if (m_allowForceTermination) {
        const auto stubbornTasks = m_activeTaskList;
        for (const auto& pTask : stubbornTasks) {
            terminateTask(pTask);
        }
    } else {
        qWarning() << "Core::~Core - force termination disabled. Active tasks may outlive shutdown window.";
        return;
    }

    while (!m_activeTaskList.empty() && waitTimer.elapsed() < (kDtorWaitMs * 2)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }

    if (!m_activeTaskList.empty()) {
        qWarning() << "Core::~Core - active tasks still present after shutdown timeout:" << m_activeTaskList.size();
    }
}

inline bool Core::ensureCalledFromOwnerThread(const char* method) const {
    if (std::this_thread::get_id() == m_ownerThreadId) {
        return true;
    }
    qWarning() << "Core::" << method << "- called from non-owner thread.";
    return false;
}

inline StartedEvent Core::makeStartedEvent(const Task& task) {
    return StartedEvent{task.m_id, task.m_type, task.m_stdArgs};
}

inline FinishedEvent Core::makeFinishedEvent(const Task& task, TaskResult result) {
    return FinishedEvent{task.m_id, task.m_type, task.m_stdArgs, std::move(result)};
}

inline TerminatedEvent Core::makeTerminatedEvent(const Task& task) {
    return TerminatedEvent{task.m_id, task.m_type, task.m_stdArgs};
}

inline StopRequestedEvent Core::makeStopRequestedEvent(const Task& task) {
    return StopRequestedEvent{task.m_id, task.m_type, task.m_stdArgs};
}

inline StopTimedOutEvent Core::makeStopTimedOutEvent(const Task& task, TaskStopTimeout timeout) {
    return StopTimedOutEvent{task.m_id, task.m_type, task.m_stdArgs, timeout};
}

inline void Core::publishStarted(const Task& task) {
    const auto event = makeStartedEvent(task);
    if (m_startedCallback) {
        m_startedCallback(event);
    }
    const auto qtArgs = task.m_argsToVariantList ? task.m_argsToVariantList() : QList<QVariant>{};
    emit startedTask(event.id, event.type, qtArgs);
}

inline void Core::publishFinished(const Task& task, TaskResult result) {
    const auto event = makeFinishedEvent(task, std::move(result));
    if (m_finishedCallback) {
        m_finishedCallback(event);
    }
    const QVariant qtResult = task.m_resultToVariant ? task.m_resultToVariant(event.result) : QVariant();
    const auto qtArgs = task.m_argsToVariantList ? task.m_argsToVariantList() : QList<QVariant>{};
    emit finishedTask(event.id, event.type, qtArgs, qtResult);
}

inline void Core::publishTerminated(const Task& task) {
    const auto event = makeTerminatedEvent(task);
    if (m_terminatedCallback) {
        m_terminatedCallback(event);
    }
    const auto qtArgs = task.m_argsToVariantList ? task.m_argsToVariantList() : QList<QVariant>{};
    emit terminatedTask(event.id, event.type, qtArgs);
}

inline void Core::publishStopRequested(const Task& task) {
    const auto event = makeStopRequestedEvent(task);
    if (m_stopRequestedCallback) {
        m_stopRequestedCallback(event);
    }
    const auto qtArgs = task.m_argsToVariantList ? task.m_argsToVariantList() : QList<QVariant>{};
    emit stopRequestedTask(event.id, event.type, qtArgs);
}

inline void Core::publishStopTimedOut(const Task& task, TaskStopTimeout timeout) {
    const auto event = makeStopTimedOutEvent(task, timeout);
    if (m_stopTimedOutCallback) {
        m_stopTimedOutCallback(event);
    }
    const auto qtArgs = task.m_argsToVariantList ? task.m_argsToVariantList() : QList<QVariant>{};
    emit stopTimedOutTask(event.id, event.type, qtArgs, event.timeout);
}

template <typename R, typename... Args>
void Core::registerTask(TaskType taskType, std::function<R(Args...)> taskFunction, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    if (!ensureCalledFromOwnerThread("registerTask")) {
        throw std::logic_error("Core::registerTask must be called from the owner thread");
    }

    std::function<TaskResult(std::remove_reference_t<Args>...)> f;
    std::function<QVariant(const TaskResult&)> resultToVariant;

    if constexpr (std::is_void_v<R>) {
        f = [taskFunction](std::remove_reference_t<Args>... args) -> TaskResult {
            taskFunction(args...);
            return TaskResult{};
        };
        resultToVariant = [](const TaskResult&) -> QVariant {
            return QVariant();
        };
    } else if constexpr (std::is_convertible_v<R, QVariant>) {
        f = [taskFunction](std::remove_reference_t<Args>... args) -> TaskResult {
            return TaskResult(taskFunction(args...));
        };
        resultToVariant = [](const TaskResult& result) -> QVariant {
            return QVariant(std::any_cast<R>(result));
        };
    } else if constexpr (QMetaTypeId<R>::Defined) {
        f = [taskFunction](std::remove_reference_t<Args>... args) -> TaskResult {
            return TaskResult(taskFunction(args...));
        };
        resultToVariant = [](const TaskResult& result) -> QVariant {
            return QVariant::fromValue(std::any_cast<R>(result));
        };
    } else {
        qWarning() << "Core::registerTask - Not convertible return type for task type:" << taskType;
        throw std::logic_error("Not convertible return type");
    }

    insertToTaskHash(taskType, std::move(f), std::move(resultToVariant), taskGroup, taskStopTimeout);
}

template <typename R, typename... Args>
void Core::registerTask(TaskType taskType, R (*taskFunction)(Args...), TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    // Use deduction guide for std::function
    registerTask(taskType, std::function(taskFunction), taskGroup, taskStopTimeout);
}

template <typename Class, typename R, typename... Args>
void Core::registerTask(TaskType taskType, R (Class::*taskMethod)(Args...), Class* taskObj, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    if (taskObj == nullptr) {
        qWarning() << "Core::registerTask - task object is null for task type:" << taskType;
        throw std::logic_error("Task object is null");
    }

    // Direct call to bind_placeholders and registerTask with explicit std::function signature
    auto boundFunc = bind_placeholders(taskMethod, taskObj, std::make_index_sequence<sizeof...(Args)>());
    // Use deduction guide for std::function
    registerTask(taskType, static_cast<std::function<R(Args...)>>(std::move(boundFunc)), taskGroup, taskStopTimeout);
}

template <typename Class, typename R, typename... Args>
void Core::registerTask(TaskType taskType, R (Class::*taskMethod)(Args...) const, Class* taskObj, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    if (taskObj == nullptr) {
        qWarning() << "Core::registerTask - task object is null for task type:" << taskType;
        throw std::logic_error("Task object is null");
    }

    auto boundFunc = bind_placeholders(taskMethod, taskObj, std::make_index_sequence<sizeof...(Args)>());
    // Use deduction guide for std::function
    registerTask(taskType, static_cast<std::function<R(Args...)>>(std::move(boundFunc)), taskGroup, taskStopTimeout);
}

// Generic overload (works for lambdas, functors)
template <typename F>
void Core::registerTask(TaskType taskType, F&& taskFunction, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    // Try to convert to std::function, relying on template argument deduction
    // This works if F has an operator() with a unique signature
    registerTask(taskType, std::function(std::forward<F>(taskFunction)), taskGroup, taskStopTimeout);
}

inline bool Core::unregisterTask(TaskType taskType) {
    if (!ensureCalledFromOwnerThread("unregisterTask")) {
        return false;
    }

    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        if (pTask->m_type == taskType) {
            qWarning() << "Core::unregisterTask - Cannot unregister active task type:" << taskType;
            return false;
        }
    }
    for (const auto& pTask : std::as_const(m_queuedTaskList)) {
        if (pTask->m_type == taskType) {
            qWarning() << "Core::unregisterTask - Cannot unregister queued task type:" << taskType;
            return false;
        }
    }
    return m_taskHash.erase(taskType) > 0;
}

template <typename... Args>
void Core::addTask(TaskType taskType, Args... args) {
    if (!ensureCalledFromOwnerThread("addTask")) {
        throw std::logic_error("Core::addTask must be called from the owner thread");
    }

    auto taskInfoIt = m_taskHash.find(taskType);
    if (taskInfoIt == m_taskHash.cend()) {
        qWarning() << "Core::addTask - Task not registered for type:" << taskType;
        throw std::logic_error("Task not registered");
    }

    try {
        const auto& taskInfo = taskInfoIt->second;
        auto storedFuncAny = taskInfo.m_function;
        auto taskFunction = std::any_cast<std::function<TaskResult(Args...)>>(storedFuncAny);
        TaskArgs stdArgs = core_detail::makeTaskArgs(args...);

        std::function<QList<QVariant>()> argsToVariantList;
        if constexpr (all_convertible_to<QVariant>::check<Args...>()) {
            argsToVariantList = [args...]() -> QList<QVariant> {
                return { QVariant::fromValue(args)... };
            };
        } else {
            qWarning() << "Core::addTask - Arguments are not convertible to QList<QVariant> for task type:" << taskType;
            argsToVariantList = []() -> QList<QVariant> {
                return {};
            };
        }

        auto taskFunctionBound = std::bind(taskFunction, args...);
        auto pTask = std::make_shared<Task>(
            std::move(taskFunctionBound),
            taskType,
            taskInfo.m_group,
            std::move(stdArgs),
            std::move(argsToVariantList),
            taskInfo.m_resultToVariant
        );

        bool start = std::none_of(m_activeTaskList.begin(), m_activeTaskList.end(), [group = pTask->m_group](const auto& pActiveTask) {
            return pActiveTask->m_group == group;
        });

        if (start && !m_blockStartTask.load()) {
            startTask(pTask);
        } else {
            m_queuedTaskList.push_back(std::move(pTask));
        }
    } catch (const std::bad_any_cast& e) {
        qWarning() << "Core::addTask - Bad arguments or function signature mismatch for task type:" << taskType << e.what();
        throw std::logic_error("Bad arguments or function signature mismatch");
    }
}

[[nodiscard]] inline std::atomic_bool* Core::stopTaskFlag() {
    return core_detail::g_currentStopFlag;
}

inline void Core::terminateTaskById(TaskId id) {
    if (!ensureCalledFromOwnerThread("terminateTaskById")) {
        return;
    }

    if (auto pTask = activeTaskById(id)) {
        if (!m_allowForceTermination) {
            qWarning() << "Core::terminateTaskById - force termination is disabled. Requesting cooperative stop for task ID:" << id;
            stopTaskById(id);
            return;
        }
        terminateTask(std::move(pTask));
    }
}

inline void Core::setAllowForceTermination(bool allow) {
    if (!ensureCalledFromOwnerThread("setAllowForceTermination")) {
        return;
    }
    m_allowForceTermination = allow;
}

inline bool Core::allowForceTermination() const {
    if (!ensureCalledFromOwnerThread("allowForceTermination")) {
        return false;
    }
    return m_allowForceTermination;
}

inline void Core::cancelTaskById(TaskId id) {
    if (!ensureCalledFromOwnerThread("cancelTaskById")) {
        return;
    }
    stopTaskById(id);
}

inline void Core::cancelTaskByType(TaskType type) {
    if (!ensureCalledFromOwnerThread("cancelTaskByType")) {
        return;
    }
    stopTaskByType(type);
}

inline void Core::cancelTaskByGroup(TaskGroup group) {
    if (!ensureCalledFromOwnerThread("cancelTaskByGroup")) {
        return;
    }
    stopTaskByGroup(group);
}

inline void Core::cancelTasks() {
    if (!ensureCalledFromOwnerThread("cancelTasks")) {
        return;
    }
    stopTasks();
}

inline void Core::cancelAllTasks() {
    if (!ensureCalledFromOwnerThread("cancelAllTasks")) {
        return;
    }
    stopAllTasks();
}

inline void Core::cancelTasksByGroup(TaskGroup group, bool includeQueued) {
    if (!ensureCalledFromOwnerThread("cancelTasksByGroup")) {
        return;
    }
    stopTasksByGroup(group, includeQueued);
}

inline void Core::stopTaskById(TaskId id) {
    if (!ensureCalledFromOwnerThread("stopTaskById")) {
        return;
    }

    if (auto pTask = activeTaskById(id)) {
        stopTask(std::move(pTask));
    }
}

inline void Core::stopTaskByType(TaskType type) {
    if (!ensureCalledFromOwnerThread("stopTaskByType")) {
        return;
    }

    if (auto pTask = activeTaskByType(type)) {
        stopTask(std::move(pTask));
    }
}

inline void Core::stopTaskByGroup(TaskGroup group) {
    if (!ensureCalledFromOwnerThread("stopTaskByGroup")) {
        return;
    }

    stopTasksByGroup(group, false);
}

inline void Core::stopTasks() {
    if (!ensureCalledFromOwnerThread("stopTasks")) {
        return;
    }

    if (m_activeTaskList.empty()) {
        return;
    }

    m_blockStartTask.store(true);
    QTimer* pTimer = new QTimer(this); // with parent for automatic cleanup!
    connect(pTimer, &QTimer::timeout, this, [this, pTimer]() {
        if (isIdle()) { // Use the public method
            m_blockStartTask.store(false);
            startQueuedTask();
            pTimer->stop();
            pTimer->deleteLater();
        }
    });

    // Calculating the maximum stop timeout among active tasks
    TaskStopTimeout maxTimeout = 0;
    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        auto taskInfoIt = m_taskHash.find(pTask->m_type);
        if (taskInfoIt != m_taskHash.cend()) {
            maxTimeout = std::max(maxTimeout, taskInfoIt->second.m_stopTimeout);
        } else {
            maxTimeout = std::max(maxTimeout, static_cast<TaskStopTimeout>(kDefaultStopTimeout));
            qWarning() << "Core::stopTasks - Missing registration for active task type:" << pTask->m_type;
        }
    }

    // Requesting to stop all active tasks
    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        stopTask(pTask);
    }

    // Starting the timer with the maximum timeout
    pTimer->start(maxTimeout);
}

inline void Core::stopAllTasks() {
    if (!ensureCalledFromOwnerThread("stopAllTasks")) {
        return;
    }

    // Remove queued tasks immediately (they never started, so no stop timeout needed).
    for (const auto& pQueuedTask : std::as_const(m_queuedTaskList)) {
        publishTerminated(*pQueuedTask);
    }
    m_queuedTaskList.clear();

    // Then request stop for all currently active tasks.
    stopTasks();
}

inline void Core::stopTasksByGroup(TaskGroup group, bool includeQueued) {
    if (!ensureCalledFromOwnerThread("stopTasksByGroup")) {
        return;
    }

    std::vector<std::shared_ptr<Task>> activeInGroup;
    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        if (pTask->m_group == group) {
            activeInGroup.push_back(pTask);
        }
    }

    for (const auto& pTask : std::as_const(activeInGroup)) {
        stopTask(pTask);
    }

    if (!includeQueued) {
        return;
    }

    for (auto it = m_queuedTaskList.begin(); it != m_queuedTaskList.end();) {
        const auto& pQueuedTask = *it;
        if (pQueuedTask->m_group == group) {
            publishTerminated(*pQueuedTask);
            it = m_queuedTaskList.erase(it);
        } else {
            ++it;
        }
    }
}

[[nodiscard]] inline bool Core::isTaskRegistered(TaskType type) {
    if (!ensureCalledFromOwnerThread("isTaskRegistered")) {
        return false;
    }

    return m_taskHash.find(type) != m_taskHash.cend();
}

inline TaskGroup Core::groupByTask(TaskType type, bool* ok) {
    if (!ensureCalledFromOwnerThread("groupByTask")) {
        if (ok) *ok = false;
        return -1;
    }

    auto taskInfoIt = m_taskHash.find(type);
    if (taskInfoIt != m_taskHash.cend()) {
        if (ok) *ok = true;
        return taskInfoIt->second.m_group;
    } else {
        if (ok) *ok = false;
        return -1;
    }
}

[[nodiscard]] inline bool Core::isIdle() {
    if (!ensureCalledFromOwnerThread("isIdle")) {
        return false;
    }

    return m_activeTaskList.empty();
}

[[nodiscard]] inline bool Core::isTaskAddedByType(TaskType type, bool* isActive) {
    if (!ensureCalledFromOwnerThread("isTaskAddedByType")) {
        if (isActive) *isActive = false;
        return false;
    }

    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        if (pTask->m_type == type) {
            if (isActive) *isActive = true;
            return true;
        }
    }
    for (const auto& pTask : std::as_const(m_queuedTaskList)) {
        if (pTask->m_type == type) {
            if (isActive) *isActive = false;
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline bool Core::isTaskAddedByGroup(TaskGroup group, bool* isActive) {
    if (!ensureCalledFromOwnerThread("isTaskAddedByGroup")) {
        if (isActive) *isActive = false;
        return false;
    }

    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        if (pTask->m_group == group) {
            if (isActive) *isActive = true;
            return true;
        }
    }
    for (const auto& pTask : std::as_const(m_queuedTaskList)) {
        if (pTask->m_group == group) {
            if (isActive) *isActive = false;
            return true;
        }
    }
    return false;
}

// --- Internal method implementations (inline) ---

inline std::shared_ptr<Core::Task> Core::activeTaskById(TaskId id) {
    auto it = std::find_if(m_activeTaskList.begin(), m_activeTaskList.end(),
                           [id](const auto& pTask) { return pTask->m_id == id; });
    return (it != m_activeTaskList.end()) ? *it : std::shared_ptr<Task>{};
}

inline std::shared_ptr<Core::Task> Core::activeTaskByType(TaskType type) {
    auto it = std::find_if(m_activeTaskList.begin(), m_activeTaskList.end(),
                           [type](const auto& pTask) { return pTask->m_type == type; });
    return (it != m_activeTaskList.end()) ? *it : std::shared_ptr<Task>{};
}

inline std::shared_ptr<Core::Task> Core::activeTaskByGroup(TaskGroup group) {
    auto it = std::find_if(m_activeTaskList.begin(), m_activeTaskList.end(),
                           [group](const auto& pTask) { return pTask->m_group == group; });
    return (it != m_activeTaskList.end()) ? *it : std::shared_ptr<Task>{};
}

inline void Core::removeActiveTask(const std::shared_ptr<Task>& pTask) {
    m_activeTaskList.erase(std::remove(m_activeTaskList.begin(), m_activeTaskList.end(), pTask), m_activeTaskList.end());
}

inline void Core::terminateTask(std::shared_ptr<Core::Task> pTask) {
    // Set stop flag to request cooperative cancellation
    pTask->m_stopFlag.store(true);

    if (pTask->m_state == TaskState::Active) {
        pTask->m_state = TaskState::StopRequested;
        publishStopRequested(*pTask);
    }

    // Determine timeout from task's registered stop timeout (verification window).
    TaskStopTimeout timeout = kDefaultStopTimeout;
    auto taskInfoIt = m_taskHash.find(pTask->m_type);
    if (taskInfoIt != m_taskHash.cend()) {
        timeout = taskInfoIt->second.m_stopTimeout;
    }

    // IMPORTANT: do not block the UI thread here.
    // Request termination first, then confirm termination asynchronously.
    bool terminationRequested = false;

#ifdef Q_OS_WIN
    if (pTask->m_threadHandle) {
        if (WaitForSingleObject(pTask->m_threadHandle, 0) != WAIT_OBJECT_0) {
            terminationRequested = (TerminateThread(pTask->m_threadHandle, 1) != 0);
        } else {
            // Thread already exited, but finishedTask might never be emitted (e.g. forceful exit path).
            pTask->m_state = TaskState::Terminated;
            CloseHandle(pTask->m_threadHandle);
            pTask->m_threadHandle = nullptr;
            publishTerminated(*pTask);
            removeActiveTask(pTask);
            startQueuedTask();
            return;
        }
    } else {
        return; // no valid handle
    }
#else
    if (pTask->m_threadHandle != 0) {
        if (!pTask->m_threadExited.load()) {
            terminationRequested = (pthread_cancel(pTask->m_threadHandle) == 0);
        } else {
            // Thread already not alive, but finishedTask might never be emitted (e.g. pthread_exit/cancel path).
            pTask->m_state = TaskState::Terminated;
            publishTerminated(*pTask);
            removeActiveTask(pTask);
            startQueuedTask();
            return;
        }
    } else {
        return; // no valid handle
    }
#endif

    if (!terminationRequested) {
        pTask->m_state = TaskState::StopTimedOut;
        qWarning() << QString("Task %1 terminate request was rejected by platform API").arg(QString::number(pTask->m_id));
        publishStopTimedOut(*pTask, timeout);
        return;
    }

    auto started = std::make_shared<QElapsedTimer>();
    started->start();
    auto checker = std::make_shared<std::function<void()>>();

    *checker = [this, pTask, timeout, started, checker]() {
        if (pTask->m_state == TaskState::Inactive
            || pTask->m_state == TaskState::Finished
            || pTask->m_state == TaskState::Terminated) {
            return; // already finished/terminated by another path
        }

        bool isAlive = false;
#ifdef Q_OS_WIN
        if (pTask->m_threadHandle) {
            const DWORD waitResult = WaitForSingleObject(pTask->m_threadHandle, 0);
            isAlive = (waitResult == WAIT_TIMEOUT);
            if (!isAlive) {
                CloseHandle(pTask->m_threadHandle);
                pTask->m_threadHandle = nullptr;
            }
        }
#else
        if (pTask->m_threadHandle != 0) {
            isAlive = !pTask->m_threadExited.load();
        }
#endif

        if (!isAlive) {
            pTask->m_state = TaskState::Terminated;
            publishTerminated(*pTask);
            removeActiveTask(pTask);
            startQueuedTask();
            return;
        }

        if (started->elapsed() >= timeout) {
            pTask->m_state = TaskState::StopTimedOut;
            qWarning() << QString("Task %1 did not stop after terminate request within timeout (%2 ms)")
                              .arg(QString::number(pTask->m_id)).arg(timeout);
            publishStopTimedOut(*pTask, timeout);
            return;
        }

        QTimer::singleShot(20, this, [checker]() {
            (*checker)();
        });
    };

    QTimer::singleShot(0, this, [checker]() {
        (*checker)();
    });
}

inline void Core::stopTask(std::shared_ptr<Core::Task> pTask) {
    pTask->m_stopFlag.store(true);
    if (pTask->m_state == TaskState::Active) {
        pTask->m_state = TaskState::StopRequested;
        publishStopRequested(*pTask);
    }

    TaskStopTimeout timeout = kDefaultStopTimeout;
    auto taskInfoIt = m_taskHash.find(pTask->m_type);
    if (taskInfoIt != m_taskHash.cend()) {
        timeout = taskInfoIt->second.m_stopTimeout;
    } else {
        qWarning() << "Core::stopTask - Missing registration for active task type:" << pTask->m_type;
    }

    QTimer::singleShot(timeout, this, [this, pTask, timeout]() {
        switch (pTask->m_state) {
        case TaskState::Finished:
            qDebug() << QString("Task %1 was successfully stopped").arg(QString::number(pTask->m_id));
            break;
        case TaskState::Terminated:
            qDebug() << QString("Task %1 was terminated").arg(QString::number(pTask->m_id));
            break;
        case TaskState::StopTimedOut:
            qDebug() << QString("Task %1 stop already timed out").arg(QString::number(pTask->m_id));
            break;
        case TaskState::StopRequested:
        case TaskState::Active:
            if (!m_allowForceTermination) {
                pTask->m_state = TaskState::StopTimedOut;
                qWarning() << QString("Task %1 stop timed out; force termination is disabled").arg(QString::number(pTask->m_id));
                publishStopTimedOut(*pTask, timeout);
                break;
            }
            qDebug() << QString("Task %1 was not stopped, terminating").arg(QString::number(pTask->m_id));
            terminateTask(pTask);
            if (pTask->m_state == TaskState::Active || pTask->m_state == TaskState::StopRequested) {
                qDebug() << QString("Task %1 terminate request is in progress")
                                .arg(QString::number(pTask->m_id));
            }
            break;
        default:
            qDebug() << QString("Task %1 unexpected state").arg(QString::number(pTask->m_id));
            break;
        }
    });
}

inline void Core::startTask(std::shared_ptr<Core::Task> pTask) {
    m_activeTaskList.push_back(pTask);
    pTask->m_state = TaskState::Active;
    pTask->m_threadExited.store(false);
    TaskHelper* pTaskHelper = new TaskHelper(
        pTask->m_functionBound,
        &pTask->m_stopFlag,
        &pTask->m_threadExited,
        [this, pTask](TaskResult result) {
            QMetaObject::invokeMethod(this, [this, pTask, result = std::move(result)]() mutable {
                pTask->m_state = TaskState::Finished;
                publishFinished(*pTask, std::move(result));
                removeActiveTask(pTask);
                startQueuedTask();
            }, Qt::QueuedConnection);
        }
    );

#ifdef Q_OS_WIN
    pTask->m_threadHandle = CreateThread(nullptr, 0, &TaskHelper::functionWrapper, pTaskHelper, 0, &pTask->m_threadId);
    if (pTask->m_threadHandle == NULL) {
        qWarning() << "Core::startTask - Failed to create thread for task ID:" << pTask->m_id << ". GetLastError:" << GetLastError();
        removeActiveTask(pTask);
        // emit taskCreationFailed(...);
        startQueuedTask();
        delete pTaskHelper;
        return; // Abort StartTask execution
    }
    // If everything is OK, continue...
#else
    // Checking pthread_create
    int result = pthread_create(&pTask->m_threadHandle, nullptr, &TaskHelper::functionWrapper, pTaskHelper);
    if (result != 0) {
        qWarning() << "Core::startTask - Failed to create thread for task ID:" << pTask->m_id << ". Error code:" << result;
        removeActiveTask(pTask);
        // emit taskCreationFailed(...);
        startQueuedTask();
        delete pTaskHelper;
        return; // Abort StartTask execution
    }
    // If everything is OK, continue...
    pthread_detach(pTask->m_threadHandle);
#endif

    publishStarted(*pTask);
}

inline void Core::startQueuedTask() {
    if (m_blockStartTask.load()) {
        return;
    }

    for (auto it = m_queuedTaskList.begin(); it != m_queuedTaskList.end();) {
        std::shared_ptr<Task> pQueuedTask = *it;
        bool canStart = std::none_of(std::as_const(m_activeTaskList).begin(), std::as_const(m_activeTaskList).end(),
                                     [&pQueuedTask](const std::shared_ptr<Task>& pActiveTask) {
            return pActiveTask->m_group == pQueuedTask->m_group;
        });
        if (canStart) {
            it = m_queuedTaskList.erase(it);
            startTask(std::move(pQueuedTask));
        } else {
            ++it;
        }
    }
}

template <typename... Args>
void Core::insertToTaskHash(TaskType taskType, std::function<TaskResult(Args...)> taskFunction, std::function<QVariant(const TaskResult&)> resultToVariant, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    if (m_taskHash.find(taskType) != m_taskHash.cend()) {
        qWarning() << "Core::registerTask - Task type is already registered:" << taskType;
        throw std::logic_error("Task type is already registered");
    }

    TaskStopTimeout normalizedStopTimeout = taskStopTimeout;
    if (normalizedStopTimeout < 0) {
        qWarning() << "Core::registerTask - Negative stop timeout for task type:"
                   << taskType << ". Using default:" << kDefaultStopTimeout;
        normalizedStopTimeout = kDefaultStopTimeout;
    }

    m_taskHash.emplace(taskType, TaskInfo{taskFunction, taskGroup, normalizedStopTimeout, std::move(resultToVariant)});
}

#endif // CORE_H

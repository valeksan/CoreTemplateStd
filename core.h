// core.h
#ifndef CORE_H
#define CORE_H
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
#include <chrono>
#include <iostream>
#include <mutex>
#include <deque>
#include <sstream>
#include <string>

// --- Using aliases to improve readability ---
using TaskId = long;
using TaskType = int;
using TaskGroup = int;
using TaskStopTimeout = int; // ms
using TaskArgs = std::vector<std::any>;
using TaskResult = std::any;

enum class CoreLogLevel {
    Debug,
    Warning
};

using CoreLogHandler = std::function<void(CoreLogLevel, const std::string&)>;

namespace core_detail {
inline thread_local std::atomic_bool* g_currentStopFlag = nullptr;

inline std::mutex& logMutex() {
    static std::mutex mutex;
    return mutex;
}

inline CoreLogHandler& logHandler() {
    static CoreLogHandler handler;
    return handler;
}

inline void setLogHandler(CoreLogHandler handler) {
    std::lock_guard<std::mutex> lock(logMutex());
    logHandler() = std::move(handler);
}

inline void publishLog(CoreLogLevel level, const std::string& message) {
    CoreLogHandler handlerCopy;
    {
        std::lock_guard<std::mutex> lock(logMutex());
        handlerCopy = logHandler();
    }

    if (handlerCopy) {
        handlerCopy(level, message);
        return;
    }

    std::ostream& stream = (level == CoreLogLevel::Warning) ? std::cerr : std::clog;
    stream << message << '\n';
}

class LogLine {
public:
    explicit LogLine(CoreLogLevel level)
        : m_level(level) {}

    LogLine(const LogLine&) = delete;
    LogLine& operator=(const LogLine&) = delete;

    ~LogLine() {
        publishLog(m_level, m_stream.str());
    }

    template <typename T>
    LogLine& operator<<(const T& value) {
        if (!m_firstValue) {
            m_stream << ' ';
        }
        m_stream << value;
        m_firstValue = false;
        return *this;
    }

private:
    CoreLogLevel m_level;
    std::ostringstream m_stream;
    bool m_firstValue = true;
};

inline LogLine logWarning() {
    return LogLine(CoreLogLevel::Warning);
}

inline LogLine logDebug() {
    return LogLine(CoreLogLevel::Debug);
}
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

/**
 * @brief The Core class manages task execution in separate threads.
 *
 * IMPORTANT: All public methods of this class (e.g., addTask, stopTaskById, etc.)
 * must be called from the same thread where the Core object lives (typically the main GUI thread).
 * Calling these methods from task threads concurrently may lead to undefined behavior.
 */
class Core {
public:
    using LogHandler = CoreLogHandler;
    using StartedCallback = std::function<void(const StartedEvent&)>;
    using FinishedCallback = std::function<void(const FinishedEvent&)>;
    using TerminatedCallback = std::function<void(const TerminatedEvent&)>;
    using StopRequestedCallback = std::function<void(const StopRequestedEvent&)>;
    using StopTimedOutCallback = std::function<void(const StopTimedOutEvent&)>;

    Core();
    ~Core();

    static void setLogHandler(LogHandler handler);
    static void clearLogHandler();

    void onStarted(StartedCallback callback);
    void onFinished(FinishedCallback callback);
    void onTerminated(TerminatedCallback callback);
    void onStopRequested(StopRequestedCallback callback);
    void onStopTimedOut(StopTimedOutCallback callback);
    void processEvents();

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

    struct ScheduledOwnerEvent {
        std::chrono::steady_clock::time_point dueAt;
        std::function<void()> event;
    };

    struct TaskInfo {
        std::any m_function;
        TaskGroup m_group;
        TaskStopTimeout m_stopTimeout;
    };

    struct Task {
        Task(std::function<TaskResult()> functionBound, TaskType type, TaskGroup group, TaskArgs stdArgs = {})
            : m_functionBound(std::move(functionBound))
            , m_type(type)
            , m_group(group)
            , m_stdArgs(std::move(stdArgs))
            , m_state(TaskState::Inactive) {

            static TaskId id_counter = 0;
            m_id = id_counter++; // m_id cannot be initialized in the list, because it depends on the counter
        }

        TaskId m_id;
        std::function<TaskResult()> m_functionBound;
        TaskType m_type;
        TaskGroup m_group;
        TaskArgs m_stdArgs;
        std::thread m_thread;
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
    void joinTaskThread(Task& task);
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
    void postToOwner(std::function<void()> event);
    void scheduleOnOwnerAfter(TaskStopTimeout delayMs, std::function<void()> event);
    void clearOwnerEvents();

    template <typename... Args>
    void insertToTaskHash(TaskType taskType, std::function<TaskResult(Args...)> taskFunction, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    std::unordered_map<TaskType, TaskInfo> m_taskHash;
    std::vector<std::shared_ptr<Task>> m_activeTaskList;
    std::vector<std::shared_ptr<Task>> m_queuedTaskList;
    std::atomic_bool m_blockStartTask{false};
    bool m_allowForceTermination = false;
    std::thread::id m_ownerThreadId;
    std::mutex m_eventMutex;
    std::deque<std::function<void()>> m_eventQueue;
    std::vector<ScheduledOwnerEvent> m_scheduledEvents;
    StartedCallback m_startedCallback;
    FinishedCallback m_finishedCallback;
    TerminatedCallback m_terminatedCallback;
    StopRequestedCallback m_stopRequestedCallback;
    StopTimedOutCallback m_stopTimedOutCallback;
};

// --- Class method implementations *after* class declarations ---

// Core Implementation
inline Core::Core()
    : m_ownerThreadId(std::this_thread::get_id()) {}

inline void Core::setLogHandler(LogHandler handler) {
    core_detail::setLogHandler(std::move(handler));
}

inline void Core::clearLogHandler() {
    core_detail::setLogHandler({});
}

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

inline void Core::processEvents() {
    if (!ensureCalledFromOwnerThread("processEvents")) {
        return;
    }

    for (;;) {
        std::function<void()> event;
        {
            std::lock_guard<std::mutex> lock(m_eventMutex);
            if (!m_eventQueue.empty()) {
                event = std::move(m_eventQueue.front());
                m_eventQueue.pop_front();
            } else {
                const auto now = std::chrono::steady_clock::now();
                auto it = std::find_if(m_scheduledEvents.begin(), m_scheduledEvents.end(),
                                       [&now](const ScheduledOwnerEvent& scheduledEvent) {
                                           return scheduledEvent.dueAt <= now;
                                       });
                if (it == m_scheduledEvents.end()) {
                    return;
                }
                event = std::move(it->event);
                m_scheduledEvents.erase(it);
            }
        }

        if (event) {
            event();
        }
    }
}

inline Core::~Core() {
    // Best-effort synchronous shutdown to avoid destroying Core while worker threads are still running.
    if (std::this_thread::get_id() != m_ownerThreadId) {
        core_detail::logWarning() << "Core::~Core - called from non-owner thread. Forcing stop flags only.";
        for (const auto& pTask : std::as_const(m_activeTaskList)) {
            pTask->m_stopFlag.store(true);
        }
        clearOwnerEvents();
        return;
    }

    // Remove queued tasks first: they never started.
    for (const auto& pQueuedTask : std::as_const(m_queuedTaskList)) {
        publishTerminated(*pQueuedTask);
    }
    m_queuedTaskList.clear();

    if (m_activeTaskList.empty()) {
        clearOwnerEvents();
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

    const auto waitStartedAt = std::chrono::steady_clock::now();
    constexpr TaskStopTimeout kDtorWaitMs = 2000;

    auto elapsedWaitMs = [&waitStartedAt]() -> TaskStopTimeout {
        return static_cast<TaskStopTimeout>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - waitStartedAt).count()
        );
    };

    while (!m_activeTaskList.empty() && elapsedWaitMs() < kDtorWaitMs) {
        processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (m_activeTaskList.empty()) {
        clearOwnerEvents();
        return;
    }

    // std::thread has no safe standard force-termination primitive. If tasks remain
    // active here, request the same timeout event path and then join before members
    // are destroyed, so worker lambdas cannot outlive Core internals.
    if (m_allowForceTermination) {
        const auto stubbornTasks = m_activeTaskList;
        for (const auto& pTask : stubbornTasks) {
            terminateTask(pTask);
        }
    } else {
        core_detail::logWarning() << "Core::~Core - force termination disabled. Active tasks may outlive shutdown window.";
    }

    while (!m_activeTaskList.empty() && elapsedWaitMs() < (kDtorWaitMs * 2)) {
        processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (!m_activeTaskList.empty()) {
        core_detail::logWarning() << "Core::~Core - joining active tasks after shutdown timeout:" << m_activeTaskList.size();
        const auto activeTasks = m_activeTaskList;
        for (const auto& pTask : activeTasks) {
            pTask->m_stopFlag.store(true);
            joinTaskThread(*pTask);
        }
        processEvents();
    }

    clearOwnerEvents();
}

inline bool Core::ensureCalledFromOwnerThread(const char* method) const {
    if (std::this_thread::get_id() == m_ownerThreadId) {
        return true;
    }
    core_detail::logWarning() << "Core::" << method << "- called from non-owner thread.";
    return false;
}

inline void Core::postToOwner(std::function<void()> event) {
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_eventQueue.push_back(std::move(event));
    }
}

inline void Core::scheduleOnOwnerAfter(TaskStopTimeout delayMs, std::function<void()> event) {
    const auto normalizedDelayMs = std::max(delayMs, static_cast<TaskStopTimeout>(0));
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_scheduledEvents.push_back(ScheduledOwnerEvent{
            std::chrono::steady_clock::now() + std::chrono::milliseconds(normalizedDelayMs),
            std::move(event)
        });
    }
}

inline void Core::clearOwnerEvents() {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_eventQueue.clear();
    m_scheduledEvents.clear();
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
}

inline void Core::publishFinished(const Task& task, TaskResult result) {
    const auto event = makeFinishedEvent(task, std::move(result));
    if (m_finishedCallback) {
        m_finishedCallback(event);
    }
}

inline void Core::publishTerminated(const Task& task) {
    const auto event = makeTerminatedEvent(task);
    if (m_terminatedCallback) {
        m_terminatedCallback(event);
    }
}

inline void Core::publishStopRequested(const Task& task) {
    const auto event = makeStopRequestedEvent(task);
    if (m_stopRequestedCallback) {
        m_stopRequestedCallback(event);
    }
}

inline void Core::publishStopTimedOut(const Task& task, TaskStopTimeout timeout) {
    const auto event = makeStopTimedOutEvent(task, timeout);
    if (m_stopTimedOutCallback) {
        m_stopTimedOutCallback(event);
    }
}

template <typename R, typename... Args>
void Core::registerTask(TaskType taskType, std::function<R(Args...)> taskFunction, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    if (!ensureCalledFromOwnerThread("registerTask")) {
        throw std::logic_error("Core::registerTask must be called from the owner thread");
    }

    std::function<TaskResult(std::remove_reference_t<Args>...)> f;
    if constexpr (std::is_void_v<R>) {
        f = [taskFunction](std::remove_reference_t<Args>... args) -> TaskResult {
            taskFunction(args...);
            return TaskResult{};
        };
    } else {
        f = [taskFunction](std::remove_reference_t<Args>... args) -> TaskResult {
            return TaskResult(taskFunction(args...));
        };
    }

    insertToTaskHash(taskType, std::move(f), taskGroup, taskStopTimeout);
}

template <typename R, typename... Args>
void Core::registerTask(TaskType taskType, R (*taskFunction)(Args...), TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    // Use deduction guide for std::function
    registerTask(taskType, std::function(taskFunction), taskGroup, taskStopTimeout);
}

template <typename Class, typename R, typename... Args>
void Core::registerTask(TaskType taskType, R (Class::*taskMethod)(Args...), Class* taskObj, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    if (taskObj == nullptr) {
        core_detail::logWarning() << "Core::registerTask - task object is null for task type:" << taskType;
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
        core_detail::logWarning() << "Core::registerTask - task object is null for task type:" << taskType;
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
            core_detail::logWarning() << "Core::unregisterTask - Cannot unregister active task type:" << taskType;
            return false;
        }
    }
    for (const auto& pTask : std::as_const(m_queuedTaskList)) {
        if (pTask->m_type == taskType) {
            core_detail::logWarning() << "Core::unregisterTask - Cannot unregister queued task type:" << taskType;
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
        core_detail::logWarning() << "Core::addTask - Task not registered for type:" << taskType;
        throw std::logic_error("Task not registered");
    }

    try {
        const auto& taskInfo = taskInfoIt->second;
        auto storedFuncAny = taskInfo.m_function;
        auto taskFunction = std::any_cast<std::function<TaskResult(Args...)>>(storedFuncAny);
        TaskArgs stdArgs = core_detail::makeTaskArgs(args...);

        auto taskFunctionBound = std::bind(taskFunction, args...);
        auto pTask = std::make_shared<Task>(
            std::move(taskFunctionBound),
            taskType,
            taskInfo.m_group,
            std::move(stdArgs)
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
        core_detail::logWarning() << "Core::addTask - Bad arguments or function signature mismatch for task type:" << taskType << e.what();
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
            core_detail::logWarning() << "Core::terminateTaskById - force termination is disabled. Requesting cooperative stop for task ID:" << id;
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

    // Calculating the maximum stop timeout among active tasks
    TaskStopTimeout maxTimeout = 0;
    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        auto taskInfoIt = m_taskHash.find(pTask->m_type);
        if (taskInfoIt != m_taskHash.cend()) {
            maxTimeout = std::max(maxTimeout, taskInfoIt->second.m_stopTimeout);
        } else {
            maxTimeout = std::max(maxTimeout, static_cast<TaskStopTimeout>(kDefaultStopTimeout));
            core_detail::logWarning() << "Core::stopTasks - Missing registration for active task type:" << pTask->m_type;
        }
    }

    // Requesting to stop all active tasks
    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        stopTask(pTask);
    }

    m_blockStartTask.store(true);
    auto resumeWhenIdle = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> weakResumeWhenIdle = resumeWhenIdle;
    *resumeWhenIdle = [this, maxTimeout, weakResumeWhenIdle]() {
        if (isIdle()) {
            m_blockStartTask.store(false);
            startQueuedTask();
            return;
        }
        if (auto resumeWhenIdle = weakResumeWhenIdle.lock()) {
            scheduleOnOwnerAfter(maxTimeout, [resumeWhenIdle]() {
                (*resumeWhenIdle)();
            });
        }
    };

    scheduleOnOwnerAfter(maxTimeout, [resumeWhenIdle]() {
        (*resumeWhenIdle)();
    });
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
    if (pTask) {
        joinTaskThread(*pTask);
    }
    m_activeTaskList.erase(std::remove(m_activeTaskList.begin(), m_activeTaskList.end(), pTask), m_activeTaskList.end());
}

inline void Core::joinTaskThread(Task& task) {
    if (!task.m_thread.joinable()) {
        return;
    }
    if (task.m_thread.get_id() == std::this_thread::get_id()) {
        return;
    }
    task.m_thread.join();
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

    core_detail::logWarning() << "Task" << pTask->m_id
                              << "force termination is not supported by the std::thread backend;"
                              << "waiting for cooperative stop";

    const auto startedAt = std::chrono::steady_clock::now();
    auto checker = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> weakChecker = checker;

    *checker = [this, pTask, timeout, startedAt, weakChecker]() {
        if (pTask->m_state == TaskState::Inactive
            || pTask->m_state == TaskState::Finished
            || pTask->m_state == TaskState::Terminated) {
            return; // already finished/terminated by another path
        }

        if (pTask->m_threadExited.load()) {
            return;
        }

        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt
        ).count();

        if (elapsedMs >= timeout) {
            pTask->m_state = TaskState::StopTimedOut;
            core_detail::logWarning() << "Task" << pTask->m_id
                       << "did not stop cooperatively within timeout" << timeout << "ms";
            publishStopTimedOut(*pTask, timeout);
            return;
        }

        if (auto checker = weakChecker.lock()) {
            scheduleOnOwnerAfter(20, [checker]() {
                (*checker)();
            });
        }
    };

    scheduleOnOwnerAfter(0, [checker]() {
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
        core_detail::logWarning() << "Core::stopTask - Missing registration for active task type:" << pTask->m_type;
    }

    scheduleOnOwnerAfter(timeout, [this, pTask, timeout]() {
        switch (pTask->m_state) {
        case TaskState::Finished:
            core_detail::logDebug() << "Task" << pTask->m_id << "was successfully stopped";
            break;
        case TaskState::Terminated:
            core_detail::logDebug() << "Task" << pTask->m_id << "was terminated";
            break;
        case TaskState::StopTimedOut:
            core_detail::logDebug() << "Task" << pTask->m_id << "stop already timed out";
            break;
        case TaskState::StopRequested:
        case TaskState::Active:
            if (!m_allowForceTermination) {
                pTask->m_state = TaskState::StopTimedOut;
                core_detail::logWarning() << "Task" << pTask->m_id << "stop timed out; force termination is disabled";
                publishStopTimedOut(*pTask, timeout);
                break;
            }
            core_detail::logDebug() << "Task" << pTask->m_id << "was not stopped, terminating";
            terminateTask(pTask);
            if (pTask->m_state == TaskState::Active || pTask->m_state == TaskState::StopRequested) {
                core_detail::logDebug() << "Task" << pTask->m_id << "terminate request is in progress";
            }
            break;
        default:
            core_detail::logDebug() << "Task" << pTask->m_id << "unexpected state";
            break;
        }
    });
}

inline void Core::startTask(std::shared_ptr<Core::Task> pTask) {
    m_activeTaskList.push_back(pTask);
    pTask->m_state = TaskState::Active;
    pTask->m_threadExited.store(false);

    try {
        pTask->m_thread = std::thread([this, pTask]() {
            core_detail::g_currentStopFlag = &pTask->m_stopFlag;
            TaskResult result;
            try {
                result = pTask->m_functionBound();
            } catch (const std::exception& e) {
                core_detail::logWarning() << "Task" << pTask->m_id << "threw exception:" << e.what();
            } catch (...) {
                core_detail::logWarning() << "Task" << pTask->m_id << "threw unknown exception";
            }
            core_detail::g_currentStopFlag = nullptr;
            pTask->m_threadExited.store(true);

            postToOwner([this, pTask, result = std::move(result)]() mutable {
                if (pTask->m_state == TaskState::Terminated) {
                    removeActiveTask(pTask);
                    startQueuedTask();
                    return;
                }
                pTask->m_state = TaskState::Finished;
                publishFinished(*pTask, std::move(result));
                removeActiveTask(pTask);
                startQueuedTask();
            });
        });
    } catch (const std::system_error& e) {
        core_detail::logWarning() << "Core::startTask - Failed to create thread for task ID:" << pTask->m_id << e.what();
        removeActiveTask(pTask);
        startQueuedTask();
        return;
    }

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
void Core::insertToTaskHash(TaskType taskType, std::function<TaskResult(Args...)> taskFunction, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    if (m_taskHash.find(taskType) != m_taskHash.cend()) {
        core_detail::logWarning() << "Core::registerTask - Task type is already registered:" << taskType;
        throw std::logic_error("Task type is already registered");
    }

    TaskStopTimeout normalizedStopTimeout = taskStopTimeout;
    if (normalizedStopTimeout < 0) {
        core_detail::logWarning() << "Core::registerTask - Negative stop timeout for task type:"
                   << taskType << ". Using default:" << kDefaultStopTimeout;
        normalizedStopTimeout = kDefaultStopTimeout;
    }

    m_taskHash.emplace(taskType, TaskInfo{taskFunction, taskGroup, normalizedStopTimeout});
}

#endif // CORE_H

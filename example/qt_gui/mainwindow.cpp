#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QDebug>
#include <QIntValidator>
#include <QListWidgetItem>
#include <QMenu>
#include <QSplitter>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#include <climits>

namespace {
constexpr int RoleTaskId = Qt::UserRole + 1;
constexpr int RoleTaskType = Qt::UserRole + 2;
constexpr int RoleTaskGroup = Qt::UserRole + 3;

int calculateSum(int a, int b, int c)
{
    qDebug() << "calculateSum() executed with args:" << a << b << c;
    return a + b + c;
}

QString createMessage(int value, const QString& text)
{
    qDebug() << "createMessage() executed with args:" << value << text;
    return QString("%1: %2").arg(text).arg(value);
}

void performAction()
{
    qDebug() << "performAction() executed.";
}

struct MultiplyFunctor {
    int factor = 1;

    int operator()(int x, int y)
    {
        qDebug() << "MultiplyFunctor::operator() executed with args:" << x << y << "and factor:" << factor;
        return (x + y) * factor;
    }
};

QString logColor(MainWindow::LogKind kind)
{
    switch (kind) {
    case MainWindow::LogKind::Started: return "#2e7d32";
    case MainWindow::LogKind::Finished: return "#1565c0";
    case MainWindow::LogKind::StopRequested: return "#ef6c00";
    case MainWindow::LogKind::StopTimedOut: return "#c62828";
    case MainWindow::LogKind::Terminated: return "#6a1b9a";
    case MainWindow::LogKind::Info: return "#37474f";
    }
    return "#37474f";
}

QString logTag(MainWindow::LogKind kind)
{
    switch (kind) {
    case MainWindow::LogKind::Started: return "STARTED";
    case MainWindow::LogKind::Finished: return "FINISHED";
    case MainWindow::LogKind::StopRequested: return "STOP REQUESTED";
    case MainWindow::LogKind::StopTimedOut: return "STOP TIMED OUT";
    case MainWindow::LogKind::Terminated: return "TERMINATED";
    case MainWindow::LogKind::Info: return "INFO";
    }
    return "INFO";
}
}

int Calculator::add(int a, int b)
{
    qDebug() << "Calculator::add() executed with args:" << a << b;
    return a + b;
}

int Calculator::multiply(int a, int b) const
{
    qDebug() << "Calculator::multiply() executed with args:" << a << b;
    return a * b;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_adapter(this)
    , m_visibleKinds({
          LogKind::Started,
          LogKind::Finished,
          LogKind::StopRequested,
          LogKind::StopTimedOut,
          LogKind::Terminated,
          LogKind::Info
      })
{
    ui->setupUi(this);
    ui->menuBar->setHidden(true);
    ui->mainToolBar->setHidden(true);

    m_adapter.core().setAllowForceTermination(true);

    if (auto* splitter = findChild<QSplitter*>("splitterMainPanels")) {
        splitter->setStretchFactor(0, 2);
        splitter->setStretchFactor(1, 3);
        splitter->setSizes({240, 320});
    }

    setupLogFilter();
    setupTaskContextMenu();
    setupInputs();
    registerTasks();
    connectAdapterSignals();
    connectControls();

    auto* eventPump = new QTimer(this);
    connect(eventPump, &QTimer::timeout, this, [this]() {
        m_adapter.processEvents();
    });
    eventPump->start(10);

    addLog(LogKind::Info, QString("Qt GUI example uses CoreQtAdapter; core.h remains std-only."));
    addLog(LogKind::Info, QString("Force termination is %1.")
                              .arg(m_adapter.core().allowForceTermination() ? "enabled" : "disabled"));

    addInitialTasks();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupLogFilter()
{
    auto* filterButton = findChild<QToolButton*>("toolButtonLogFilter");
    if (!filterButton) {
        return;
    }

    filterButton->setPopupMode(QToolButton::InstantPopup);

    auto* menu = new QMenu(filterButton);
    auto makeFilterAction = [this, menu](const QString& title, LogKind kind) {
        QAction* action = menu->addAction(title);
        action->setCheckable(true);
        action->setChecked(true);
        connect(action, &QAction::toggled, this, [this, kind](bool checked) {
            if (checked) {
                m_visibleKinds.insert(kind);
            } else {
                m_visibleKinds.remove(kind);
            }
            rebuildLog();
        });
    };

    makeFilterAction("Started", LogKind::Started);
    makeFilterAction("Finished", LogKind::Finished);
    makeFilterAction("Stop Requested", LogKind::StopRequested);
    makeFilterAction("Stop Timed Out", LogKind::StopTimedOut);
    makeFilterAction("Terminated", LogKind::Terminated);
    makeFilterAction("Info", LogKind::Info);
    menu->addSeparator();
    menu->addAction("Clear Log", this, [this]() {
        m_logEntries.clear();
        ui->textEdit->clear();
    });

    filterButton->setMenu(menu);
}

void MainWindow::setupTaskContextMenu()
{
    ui->listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = ui->listWidget->itemAt(pos);
        if (!item) {
            return;
        }

        const auto taskId = static_cast<TaskId>(item->data(RoleTaskId).toLongLong());
        const auto taskType = static_cast<TaskType>(item->data(RoleTaskType).toInt());
        const auto taskGroup = static_cast<TaskGroup>(item->data(RoleTaskGroup).toInt());

        QMenu menu(this);
        menu.addAction("Cancel This Task", this, [this, taskId]() {
            m_adapter.core().cancelTaskById(taskId);
        });
        menu.addAction("Terminate This Task", this, [this, taskId]() {
            m_adapter.core().terminateTaskById(taskId);
        });
        menu.addSeparator();
        menu.addAction("Cancel By Type", this, [this, taskType]() {
            m_adapter.core().cancelTaskByType(taskType);
        });
        menu.addAction("Cancel Group (Active)", this, [this, taskGroup]() {
            m_adapter.core().cancelTasksByGroup(taskGroup, false);
        });
        menu.addAction("Cancel Group (All)", this, [this, taskGroup]() {
            m_adapter.core().cancelTasksByGroup(taskGroup, true);
        });

        menu.exec(ui->listWidget->viewport()->mapToGlobal(pos));
    });
}

void MainWindow::setupInputs()
{
    ui->lineEditStopTaskId->setValidator(new QIntValidator(0, INT_MAX, this));
    ui->lineEditStopTaskType->setValidator(new QIntValidator(0, INT_MAX, this));
    ui->lineEditStopTaskGroup->setValidator(new QIntValidator(0, INT_MAX, this));
}

void MainWindow::registerTasks()
{
    auto& core = m_adapter.core();

    core.registerTask(TASK_STOPPABLE, [this]() {
        auto* stopFlag = m_adapter.core().stopTaskFlag();
        for (int counter = 0; counter < 10 && stopFlag && !stopFlag->load(); ++counter) {
            qDebug() << "TASK_STOPPABLE - Iteration:" << counter << "on thread:" << QThread::currentThread();
            QThread::msleep(1000);
        }
    }, 0);

    core.registerTask(TASK_NON_COOPERATIVE, []() {
        qDebug() << "TASK_NON_COOPERATIVE - Starting long-running operation...";
        for (int i = 0; i < 100; ++i) {
            qDebug() << "TASK_NON_COOPERATIVE - Working... iteration" << i;
            QThread::msleep(500);
        }
    }, 1, 2000);

    core.registerTask(TASK_STOPPABLE_WITH_ARG, [this](int durationSeconds) {
        auto* stopFlag = m_adapter.core().stopTaskFlag();
        for (int remaining = durationSeconds; remaining > 0 && stopFlag && !stopFlag->load(); --remaining) {
            qDebug() << "TASK_STOPPABLE_WITH_ARG - Remaining:" << remaining;
            QThread::msleep(1000);
        }
    }, 2);

    core.registerTask(TASK_CLASS_METHOD, &Calculator::add, &m_calculator);
    core.registerTask(TASK_CLASS_CONST_METHOD, &Calculator::multiply, &m_calculator);
    core.registerTask(TASK_FREE_FUNCTION_RETURN, calculateSum);
    core.registerTask(TASK_STRING_RETURN, createMessage);
    core.registerTask(TASK_VOID_FUNCTION, performAction);
    core.registerTask(TASK_FUNCTOR, MultiplyFunctor{5});
    core.registerTask(TASK_LAMBDA, [](int x) -> int {
        qDebug() << "TASK_LAMBDA executed with arg:" << x;
        return x * 10;
    });
}

void MainWindow::connectAdapterSignals()
{
    connect(&m_adapter, &CoreQtAdapter::startedTask, this, [this](TaskId id, TaskType type, const QVariantList&) {
        const QString info = formatTaskInfo(id, type);
        addLog(LogKind::Started, QString("Task (%1) started.").arg(info));

        auto* item = new QListWidgetItem(info);
        bool ok = false;
        const TaskGroup group = m_adapter.core().groupByTask(type, &ok);
        item->setData(RoleTaskId, static_cast<qlonglong>(id));
        item->setData(RoleTaskType, type);
        item->setData(RoleTaskGroup, ok ? group : -1);
        ui->listWidget->addItem(item);
    });

    connect(&m_adapter, &CoreQtAdapter::finishedTask, this,
            [this](TaskId id, TaskType type, const QVariantList&, const QVariant& result) {
        addLog(LogKind::Finished, QString("Task (%1) finished with result: %2.")
                                      .arg(formatTaskInfo(id, type), result.toString()));
        removeTaskItemById(id);
    });

    connect(&m_adapter, &CoreQtAdapter::terminatedTask, this, [this](TaskId id, TaskType type, const QVariantList&) {
        addLog(LogKind::Terminated, QString("Task (%1) was terminated.").arg(formatTaskInfo(id, type)));
        removeTaskItemById(id);
    });

    connect(&m_adapter, &CoreQtAdapter::stopRequestedTask, this, [this](TaskId id, TaskType type, const QVariantList&) {
        addLog(LogKind::StopRequested, QString("Task (%1) stop requested.").arg(formatTaskInfo(id, type)));
    });

    connect(&m_adapter, &CoreQtAdapter::stopTimedOutTask, this,
            [this](TaskId id, TaskType type, const QVariantList&, TaskStopTimeout timeout) {
        addLog(LogKind::StopTimedOut, QString("Task (%1) stop timed out after %2 ms.")
                                      .arg(formatTaskInfo(id, type))
                                      .arg(timeout));
    });
}

void MainWindow::connectControls()
{
    connect(ui->pushButtonAddTask0, &QPushButton::clicked, this, [this]() {
        m_adapter.core().addTask(TASK_STOPPABLE);
    });

    connect(ui->pushButtonAddTask1, &QPushButton::clicked, this, [this]() {
        m_adapter.core().addTask(TASK_NON_COOPERATIVE);
    });

    connect(ui->pushButtonAddTask2, &QPushButton::clicked, this, [this]() {
        m_adapter.core().addTask(TASK_STOPPABLE_WITH_ARG, 5);
    });

    connect(ui->pushButtonStopTaskById, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const auto id = static_cast<TaskId>(ui->lineEditStopTaskId->text().toLongLong(&ok));
        if (ok) {
            m_adapter.core().cancelTaskById(id);
        }
    });

    connect(ui->pushButtonStopTaskByType, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const auto type = static_cast<TaskType>(ui->lineEditStopTaskType->text().toInt(&ok));
        if (ok) {
            m_adapter.core().cancelTaskByType(type);
        }
    });

    connect(ui->pushButtonStopTaskByGroup, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const auto group = static_cast<TaskGroup>(ui->lineEditStopTaskGroup->text().toInt(&ok));
        if (ok) {
            m_adapter.core().cancelTasksByGroup(group, true);
        }
    });

    connect(ui->pushButtonStopTasks, &QPushButton::clicked, this, [this]() {
        QMenu menu(this);
        QAction* allowForceTerminate = menu.addAction("Allow Force Termination");
        allowForceTerminate->setCheckable(true);
        allowForceTerminate->setChecked(m_adapter.core().allowForceTermination());
        menu.addSeparator();

        QAction* stopActive = menu.addAction("Cancel Active Tasks");
        QAction* stopAll = menu.addAction("Cancel ALL Tasks (Active + Queued)");
        menu.addSeparator();
        QAction* stopGroupActive = menu.addAction("Cancel Active Tasks By Group");
        QAction* stopGroupAll = menu.addAction("Cancel ALL Tasks By Group (Active + Queued)");

        QAction* selected = menu.exec(ui->pushButtonStopTasks->mapToGlobal(QPoint(0, ui->pushButtonStopTasks->height())));
        if (!selected) {
            return;
        }

        if (selected == allowForceTerminate) {
            const bool enabled = allowForceTerminate->isChecked();
            m_adapter.core().setAllowForceTermination(enabled);
            addLog(LogKind::Info, QString("Force termination %1.").arg(enabled ? "enabled" : "disabled"));
            return;
        }

        if (selected == stopActive) {
            m_adapter.core().cancelTasks();
            return;
        }

        if (selected == stopAll) {
            m_adapter.core().cancelAllTasks();
            return;
        }

        bool ok = false;
        const auto group = static_cast<TaskGroup>(ui->lineEditStopTaskGroup->text().toInt(&ok));
        if (!ok) {
            return;
        }

        if (selected == stopGroupActive) {
            m_adapter.core().cancelTasksByGroup(group, false);
        } else if (selected == stopGroupAll) {
            m_adapter.core().cancelTasksByGroup(group, true);
        }
    });
}

void MainWindow::addInitialTasks()
{
    m_adapter.core().addTask(TASK_CLASS_METHOD, 10, 20);
    m_adapter.core().addTask(TASK_CLASS_CONST_METHOD, 10, 20);
    m_adapter.core().addTask(TASK_FREE_FUNCTION_RETURN, 1, 2, 3);
    m_adapter.core().addTask(TASK_STRING_RETURN, 100, QString("Message"));
    m_adapter.core().addTask(TASK_VOID_FUNCTION);
    m_adapter.core().addTask(TASK_FUNCTOR, 7, 8);
    m_adapter.core().addTask(TASK_LAMBDA, 42);
}

void MainWindow::addLog(LogKind kind, const QString& text)
{
    m_logEntries.append(LogEntry{kind, text});
    if (!m_visibleKinds.contains(kind)) {
        return;
    }

    const QString html = QString("<span style=\"color:%1;\"><b>[%2]</b> %3</span>")
                             .arg(logColor(kind), logTag(kind), text.toHtmlEscaped());
    ui->textEdit->append(html);
}

void MainWindow::rebuildLog()
{
    ui->textEdit->clear();
    for (const auto& entry : m_logEntries) {
        if (m_visibleKinds.contains(entry.kind)) {
            const QString html = QString("<span style=\"color:%1;\"><b>[%2]</b> %3</span>")
                                     .arg(logColor(entry.kind), logTag(entry.kind), entry.text.toHtmlEscaped());
            ui->textEdit->append(html);
        }
    }
}

void MainWindow::removeTaskItemById(TaskId taskId)
{
    for (int i = 0; i < ui->listWidget->count(); ++i) {
        QListWidgetItem* item = ui->listWidget->item(i);
        if (item && item->data(RoleTaskId).toLongLong() == taskId) {
            delete ui->listWidget->takeItem(i);
            return;
        }
    }
}

QString MainWindow::formatTaskInfo(TaskId id, TaskType type)
{
    bool ok = false;
    const TaskGroup group = m_adapter.core().groupByTask(type, &ok);
    return QString("ID: %1, Type: %2, Group: %3")
        .arg(id)
        .arg(type)
        .arg(ok ? group : -1);
}

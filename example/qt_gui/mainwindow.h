#ifndef EXAMPLE_QT_GUI_MAINWINDOW_H
#define EXAMPLE_QT_GUI_MAINWINDOW_H

#include "CoreQtAdapter.h"

#include <QMainWindow>
#include <QSet>
#include <QString>
#include <QVector>

namespace Ui {
class MainWindow;
}

enum Tasks {
    TASK_STOPPABLE = 0,
    TASK_NON_COOPERATIVE,
    TASK_STOPPABLE_WITH_ARG,
    TASK_CLASS_METHOD,
    TASK_CLASS_CONST_METHOD,
    TASK_FREE_FUNCTION_RETURN,
    TASK_STRING_RETURN,
    TASK_VOID_FUNCTION,
    TASK_FUNCTOR,
    TASK_LAMBDA
};

class Calculator {
public:
    int add(int a, int b);
    int multiply(int a, int b) const;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    enum class LogKind {
        Started,
        Finished,
        StopRequested,
        StopTimedOut,
        Terminated,
        Info
    };

private:
    struct LogEntry {
        LogKind kind;
        QString text;
    };

    void setupLogFilter();
    void setupTaskContextMenu();
    void setupInputs();
    void registerTasks();
    void connectAdapterSignals();
    void connectControls();
    void addInitialTasks();
    void addLog(LogKind kind, const QString& text);
    void rebuildLog();
    void removeTaskItemById(TaskId taskId);
    QString formatTaskInfo(TaskId id, TaskType type);

    Ui::MainWindow* ui = nullptr;
    CoreQtAdapter m_adapter;
    Calculator m_calculator;
    QVector<LogEntry> m_logEntries;
    QSet<int> m_visibleKinds;
};

#endif // EXAMPLE_QT_GUI_MAINWINDOW_H

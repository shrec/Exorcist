#pragma once

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QProcess;
class QPlainTextEdit;
class QPushButton;
class TaskDiscovery;
struct TaskEntry;

/// Panel that shows discovered tasks and lets the user run them.
class TaskRunnerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TaskRunnerPanel(TaskDiscovery *discovery, QWidget *parent = nullptr);

    void setWorkingDirectory(const QString &dir) { m_workDir = dir; }
    void refresh();

    void runTask(const TaskEntry &task);
    void runLastTask();
    void stopTask();
    bool isRunning() const;

signals:
    void taskStarted(const QString &name);
    void taskFinished(const QString &name, int exitCode);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);

private:
    void buildUi();
    void populateTree();
    void appendOutput(const QString &text, bool isError = false);

    TaskDiscovery *m_discovery;
    QTreeWidget   *m_tree       = nullptr;
    QPlainTextEdit *m_output    = nullptr;
    QPushButton   *m_runBtn     = nullptr;
    QPushButton   *m_stopBtn    = nullptr;
    QPushButton   *m_refreshBtn = nullptr;
    QProcess      *m_process    = nullptr;
    QString        m_workDir;
    TaskEntry     *m_lastTask   = nullptr;
};

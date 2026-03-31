#include "taskrunnerpanel.h"
#include "taskdiscovery.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

TaskRunnerPanel::TaskRunnerPanel(TaskDiscovery *discovery, QWidget *parent)
    : QWidget(parent)
    , m_discovery(discovery)
{
    buildUi();
    connect(m_discovery, &TaskDiscovery::tasksChanged, this, &TaskRunnerPanel::populateTree);
}

void TaskRunnerPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 2, 4, 2);

    m_runBtn = new QPushButton(tr("Run"));
    m_runBtn->setToolTip(tr("Run selected task"));
    m_runBtn->setEnabled(false);

    m_stopBtn = new QPushButton(tr("Stop"));
    m_stopBtn->setToolTip(tr("Stop running task"));
    m_stopBtn->setEnabled(false);

    m_refreshBtn = new QPushButton(tr("Refresh"));
    m_refreshBtn->setToolTip(tr("Rediscover tasks"));

    toolbar->addWidget(m_runBtn);
    toolbar->addWidget(m_stopBtn);
    toolbar->addWidget(m_refreshBtn);
    toolbar->addStretch();
    root->addLayout(toolbar);

    // Splitter: tree | output
    auto *splitter = new QSplitter(Qt::Vertical, this);

    // Task tree
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({ tr("Task"), tr("Source"), tr("Command") });
    m_tree->header()->setStretchLastSection(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    splitter->addWidget(m_tree);

    // Output log
    m_output = new QPlainTextEdit;
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(10000);
    m_output->setFont(QFont(QStringLiteral("Consolas"), 9));
    splitter->addWidget(m_output);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter);

    // Connections
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &TaskRunnerPanel::onItemDoubleClicked);

    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem *item) {
        m_runBtn->setEnabled(item && item->data(0, Qt::UserRole).isValid());
    });

    connect(m_runBtn, &QPushButton::clicked, this, [this]() {
        auto *item = m_tree->currentItem();
        if (!item) return;
        const int idx = item->data(0, Qt::UserRole).toInt();
        const auto tasks = m_discovery->tasks();
        if (idx >= 0 && idx < tasks.size())
            runTask(tasks.at(idx));
    });

    connect(m_stopBtn, &QPushButton::clicked, this, &TaskRunnerPanel::stopTask);
    connect(m_refreshBtn, &QPushButton::clicked, this, &TaskRunnerPanel::refresh);
}

void TaskRunnerPanel::populateTree()
{
    m_tree->clear();

    // Group tasks by sourceType
    QMap<QString, QList<int>> groups;
    const auto tasks = m_discovery->tasks();
    for (int i = 0; i < tasks.size(); ++i)
        groups[tasks.at(i).sourceType].append(i);

    static const QMap<QString, QString> labels = {
        { QStringLiteral("taskfile"), QStringLiteral("Taskfile") },
        { QStringLiteral("justfile"), QStringLiteral("Just") },
        { QStringLiteral("makefile"), QStringLiteral("Make") },
        { QStringLiteral("npm"),      QStringLiteral("npm scripts") },
        { QStringLiteral("script"),   QStringLiteral("Scripts") },
    };

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const QString label = labels.value(it.key(), it.key());
        auto *group = new QTreeWidgetItem(m_tree, { label });
        group->setExpanded(true);

        for (int idx : it.value()) {
            const TaskEntry &te = tasks.at(idx);
            auto *item = new QTreeWidgetItem(group, { te.name, te.source, te.command });
            item->setData(0, Qt::UserRole, idx);
            if (!te.description.isEmpty())
                item->setToolTip(0, te.description);
        }
    }

    m_tree->expandAll();
}

void TaskRunnerPanel::refresh()
{
    m_discovery->refresh();
}

void TaskRunnerPanel::runTask(const TaskEntry &task)
{
    if (isRunning()) {
        stopTask();
    }

    m_output->clear();
    appendOutput(tr(">>> Running: %1\n").arg(task.command));

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
            appendOutput(QString::fromUtf8(m_process->readAllStandardOutput()));
        });
        connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
            appendOutput(QString::fromUtf8(m_process->readAllStandardError()), true);
        });
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus) {
            appendOutput(tr("\n>>> Task finished (exit code %1)\n").arg(exitCode));
            m_stopBtn->setEnabled(false);
            m_runBtn->setEnabled(true);
            // Emit with name stored in process property
            const QString name = m_process->property("taskName").toString();
            emit taskFinished(name, exitCode);
        });
    }

    m_process->setProperty("taskName", task.name);

    if (!m_workDir.isEmpty())
        m_process->setWorkingDirectory(m_workDir);

    // Store last task for "run last" feature
    delete m_lastTask;
    m_lastTask = new TaskEntry(task);

    m_stopBtn->setEnabled(true);
    m_runBtn->setEnabled(false);
    emit taskStarted(task.name);

    // Execute via shell
#ifdef Q_OS_WIN
    m_process->start(QStringLiteral("cmd.exe"), { QStringLiteral("/c"), task.command });
#else
    m_process->start(QStringLiteral("/bin/sh"), { QStringLiteral("-c"), task.command });
#endif
}

void TaskRunnerPanel::runLastTask()
{
    if (m_lastTask)
        runTask(*m_lastTask);
}

void TaskRunnerPanel::stopTask()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
    }
}

bool TaskRunnerPanel::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void TaskRunnerPanel::onItemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    const int idx = item->data(0, Qt::UserRole).toInt();
    const auto tasks = m_discovery->tasks();
    if (idx >= 0 && idx < tasks.size())
        runTask(tasks.at(idx));
}

void TaskRunnerPanel::appendOutput(const QString &text, bool isError)
{
    Q_UNUSED(isError)
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    m_output->moveCursor(QTextCursor::End);
}

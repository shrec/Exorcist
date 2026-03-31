#include "devopspanel.h"
#include "servicediscovery.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

DevOpsPanel::DevOpsPanel(ServiceDiscovery *discovery, QWidget *parent)
    : QWidget(parent)
    , m_discovery(discovery)
{
    buildUi();
    connect(m_discovery, &ServiceDiscovery::servicesChanged,
            this, &DevOpsPanel::populateTree);
}

void DevOpsPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 2, 4, 2);

    auto *refreshBtn = new QPushButton(tr("Refresh"));
    refreshBtn->setToolTip(tr("Rescan workspace for DevOps tooling"));
    connect(refreshBtn, &QPushButton::clicked, this, &DevOpsPanel::refresh);

    m_stopBtn = new QPushButton(tr("Stop"));
    m_stopBtn->setToolTip(tr("Stop running command"));
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &DevOpsPanel::stopCommand);

    toolbar->addWidget(refreshBtn);
    toolbar->addWidget(m_stopBtn);
    toolbar->addStretch();
    root->addLayout(toolbar);

    // Splitter: services tree + output
    auto *splitter = new QSplitter(Qt::Vertical, this);

    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({ tr("Service"), tr("Config"), tr("Description") });
    m_tree->header()->setStretchLastSection(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    splitter->addWidget(m_tree);

    m_output = new QPlainTextEdit;
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(10000);
    m_output->setFont(QFont(QStringLiteral("Consolas"), 9));
    splitter->addWidget(m_output);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter);

    // Double-click to open config file
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem *item, int) {
        if (!item) return;
        const QString path = item->data(1, Qt::UserRole).toString();
        if (!path.isEmpty()) {
            // TODO: could emit a signal to open the file in the editor
        }
    });
}

void DevOpsPanel::populateTree()
{
    m_tree->clear();

    static const QMap<int, QString> typeIcons = {
        { DevOpsService::Docker,        QStringLiteral("Docker") },
        { DevOpsService::DockerCompose, QStringLiteral("Docker Compose") },
        { DevOpsService::Terraform,     QStringLiteral("Terraform") },
        { DevOpsService::Ansible,       QStringLiteral("Ansible") },
        { DevOpsService::Kubernetes,    QStringLiteral("Kubernetes") },
        { DevOpsService::GitHubActions, QStringLiteral("GitHub Actions") },
    };

    for (const auto &svc : m_discovery->services()) {
        const QString typeName = typeIcons.value(svc.type, QStringLiteral("Unknown"));
        auto *item = new QTreeWidgetItem(m_tree, {
            svc.name.isEmpty() ? typeName : svc.name,
            QFileInfo(svc.configFile).fileName(),
            svc.description
        });
        item->setData(1, Qt::UserRole, svc.configFile);
    }

    if (m_discovery->services().isEmpty()) {
        auto *item = new QTreeWidgetItem(m_tree, {
            tr("No DevOps services detected"),
            QString(),
            tr("Add Dockerfile, docker-compose.yml, *.tf, or ansible.cfg")
        });
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    }
}

void DevOpsPanel::refresh()
{
    m_discovery->refresh();
}

void DevOpsPanel::runCommand(const QString &label, const QString &command)
{
    if (isRunning())
        stopCommand();

    m_output->clear();
    appendOutput(QStringLiteral(">>> %1\n>>> %2\n\n").arg(label, command));

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
            appendOutput(QStringLiteral("\n>>> Finished (exit code %1)\n").arg(exitCode));
            m_stopBtn->setEnabled(false);
            const QString lbl = m_process->property("cmdLabel").toString();
            emit commandFinished(lbl, exitCode);
        });
    }

    m_process->setProperty("cmdLabel", label);
    if (!m_workDir.isEmpty())
        m_process->setWorkingDirectory(m_workDir);

    m_stopBtn->setEnabled(true);
    emit commandStarted(label);

#ifdef Q_OS_WIN
    m_process->start(QStringLiteral("cmd.exe"), { QStringLiteral("/c"), command });
#else
    m_process->start(QStringLiteral("/bin/sh"), { QStringLiteral("-c"), command });
#endif
}

void DevOpsPanel::stopCommand()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
    }
}

bool DevOpsPanel::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void DevOpsPanel::appendOutput(const QString &text, bool isError)
{
    Q_UNUSED(isError)
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    m_output->moveCursor(QTextCursor::End);
}

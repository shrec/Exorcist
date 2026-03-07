#include "mcppanel.h"
#include "mcpclient.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

McpPanel::McpPanel(McpClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    auto *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(4, 4, 4, 4);

    m_statusLabel = new QLabel(tr("MCP Servers"), this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "font-weight:bold; padding:4px; color:#ccc;"));
    mainLay->addWidget(m_statusLabel);

    auto *splitter = new QSplitter(Qt::Vertical, this);

    // ── Server list ───────────────────────────────────────────
    auto *serverWidget = new QWidget(this);
    auto *serverLay = new QVBoxLayout(serverWidget);
    serverLay->setContentsMargins(0, 0, 0, 0);
    serverLay->addWidget(new QLabel(tr("Servers:"), serverWidget));

    m_serverList = new QListWidget(serverWidget);
    m_serverList->setStyleSheet(QStringLiteral(
        "QListWidget { background:#1e1e1e; color:#ccc; border:1px solid #333; }"
        "QListWidget::item:selected { background:#264f78; }"));
    serverLay->addWidget(m_serverList);

    // Add server form
    auto *formLay = new QHBoxLayout;
    m_nameInput = new QLineEdit(serverWidget);
    m_nameInput->setPlaceholderText(tr("Name"));
    m_nameInput->setMaximumWidth(100);
    formLay->addWidget(m_nameInput);

    m_cmdInput = new QLineEdit(serverWidget);
    m_cmdInput->setPlaceholderText(tr("Command (e.g. npx)"));
    formLay->addWidget(m_cmdInput);

    m_argsInput = new QLineEdit(serverWidget);
    m_argsInput->setPlaceholderText(tr("Args (space-separated)"));
    formLay->addWidget(m_argsInput);
    serverLay->addLayout(formLay);

    auto *btnLay = new QHBoxLayout;
    m_addBtn = new QPushButton(tr("Add"), serverWidget);
    m_removeBtn = new QPushButton(tr("Remove"), serverWidget);
    m_connectBtn = new QPushButton(tr("Connect"), serverWidget);
    btnLay->addWidget(m_addBtn);
    btnLay->addWidget(m_removeBtn);
    btnLay->addWidget(m_connectBtn);
    btnLay->addStretch();
    serverLay->addLayout(btnLay);
    splitter->addWidget(serverWidget);

    // ── Tool list ─────────────────────────────────────────────
    auto *toolWidget = new QWidget(this);
    auto *toolLay = new QVBoxLayout(toolWidget);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->addWidget(new QLabel(tr("Discovered Tools:"), toolWidget));

    m_toolList = new QListWidget(toolWidget);
    m_toolList->setStyleSheet(QStringLiteral(
        "QListWidget { background:#1e1e1e; color:#ccc; border:1px solid #333; }"));
    toolLay->addWidget(m_toolList);
    splitter->addWidget(toolWidget);

    mainLay->addWidget(splitter);

    // ── Connections ───────────────────────────────────────────
    connect(m_addBtn, &QPushButton::clicked, this, &McpPanel::addServer);
    connect(m_removeBtn, &QPushButton::clicked, this, &McpPanel::removeSelected);
    connect(m_connectBtn, &QPushButton::clicked, this, [this]() {
        auto *item = m_serverList->currentItem();
        if (!item) return;
        const QString name = item->text().section(QLatin1String(" "), 0, 0);
        if (m_client->isConnected(name)) {
            m_client->disconnectServer(name);
        } else {
            m_client->connectServer(name);
        }
        refreshList();
    });

    connect(m_client, &McpClient::serverConnected, this, [this](const QString &) {
        refreshList();
        m_statusLabel->setText(tr("MCP — %1 server(s) connected")
                                   .arg(m_client->connectedServers().size()));
    });
    connect(m_client, &McpClient::serverDisconnected, this, [this](const QString &) {
        refreshList();
    });
    connect(m_client, &McpClient::serverError, this,
            [this](const QString &name, const QString &err) {
        m_statusLabel->setText(tr("Error (%1): %2").arg(name, err));
    });
    connect(m_client, &McpClient::toolsDiscovered, this,
            [this](const QString &, const QList<McpToolInfo> &) {
        refreshList();
        emit toolsChanged();
    });
}

void McpPanel::addServer()
{
    const QString name = m_nameInput->text().trimmed();
    const QString cmd  = m_cmdInput->text().trimmed();
    const QString args = m_argsInput->text().trimmed();

    if (name.isEmpty() || cmd.isEmpty()) {
        m_statusLabel->setText(tr("Name and command are required"));
        return;
    }

    McpServerConfig config;
    config.name    = name;
    config.command = cmd;
    config.args    = args.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    m_client->addServer(config);
    m_nameInput->clear();
    m_cmdInput->clear();
    m_argsInput->clear();
    refreshList();
}

void McpPanel::removeSelected()
{
    auto *item = m_serverList->currentItem();
    if (!item) return;
    const QString name = item->text().section(QLatin1String(" "), 0, 0);
    m_client->disconnectServer(name);
    refreshList();
}

void McpPanel::refreshList()
{
    m_serverList->clear();
    const QStringList connected = m_client->connectedServers();
    // We need to iterate all configured servers — use connectedServers + approach
    // For simplicity, just list connected ones and let add show all
    for (const QString &name : connected) {
        m_serverList->addItem(name + tr(" (connected)"));
    }

    m_toolList->clear();
    const QList<McpToolInfo> tools = m_client->allTools();
    for (const McpToolInfo &t : tools) {
        m_toolList->addItem(QStringLiteral("[%1] %2 — %3")
                                .arg(t.serverName, t.name, t.description));
    }
}

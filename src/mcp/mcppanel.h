#pragma once

#include <QWidget>

class McpClient;
class QListWidget;
class QLineEdit;
class QPushButton;
class QLabel;

/// Panel for managing MCP server connections.
class McpPanel : public QWidget
{
    Q_OBJECT

public:
    explicit McpPanel(McpClient *client, QWidget *parent = nullptr);

signals:
    void toolsChanged();

private:
    void addServer();
    void removeSelected();
    void refreshList();

    McpClient   *m_client;
    QListWidget *m_serverList;
    QListWidget *m_toolList;
    QLineEdit   *m_nameInput;
    QLineEdit   *m_cmdInput;
    QLineEdit   *m_argsInput;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_connectBtn;
    QLabel      *m_statusLabel;
};

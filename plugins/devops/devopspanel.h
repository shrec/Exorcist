#pragma once

#include <QWidget>

class QTreeWidget;
class QPlainTextEdit;
class QPushButton;
class QProcess;
class ServiceDiscovery;

/// Panel that shows discovered DevOps services and provides quick-action buttons.
class DevOpsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DevOpsPanel(ServiceDiscovery *discovery, QWidget *parent = nullptr);

    void setWorkingDirectory(const QString &dir) { m_workDir = dir; }
    void refresh();

    void runCommand(const QString &label, const QString &command);
    void stopCommand();
    bool isRunning() const;

signals:
    void commandStarted(const QString &label);
    void commandFinished(const QString &label, int exitCode);

private:
    void buildUi();
    void populateTree();
    void appendOutput(const QString &text, bool isError = false);

    ServiceDiscovery *m_discovery;
    QTreeWidget      *m_tree      = nullptr;
    QPlainTextEdit   *m_output    = nullptr;
    QPushButton      *m_stopBtn   = nullptr;
    QProcess         *m_process   = nullptr;
    QString           m_workDir;
};

#include "serialmonitorplugin.h"

#include "serialmonitorcontroller.h"
#include "serialmonitorpanel.h"

#include "core/idockmanager.h"
#include "sdk/icommandservice.h"
#include "sdk/iterminalservice.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTime>
#include <memory>

namespace {

bool hasWorkspaceFile(const QString &root, const QString &name)
{
    return !root.isEmpty() && QFile::exists(QDir(root).filePath(name));
}

QString settingsPrefix()
{
    return QStringLiteral("serialMonitor/");
}

}

SerialMonitorPlugin::SerialMonitorPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo SerialMonitorPlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.serial-monitor"),
        QStringLiteral("Serial Monitor"),
        QStringLiteral("1.0.0"),
        QStringLiteral("Serial monitor dock and external monitor launcher for embedded workspaces."),
        QStringLiteral("Exorcist"),
        QStringLiteral("1.0"),
        {}
    };
}

bool SerialMonitorPlugin::initializePlugin()
{
    m_controller = std::make_unique<SerialMonitorController>();
    registerCommands();
    installMenusAndToolBar();
    return true;
}

QWidget *SerialMonitorPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId != QStringLiteral("SerialMonitorDock"))
        return nullptr;

    if (!m_panel) {
        auto panel = std::make_unique<SerialMonitorPanel>(parent);
        m_panel = panel.get();
        connect(m_panel, &SerialMonitorPanel::refreshRequested,
            m_controller.get(), &SerialMonitorController::refreshPorts);
        connect(m_panel, &SerialMonitorPanel::connectRequested,
            m_controller.get(), &SerialMonitorController::connectToPort);
        connect(m_panel, &SerialMonitorPanel::disconnectRequested,
            m_controller.get(), &SerialMonitorController::disconnectPort);
        connect(m_panel, &SerialMonitorPanel::sendTextRequested,
            m_controller.get(), &SerialMonitorController::sendText);
        connect(m_panel, &SerialMonitorPanel::settingsChanged,
                this, [this]() { savePanelSettings(); });
        connect(m_panel, &SerialMonitorPanel::openExternalRequested,
                this, [this]() {
            if (auto *cmds = commands())
                cmds->executeCommand(QStringLiteral("serialMonitor.openExternal"));
        });
        connect(m_controller.get(), &SerialMonitorController::portsChanged,
                this, [this](const QStringList &ports) {
            if (!m_panel)
                return;
            const QString previousPort = m_panel->selectedPort();
            m_panel->setAvailablePorts(ports);
            if (!previousPort.isEmpty())
                m_panel->setSelectedPort(previousPort);
        });
        connect(m_controller.get(), &SerialMonitorController::statusChanged,
            m_panel, &SerialMonitorPanel::setStatusText);
        connect(m_controller.get(), &SerialMonitorController::textReceived,
                this, [this](const QString &text) {
            if (m_panel)
                m_panel->appendMessage(formatLogLine(text));
        });
        connect(m_controller.get(), &SerialMonitorController::connectionChanged,
            m_panel, &SerialMonitorPanel::setConnected);
        panel.release();
    }

    m_panel->setParent(parent);
    restorePanelSettings();
    if (m_controller)
        m_controller->refreshPorts();
    return m_panel;
}

void SerialMonitorPlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    cmds->registerCommand(QStringLiteral("serialMonitor.show"),
                          tr("Serial Monitor: Show Panel"),
                          [this]() {
        showPanel(QStringLiteral("SerialMonitorDock"));
        if (m_controller)
            m_controller->refreshPorts();
    });

        cmds->registerCommand(QStringLiteral("serialMonitor.refreshPorts"),
                  tr("Serial Monitor: Refresh Ports"),
                  [this]() {
        if (m_controller)
            m_controller->refreshPorts();
        });

    cmds->registerCommand(QStringLiteral("serialMonitor.clear"),
                          tr("Serial Monitor: Clear"),
                          [this]() {
        if (m_panel)
            m_panel->clearMessages();
    });

    cmds->registerCommand(QStringLiteral("serialMonitor.openExternal"),
                          tr("Serial Monitor: Run External Monitor"),
                          [this]() {
        const QString command = externalMonitorCommand();
        showPanel(QStringLiteral("SerialMonitorDock"));

        if (command.isEmpty()) {
            if (m_panel) {
                m_panel->appendMessage(formatLogLine(
                    tr("No workspace-specific serial monitor command was inferred.")));
            }
            showWarning(tr("No external serial monitor command is configured for this workspace."));
            return;
        }

        if (m_panel)
            m_panel->appendMessage(formatLogLine(tr("Launching: %1").arg(command)));

        if (auto *term = terminal())
            term->runCommand(command);
    });
}

void SerialMonitorPlugin::installMenusAndToolBar()
{
    ensureMenu(QStringLiteral("serial"), tr("&Serial"));
    createToolBar(QStringLiteral("serial"), tr("Serial"));
    addToolBarCommands(QStringLiteral("serial"), {
        {tr("Monitor"), QStringLiteral("serialMonitor.show")},
        {tr("Refresh"), QStringLiteral("serialMonitor.refreshPorts")},
        {tr("Run"), QStringLiteral("serialMonitor.openExternal")},
    }, this);

    addMenuCommand(QStringLiteral("serial"), tr("Show Serial &Monitor"),
                   QStringLiteral("serialMonitor.show"), this);
    addMenuCommand(QStringLiteral("serial"), tr("&Refresh Ports"),
                   QStringLiteral("serialMonitor.refreshPorts"), this);
    addMenuCommand(QStringLiteral("serial"), tr("Run E&xternal Monitor"),
                   QStringLiteral("serialMonitor.openExternal"), this);
}

void SerialMonitorPlugin::restorePanelSettings() const
{
    if (!m_panel)
        return;

    QSettings settings;
    m_panel->setSelectedPort(settings.value(settingsPrefix() + QStringLiteral("lastPort")).toString());
    m_panel->setSelectedBaudRate(settings.value(settingsPrefix() + QStringLiteral("baudRate"), 115200).toInt());
    m_panel->setSelectedNewlineMode(static_cast<SerialMonitorPanel::NewlineMode>(
        settings.value(settingsPrefix() + QStringLiteral("newlineMode"),
                       static_cast<int>(SerialMonitorPanel::NewlineMode::Lf)).toInt()));
    m_panel->setTimestampsEnabled(settings.value(settingsPrefix() + QStringLiteral("timestamps"), false).toBool());
}

void SerialMonitorPlugin::savePanelSettings() const
{
    if (!m_panel)
        return;

    QSettings settings;
    settings.setValue(settingsPrefix() + QStringLiteral("lastPort"), m_panel->selectedPort());
    settings.setValue(settingsPrefix() + QStringLiteral("baudRate"), m_panel->selectedBaudRate());
    settings.setValue(settingsPrefix() + QStringLiteral("newlineMode"),
                      static_cast<int>(m_panel->selectedNewlineMode()));
    settings.setValue(settingsPrefix() + QStringLiteral("timestamps"), m_panel->timestampsEnabled());
}

QString SerialMonitorPlugin::formatLogLine(const QString &text) const
{
    if (!m_panel || !m_panel->timestampsEnabled())
        return text;

    return QStringLiteral("[%1] %2")
        .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), text);
}

QString SerialMonitorPlugin::externalMonitorCommand() const
{
    const QString root = workspaceRoot();
    if (hasWorkspaceFile(root, QStringLiteral("platformio.ini")))
        return QStringLiteral("platformio device monitor");
    return QString();
}


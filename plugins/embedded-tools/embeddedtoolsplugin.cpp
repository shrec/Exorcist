#include "embeddedtoolsplugin.h"

#include "embeddedtoolspanel.h"

#include "core/idockmanager.h"
#include "core/imenumanager.h"
#include "sdk/ibuildsystem.h"
#include "sdk/icommandservice.h"
#include "sdk/iterminalservice.h"

#include <QCryptographicHash>
#include <QSettings>

#include <memory>

EmbeddedToolsPlugin::EmbeddedToolsPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo EmbeddedToolsPlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.embedded-tools"),
        QStringLiteral("Embedded Tools"),
        QStringLiteral("1.0.0"),
        QStringLiteral("Embedded upload and monitor entry points for MCU workspaces."),
        QStringLiteral("Exorcist"),
        QStringLiteral("1.0"),
        {PluginPermission::WorkspaceRead, PluginPermission::TerminalExecute, PluginPermission::FilesystemRead}
    };
}

bool EmbeddedToolsPlugin::initializePlugin()
{
    registerCommands();
    installMenusAndToolBar();
    return true;
}

QWidget *EmbeddedToolsPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId != QStringLiteral("EmbeddedToolsDock"))
        return nullptr;

    if (!m_panel) {
        auto panel = std::make_unique<EmbeddedToolsPanel>(parent);
        m_panel = panel.get();
        connect(m_panel, &EmbeddedToolsPanel::configureRequested,
                this, [this]() {
            if (auto *cmds = commands())
                cmds->executeCommand(QStringLiteral("build.configure"));
        });
        connect(m_panel, &EmbeddedToolsPanel::flashRequested,
                this, [this]() {
            if (auto *cmds = commands())
                cmds->executeCommand(QStringLiteral("embedded.flashFirmware"));
        });
        connect(m_panel, &EmbeddedToolsPanel::monitorRequested,
                this, [this]() {
            if (auto *cmds = commands())
                cmds->executeCommand(QStringLiteral("embedded.openMonitor"));
        });
        connect(m_panel, &EmbeddedToolsPanel::debugRequested,
            this, [this]() {
            if (auto *cmds = commands())
                cmds->executeCommand(QStringLiteral("embedded.startDebugRoute"));
        });
        connect(m_panel, &EmbeddedToolsPanel::flashCommandOverrideChanged,
                this, [this](const QString &) {
            if (!m_panel)
                return;
            persistCommandOverrides(m_panel->flashCommandOverride(),
                                    m_panel->monitorCommandOverride(),
                                    m_panel->debugCommandOverride());
        });
        connect(m_panel, &EmbeddedToolsPanel::monitorCommandOverrideChanged,
                this, [this](const QString &) {
            if (!m_panel)
                return;
            persistCommandOverrides(m_panel->flashCommandOverride(),
                                    m_panel->monitorCommandOverride(),
                                    m_panel->debugCommandOverride());
        });
        connect(m_panel, &EmbeddedToolsPanel::debugCommandOverrideChanged,
                this, [this](const QString &) {
            if (!m_panel)
                return;
            persistCommandOverrides(m_panel->flashCommandOverride(),
                                    m_panel->monitorCommandOverride(),
                                    m_panel->debugCommandOverride());
        });
        connect(m_panel, &EmbeddedToolsPanel::resetOverridesRequested,
                this, [this]() {
            clearCommandOverrides();
            updatePanelHints();
        });
        panel.release();
    }

    m_panel->setParent(parent);
    updatePanelHints();
    return m_panel;
}

void EmbeddedToolsPlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    cmds->registerCommand(QStringLiteral("embedded.showTools"),
                          tr("Embedded: Show Tools"),
                          [this]() {
        showPanel(QStringLiteral("EmbeddedToolsDock"));
    });

    cmds->registerCommand(QStringLiteral("embedded.flashFirmware"),
                          tr("Embedded: Flash Firmware"),
                          [this]() {
        const QString command = currentResolution().flashCommand;
        if (command.isEmpty()) {
            showWarning(tr("No embedded upload command could be inferred for this workspace."));
            return;
        }

        if (auto *term = terminal()) {
            term->runCommand(command);
            showPanel(QStringLiteral("TerminalDock"));
        }
    });

    cmds->registerCommand(QStringLiteral("embedded.openMonitor"),
                          tr("Embedded: Open Serial Monitor"),
                          [this]() {
        showPanel(QStringLiteral("SerialMonitorDock"));

        const QString command = currentResolution().monitorCommand;
        if (command.isEmpty()) {
            showStatusMessage(
                tr("Open the Serial Monitor panel and configure the target command manually."),
                4000);
            return;
        }

        if (auto *term = terminal())
            term->runCommand(command);
    });

    cmds->registerCommand(QStringLiteral("embedded.startDebugRoute"),
                          tr("Embedded: Start Debug Route"),
                          [this]() {
        const EmbeddedCommandResolution resolution = currentResolution();
        showPanel(QStringLiteral("DebugDock"));

        if (resolution.debugCommand.isEmpty()) {
            showStatusMessage(
                tr("%1").arg(resolution.debugSummary.isEmpty()
                    ? tr("No embedded debug route could be inferred for this workspace.")
                    : resolution.debugSummary),
                5000);
            return;
        }

        if (auto *term = terminal()) {
            term->runCommand(resolution.debugCommand);
            showPanel(QStringLiteral("TerminalDock"));
        }
    });
}

void EmbeddedToolsPlugin::installMenusAndToolBar()
{
    ensureMenu(QStringLiteral("embedded"), tr("&Embedded"));

    createToolBar(QStringLiteral("embedded"), tr("Embedded"));
    addToolBarCommands(QStringLiteral("embedded"), {
        {tr("Tools"), QStringLiteral("embedded.showTools")},
        {tr("Flash"), QStringLiteral("embedded.flashFirmware")},
        {tr("Monitor"), QStringLiteral("embedded.openMonitor")},
        {tr("Debug"), QStringLiteral("embedded.startDebugRoute")},
    }, this);

    addMenuCommand(QStringLiteral("embedded"), tr("&Show Embedded Tools"),
                   QStringLiteral("embedded.showTools"), this);
    addMenuCommand(QStringLiteral("embedded"), tr("&Flash Firmware"),
                   QStringLiteral("embedded.flashFirmware"), this);
    addMenuCommand(QStringLiteral("embedded"), tr("Open Serial &Monitor"),
                   QStringLiteral("embedded.openMonitor"), this);
    addMenuCommand(QStringLiteral("embedded"), tr("Start &Debug Route"),
                   QStringLiteral("embedded.startDebugRoute"), this);
}

void EmbeddedToolsPlugin::updatePanelHints()
{
    if (!m_panel)
        return;

    const EmbeddedCommandResolution inferred = inferredResolution();
    const EmbeddedCommandResolution overrides = commandOverrides();

    m_panel->setWorkspaceSummary(tr("%1").arg(inferred.workspaceSummary));
    m_panel->setRecommendedCommands(inferred.flashCommand, inferred.monitorCommand);
    m_panel->setDebugRoute(inferred.debugSummary, inferred.debugCommand);
    m_panel->setCommandOverrides(overrides.flashCommand,
                                 overrides.monitorCommand,
                                 overrides.debugCommand);
}

EmbeddedCommandResolution EmbeddedToolsPlugin::inferredResolution() const
{
    EmbeddedCommandResolution resolution = m_commandResolver.resolve(workspaceRoot());

    if (auto *buildSystem = service<IBuildSystem>(QStringLiteral("buildSystem"))) {
        if (buildSystem->hasProject() && resolution.flashCommand.isEmpty())
            resolution.flashCommand = QStringLiteral("cmake --build build --target flash");
    }

    return resolution;
}

EmbeddedCommandResolution EmbeddedToolsPlugin::commandOverrides() const
{
    QSettings settings;
    settings.beginGroup(workspaceSettingsKey());
    EmbeddedCommandResolution overrides;
    overrides.flashCommand = settings.value(QStringLiteral("flashCommand")).toString().trimmed();
    overrides.monitorCommand = settings.value(QStringLiteral("monitorCommand")).toString().trimmed();
    overrides.debugCommand = settings.value(QStringLiteral("debugCommand")).toString().trimmed();
    settings.endGroup();
    return overrides;
}

void EmbeddedToolsPlugin::persistCommandOverrides(const QString &flashCommand,
                                                  const QString &monitorCommand,
                                                  const QString &debugCommand) const
{
    QSettings settings;
    settings.beginGroup(workspaceSettingsKey());
    settings.setValue(QStringLiteral("flashCommand"), flashCommand.trimmed());
    settings.setValue(QStringLiteral("monitorCommand"), monitorCommand.trimmed());
    settings.setValue(QStringLiteral("debugCommand"), debugCommand.trimmed());
    settings.endGroup();
}

void EmbeddedToolsPlugin::clearCommandOverrides() const
{
    QSettings settings;
    settings.remove(workspaceSettingsKey());
}

QString EmbeddedToolsPlugin::workspaceSettingsKey() const
{
    const QString root = workspaceRoot().isEmpty() ? QStringLiteral("global") : workspaceRoot();
    const QByteArray digest = QCryptographicHash::hash(root.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("embeddedTools/workspaces/%1").arg(QString::fromLatin1(digest));
}

EmbeddedCommandResolution EmbeddedToolsPlugin::currentResolution() const
{
    EmbeddedCommandResolution resolution = inferredResolution();
    const EmbeddedCommandResolution overrides = commandOverrides();
    if (!overrides.flashCommand.isEmpty())
        resolution.flashCommand = overrides.flashCommand;
    if (!overrides.monitorCommand.isEmpty())
        resolution.monitorCommand = overrides.monitorCommand;
    if (!overrides.debugCommand.isEmpty())
        resolution.debugCommand = overrides.debugCommand;
    return resolution;
}


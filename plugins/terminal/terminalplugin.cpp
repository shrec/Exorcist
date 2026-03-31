#include "terminalplugin.h"

#include "terminal/terminalpanel.h"

#include "sdk/ihostservices.h"
#include "sdk/iterminalservice.h"
#include "serviceregistry.h"

#include <QObject>
#include <QString>

// ── TerminalServiceImpl (plugin-local) ────────────────────────────────────────
// Bridges TerminalPanel to the ITerminalService SDK interface.
// Lives here because TerminalPanel is owned by this plugin.
class TerminalServiceImpl : public QObject, public ITerminalService
{
    Q_OBJECT
public:
    explicit TerminalServiceImpl(TerminalPanel *panel, QObject *parent = nullptr)
        : QObject(parent), m_panel(panel) {}

    void runCommand(const QString &command) override
    {
        if (m_panel) m_panel->sendCommand(command);
    }
    void sendInput(const QString &text) override
    {
        if (m_panel) m_panel->sendInput(text);
    }
    QString recentOutput(int maxLines) const override
    {
        return m_panel ? m_panel->recentOutput(maxLines) : QString();
    }
    void openTerminal() override
    {
        if (m_panel) m_panel->sendCommand(QString());
    }
    QString selectedText() const override
    {
        return m_panel ? m_panel->selectedText() : QString();
    }
    void setWorkingDirectory(const QString &dir) override
    {
        if (m_panel) m_panel->setWorkingDirectory(dir);
    }

private:
    TerminalPanel *m_panel;
};

// ── TerminalPlugin ────────────────────────────────────────────────────────────

TerminalPlugin::TerminalPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo TerminalPlugin::info() const
{
    PluginInfo i;
    i.id          = QStringLiteral("org.exorcist.terminal");
    i.name        = QStringLiteral("Terminal");
    i.version     = QStringLiteral("1.0.0");
    i.description = QStringLiteral("Integrated terminal emulator with multi-tab support.");
    i.requestedPermissions = {PluginPermission::TerminalExecute, PluginPermission::WorkspaceRead};
    return i;
}

bool TerminalPlugin::initializePlugin()
{
    m_panel = new TerminalPanel(nullptr);

    // Register ITerminalService so the shell and other plugins can resolve
    // it via terminal() or service<ITerminalService>("terminalService").
    registerService(QStringLiteral("terminalService"),
                            new TerminalServiceImpl(m_panel, this));

    return true;
}

void TerminalPlugin::shutdownPlugin()
{
    m_panel = nullptr;
}

void TerminalPlugin::onWorkspaceChanged(const QString &root)
{
    if (auto *ts = terminal())
        ts->setWorkingDirectory(root);
}

QWidget *TerminalPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QLatin1String("TerminalDock") && m_panel) {
        m_panel->setParent(parent);
        return m_panel;
    }
    return nullptr;
}

#include "terminalplugin.moc"

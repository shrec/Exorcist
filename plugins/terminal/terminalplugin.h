#pragma once

#include "plugin/workbenchpluginbase.h"
#include "plugin/iviewcontributor.h"

#include <QObject>

class TerminalPanel;

// ── TerminalPlugin ────────────────────────────────────────────────────────────
// Workbench plugin that owns the terminal emulator panel, contributes the
// TerminalDock view, and registers ITerminalService in the ServiceRegistry.
class TerminalPlugin : public QObject,
                       public WorkbenchPluginBase,
                       public IViewContributor
{
    Q_OBJECT
    Q_INTERFACES(IPlugin IViewContributor)

public:
    explicit TerminalPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;
    void shutdownPlugin() override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

protected:
    bool initializePlugin() override;

private:
    TerminalPanel *m_panel = nullptr;
};

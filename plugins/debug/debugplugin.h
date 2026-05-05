#pragma once

#include <QObject>
#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"

class GdbMiAdapter;
class DebugPanel;
class MemoryViewPanel;
class DisassemblyPanel;
class IDebugService;
class EditorManager;
class EditorView;

class DebugPlugin : public QObject, public WorkbenchPluginBase, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor)

public:
    explicit DebugPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private slots:
    void onAdapterStartedShowDock();
    void onDebugStoppedShowDock();
    void onAdapterErrorShowInfo(const QString &msg);

private slots:
    /// Per-editor breakpoint gutter integration.  Pulled out of MainWindow
    /// + PostPluginBootstrap (rule L1, L5) — debug plugin owns the wiring.
    void onEditorOpenedWireBreakpoints(EditorView *editor, const QString &path);
    void onEditorBreakpointToggled(const QString &fp, int line, bool added);
    void onEditorBreakpointConditionChanged(const QString &fp, int line,
                                            const QString &condition);
    void onEditorBreakpointEnabledChanged(const QString &fp, int line, bool enabled);

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;
    void onWorkspaceChanged(const QString &root) override;
    void registerCommands();
    void installMenusAndToolBar();

    /// Iterate currently-open editors and wire breakpoint signals on each
    /// (called once during initializePlugin — afterwards new editors are
    /// picked up via EditorManager::editorOpened).
    void wireExistingEditors();

    GdbMiAdapter     *m_adapter         = nullptr;
    DebugPanel       *m_panel           = nullptr;
    MemoryViewPanel  *m_memoryView      = nullptr;
    DisassemblyPanel *m_disassemblyView = nullptr;
    IDebugService    *m_debugService    = nullptr;
    EditorManager    *m_editorMgr       = nullptr;
};

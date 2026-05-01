#pragma once

#include <QObject>
#include <QMetaObject>
#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"
#include "agent/iagentplugin.h"
#include "aiinterface.h"

#include <QHash>
#include <memory>
#include <vector>

class ITool;
class GitService;
class GitPanel;
class DiffExplorerPanel;
class MergeEditor;
class InlineBlameOverlay;
class EditorView;
class QPlainTextEdit;

class GitPlugin : public QObject,
                  public WorkbenchPluginBase,
                  public IViewContributor,
                  public IAgentToolPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor IAgentToolPlugin)

public:
    explicit GitPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

    // IAgentToolPlugin
    QString toolPluginName() const override { return QStringLiteral("Git"); }
    QStringList contexts() const override { return {QStringLiteral("git")}; }
    std::vector<std::unique_ptr<ITool>> createTools() override;

private slots:
    // Slot signatures must match SDK signal exactly so SIGNAL/SLOT string-based
    // connect can resolve across DLL boundaries (PMF connect silently fails
    // when SDK MOC is duplicated into multiple plugin DLLs).
    void onGenerateCommitMessageRequested();
    void onGitDiffReady(const QString &filePath, const QString &diff);
    void onAgentResponseFinished(const QString &requestId,
                                 const AgentResponse &response);
    void onResolveConflictsRequested();
    // Inline blame wiring (cross-DLL via SIGNAL/SLOT). Receives the editor as a
    // QPlainTextEdit*; we do not depend on EditorView's full type here.
    void onEditorOpened(EditorView *editor, const QString &path);
    void toggleInlineBlame(bool on);

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;

    void wireCommitMessageGeneration();
    void wireConflictResolution();
    void wireInlineBlame();
    void installBlameOverlay(EditorView *editor, const QString &path);
    void applyBlameEnabledFromSettings();
    bool inlineBlameEnabled() const;

    GitService        *m_git          = nullptr;
    GitPanel          *m_gitPanel     = nullptr;
    DiffExplorerPanel *m_diffExplorer = nullptr;
    MergeEditor       *m_mergeEditor  = nullptr;

    // One-shot connection for the next AgentOrchestrator response (commit msg)
    QMetaObject::Connection m_pendingCommitMsgConn;

    // Inline blame: per-editor overlay (lifetime tied to editor via QPointer).
    QHash<QObject*, InlineBlameOverlay*> m_blameOverlays;
};

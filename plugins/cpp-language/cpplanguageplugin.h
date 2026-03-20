#pragma once

#include <QObject>

#include "plugin/iviewcontributor.h"
#include "plugin/languageworkbenchpluginbase.h"

#include <QPointer>
#include <QTimer>
#include <QString>

class ClangdManager;
class CppWorkspacePanel;
class IBuildSystem;
class IDebugService;
class ILaunchService;
class ITestRunner;
class ISearchService;
class LspClient;
class ILspService;

class CppLanguagePlugin : public QObject, public LanguageWorkbenchPluginBase, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin IViewContributor)

public:
    explicit CppLanguagePlugin(QObject *parent = nullptr);

    PluginInfo info() const override;
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;
    /// Register all C/C++ commands with ICommandService.
    void registerCommands();
    void installMenus();
    void installToolBar();
    void installStatusItem();
    void resolveOptionalServices();
    void wireBuildSystem();
    void wireLaunchService();
    void wireTestRunner();
    void wireSearchService();
    void wireDebugService();
    void updateStatusPresentation(const QString &text, const QString &tooltip = QString());
    void updateWorkspacePanel();
    void updateWorkspaceCard(const QString &cardId,
                             const QString &title,
                             const QString &detail);
    void refreshWorkspaceSnapshot();
    QString currentCompileCommandsPath() const;
    QString currentBuildDirectory() const;

    /// Open the corresponding header for a source file, or vice versa.
    void switchHeaderSource(const QString &filePath);

    ClangdManager     *m_clangd        = nullptr;
    CppWorkspacePanel *m_workspacePanel = nullptr;
    IBuildSystem      *m_buildSystem   = nullptr;
    IDebugService     *m_debugService  = nullptr;
    ILaunchService    *m_launchService = nullptr;
    ITestRunner       *m_testRunner    = nullptr;
    ISearchService    *m_searchService = nullptr;
    LspClient         *m_lspClient     = nullptr;
    ILspService       *m_lspService    = nullptr;
    /// How many times clangd has been auto-restarted since last clean start.
    int                m_restartCount = 0;
    bool               m_expectRestart = false;
    bool               m_isShuttingDown = false;
    bool               m_buildSignalsWired = false;
    bool               m_launchSignalsWired = false;
    bool               m_testSignalsWired = false;
    bool               m_searchSignalsWired = false;
    bool               m_debugSignalsWired = false;

    QString            m_workspaceRootText;
    QString            m_activeFileText;
    QString            m_buildDirectoryText;
    QString            m_languageStatusTitle;
    QString            m_languageStatusDetail;
    QString            m_buildStatusTitle;
    QString            m_buildStatusDetail;
    QString            m_debugStatusTitle;
    QString            m_debugStatusDetail;
    QString            m_testStatusTitle;
    QString            m_testStatusDetail;
    QString            m_searchStatusTitle;
    QString            m_searchStatusDetail;

    QTimer             m_workspaceRefreshTimer;
};

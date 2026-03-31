#pragma once

#include <QHash>
#include <QSet>
#include <QStringList>
#include <QWidget>

class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

// ── CppWorkspacePanel ─────────────────────────────────────────────────────────
//
// Visual Studio 2022–style Solution Explorer panel for C++ workspaces.
//
// Layout:
//   ┌ Header toolbar  (28 px, VS2022 #2d2d30) ──────────────────────────────┐
//   │ C++ Workspace                          [Filter] [Collapse] [Refresh]  │
//   ├ Filter bar  (hidden by default) ──────────────────────────────────────┤
//   │ [Filter files...                                              ] [X]   │
//   ├ Project info bar  (compact, bg #252526) ──────────────────────────────┤
//   │  Root: ProjectName  |  Build: build-llvm  |  Active: main.cpp        │
//   ├ Actions bar  (2 rows, bg #1b1b1c) ────────────────────────────────────┤
//   │  [⚙ Config] [⬛ Build] [▶ Run] [● Debug] [✕ Clean]                  │
//   │  [⬜ Tests]  [↻ Index]                                                │
//   ├ File tree  (QTreeWidget, VS2022 dark) ─────────────────────────────── │
//   │  > Solution 'ProjectName'                                             │
//   │    > src/                                                             │
//   │      main.cpp                                                         │
//   │    > plugins/                                                         │
//   ├ Status cards (expandable, bg #252526) ────────────────────────────────┤
//   │  [Status]                                            [toggle]         │
//   │  ● clangd: Ready      ● Build: Succeeded                             │
//   │  ● Debug: Idle         ● Tests: 54 passed                             │
//   │  ● Search: Ready                                                      │
//   └───────────────────────────────────────────────────────────────────────┘

class CppWorkspacePanel : public QWidget
{
    Q_OBJECT

public:
    explicit CppWorkspacePanel(QWidget *parent = nullptr);

    /// Update workspace header info and refresh the file tree.
    void setWorkspaceSummary(const QString &workspaceRoot,
                             const QString &activeFile,
                             const QString &buildDirectory,
                             const QString &compileCommandsPath);

    /// Update a named status indicator card.
    /// cardId: "language" | "build" | "debug" | "tests" | "search"
    void setCardStatus(const QString &cardId,
                       const QString &title,
                       const QString &detail);

    /// Update the CMake targets list. activeTarget is the currently selected target name.
    void setTargets(const QStringList &targets, const QString &activeTarget);

    /// Enable / disable a command (stored for right-click context menu).
    void setActionEnabled(const QString &actionId, bool enabled);

signals:
    void commandRequested(const QString &commandId);
    void panelRequested(const QString &panelId);
    void buildTerminalRequested();
    void fileOpenRequested(const QString &filePath);
    /// Emitted when the user selects a different active build target.
    void activeTargetChanged(const QString &targetName);

private slots:
    void toggleFilterBar();
    void applyFilter(const QString &text);
    void collapseAll();
    void refreshTree();
    void toggleStatusCards();
    void toggleTargets();
    void onTargetDoubleClicked(QListWidgetItem *item);
    void onTargetContextMenu(const QPoint &pos);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onItemContextMenu(const QPoint &pos);

private:
    void buildTree();
    void populateDirectory(QTreeWidgetItem *parent,
                           const QString   &dirPath,
                           int              depth);
    void setFilterVisible(bool visible);
    void filterItems(QTreeWidgetItem *item, const QString &lowerText);
    void updateProjectInfoBar();
    static bool isRelevantFile(const QString &fileName);
    static bool isExcludedDir(const QString &name);
    static QColor fileColor(const QString &fileName);

    QString m_workspaceRoot;
    QString m_activeFile;
    QString m_buildDirectory;
    QString m_compileCommandsPath;

    // Header
    QLabel      *m_titleLabel  = nullptr;
    QToolButton *m_searchBtn   = nullptr;

    // Filter bar
    QWidget     *m_filterBar   = nullptr;
    QLineEdit   *m_filterEdit  = nullptr;

    // Project info bar
    QWidget     *m_infoBar     = nullptr;
    QLabel      *m_infoRootLabel     = nullptr;
    QLabel      *m_infoBuildLabel    = nullptr;
    QLabel      *m_infoActiveLabel   = nullptr;

    // Actions bar
    QWidget     *m_actionsBar      = nullptr;
    QHash<QString, QToolButton *> m_actionButtonsMap;

    // Tree
    QTreeWidget *m_tree        = nullptr;

    // Targets section
    QWidget      *m_targetsHeader     = nullptr;
    QWidget      *m_targetsBody       = nullptr;
    QToolButton  *m_targetsToggleBtn  = nullptr;
    QListWidget  *m_targetsList       = nullptr;
    bool          m_targetsExpanded   = true;
    QString       m_activeTarget;

    // Status cards section
    QWidget     *m_statusCardsHeader = nullptr;
    QWidget     *m_statusCardsBody   = nullptr;
    QToolButton *m_statusToggleBtn   = nullptr;
    bool         m_statusExpanded    = true;
    QHash<QString, QLabel *> m_statusLabels;

    // Enabled state cache for context menu
    QSet<QString> m_disabledActions;
};

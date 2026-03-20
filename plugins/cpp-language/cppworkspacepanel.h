#pragma once

#include <QHash>
#include <QSet>
#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

// ── CppWorkspacePanel ─────────────────────────────────────────────────────────
//
// Visual Studio–style Solution Explorer panel for C++ workspaces.
//
// Layout:
//   ┌ Header toolbar  (28 px, VS2022 #2d2d30) ──────────────────────────────┐
//   │ C++ Workspace                          [🔍] [⊟] [↻]                  │
//   ├ Filter bar  (hidden by default) ──────────────────────────────────────┤
//   │ 🔍 [Filter...                                               ] [✕]    │
//   ├ File tree  (QTreeWidget, VS2022 dark) ─────────────────────────────── │
//   │  ▸ Solution 'ProjectName'                                             │
//   │    ▸ 📁 src/                                                          │
//   │      📄 main.cpp                                                      │
//   │    ▸ 📁 plugins/                                                      │
//   ├ Status strip  (2-line compact, bg #252526) ────────────────────────── │
//   │  ● clangd: Ready     ⚙ Build: Idle     ✓ Tests: —                   │
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

    /// Update a named status indicator in the bottom strip.
    /// cardId: "language" | "build" | "debug" | "tests" | "search"
    void setCardStatus(const QString &cardId,
                       const QString &title,
                       const QString &detail);

    /// Enable / disable a command (stored for right-click context menu).
    void setActionEnabled(const QString &actionId, bool enabled);

signals:
    void commandRequested(const QString &commandId);
    void panelRequested(const QString &panelId);
    void buildTerminalRequested();
    void fileOpenRequested(const QString &filePath);

private slots:
    void toggleFilterBar();
    void applyFilter(const QString &text);
    void collapseAll();
    void refreshTree();
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onItemContextMenu(const QPoint &pos);

private:
    void buildTree();
    void populateDirectory(QTreeWidgetItem *parent,
                           const QString   &dirPath,
                           int              depth);
    void setFilterVisible(bool visible);
    void filterItems(QTreeWidgetItem *item, const QString &lowerText);
    static bool isRelevantFile(const QString &fileName);
    static bool isExcludedDir(const QString &name);
    static QColor fileColor(const QString &fileName);

    QString m_workspaceRoot;
    QString m_activeFile;

    // Header
    QLabel      *m_titleLabel  = nullptr;
    QToolButton *m_searchBtn   = nullptr;

    // Filter bar
    QWidget     *m_filterBar   = nullptr;
    QLineEdit   *m_filterEdit  = nullptr;

    // Tree
    QTreeWidget *m_tree        = nullptr;

    // Status strip: id → label widget showing "● Title: Detail"
    QHash<QString, QLabel *> m_statusLabels;

    // Enabled state cache for context menu
    QSet<QString> m_disabledActions;
};

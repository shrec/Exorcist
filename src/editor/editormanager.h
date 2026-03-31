#pragma once

#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QStringList>

class EditorView;
class QTabWidget;
class QTreeView;
class QStackedWidget;
class SolutionTreeModel;
class ProjectManager;
class BreadcrumbBar;
class IFileSystem;
class LanguageProfileManager;

/// Owns the core editor/tab/project state previously scattered across MainWindow.
/// Provides typed accessors so MainWindow can interact with editor subsystems
/// without keeping individual member pointers.
///
/// D1 extraction: openFile, closeTab, closeAllTabs, closeOtherTabs have moved
/// here.  Signal editorOpened() fires after each new tab is created so that
/// MainWindow (or any other observer) can wire per-editor connections (LSP,
/// inline chat, AI actions, breakpoints, etc.) without EditorManager needing
/// to know about those subsystems.
class EditorManager : public QObject
{
    Q_OBJECT

public:
    explicit EditorManager(QObject *parent = nullptr);

    // ── Accessors ──────────────────────────────────────────────────────────
    QTabWidget        *tabs() const { return m_tabs; }
    QTreeView         *fileTree() const { return m_fileTree; }
    SolutionTreeModel *treeModel() const { return m_treeModel; }
    ProjectManager    *projectManager() const { return m_projectManager; }
    BreadcrumbBar     *breadcrumb() const { return m_breadcrumb; }

    /// Returns the EditorView for the currently active tab, or nullptr.
    EditorView        *currentEditor() const;
    /// Returns the EditorView at tab index, or nullptr.
    EditorView        *editorAt(int index) const;

    const QString     &currentFolder() const { return m_currentFolder; }
    const QStringList &includePaths() const { return m_includePaths; }
    QMetaObject::Connection cursorConn() const { return m_cursorConn; }

    // ── Setters ────────────────────────────────────────────────────────────
    void setTabs(QTabWidget *tabs) { m_tabs = tabs; }
    void setFileTree(QTreeView *tree) { m_fileTree = tree; }
    void setTreeModel(SolutionTreeModel *model) { m_treeModel = model; }
    void setProjectManager(ProjectManager *pm) { m_projectManager = pm; }
    void setBreadcrumb(BreadcrumbBar *bc) { m_breadcrumb = bc; }
    void setCurrentFolder(const QString &folder) { m_currentFolder = folder; }
    void setIncludePaths(const QStringList &paths) { m_includePaths = paths; }
    void addIncludePath(const QString &path) { m_includePaths << path; }
    void setCursorConn(const QMetaObject::Connection &conn) { m_cursorConn = conn; }

    // ── D1 deps (call these after the objects are constructed) ─────────────
    void setFileSystem(IFileSystem *fs) { m_fileSystem = fs; }
    void setCentralStack(QStackedWidget *stack) { m_centralStack = stack; }
    void setLanguageProfileManager(LanguageProfileManager *lpm) { m_langProfile = lpm; }

    // ── File / tab lifecycle ───────────────────────────────────────────────
    /// Open (or focus) a file.  Emits editorOpened() for per-editor wiring.
    void openFile(const QString &path);

    /// Remove and delete the tab at index.  Emits tabClosed(path).
    void closeTab(int index);

    /// Close every open tab.
    void closeAllTabs();

    /// Close every tab except the one at keepIndex.
    void closeOtherTabs(int keepIndex);

signals:
    /// Fired after a new EditorView is added to the tab strip.
    /// Connect to wire LSP bridges, AI actions, inline chat, breakpoints, etc.
    void editorOpened(EditorView *editor, const QString &path);

    /// Proxy for MainWindow's status bar — avoids EditorManager knowing about QMainWindow.
    void statusMessage(const QString &message, int timeoutMs);

    /// Fired when a tab is closed; path is empty for unsaved/untitled editors.
    void tabClosed(const QString &path);

private:
    QTabWidget             *m_tabs           = nullptr;
    QTreeView              *m_fileTree       = nullptr;
    SolutionTreeModel      *m_treeModel      = nullptr;
    ProjectManager         *m_projectManager = nullptr;
    BreadcrumbBar          *m_breadcrumb     = nullptr;
    QString                 m_currentFolder;
    QStringList             m_includePaths;
    QMetaObject::Connection m_cursorConn;

    // D1 deps
    IFileSystem            *m_fileSystem    = nullptr;
    QStackedWidget         *m_centralStack  = nullptr;
    LanguageProfileManager *m_langProfile   = nullptr;
};

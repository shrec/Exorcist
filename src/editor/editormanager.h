#pragma once

#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QStringList>

class EditorView;
class QTabWidget;
class QTreeView;
class SolutionTreeModel;
class ProjectManager;
class BreadcrumbBar;

/// Owns the core editor/tab/project state previously scattered across MainWindow.
/// Provides typed accessors so MainWindow can interact with editor subsystems
/// without keeping individual member pointers.
class EditorManager : public QObject
{
    Q_OBJECT

public:
    explicit EditorManager(QObject *parent = nullptr);

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

    void setTabs(QTabWidget *tabs) { m_tabs = tabs; }
    void setFileTree(QTreeView *tree) { m_fileTree = tree; }
    void setTreeModel(SolutionTreeModel *model) { m_treeModel = model; }
    void setProjectManager(ProjectManager *pm) { m_projectManager = pm; }
    void setBreadcrumb(BreadcrumbBar *bc) { m_breadcrumb = bc; }
    void setCurrentFolder(const QString &folder) { m_currentFolder = folder; }
    void setIncludePaths(const QStringList &paths) { m_includePaths = paths; }
    void addIncludePath(const QString &path) { m_includePaths << path; }
    void setCursorConn(const QMetaObject::Connection &conn) { m_cursorConn = conn; }

private:
    QTabWidget             *m_tabs           = nullptr;
    QTreeView              *m_fileTree       = nullptr;
    SolutionTreeModel      *m_treeModel      = nullptr;
    ProjectManager         *m_projectManager = nullptr;
    BreadcrumbBar          *m_breadcrumb     = nullptr;
    QString                 m_currentFolder;
    QStringList             m_includePaths;
    QMetaObject::Connection m_cursorConn;
};

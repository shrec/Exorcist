#pragma once

#include <QAbstractItemModel>
#include <QFileIconProvider>
#include <QFileSystemWatcher>
#include <QSet>
#include <memory>
#include <vector>

class GitService;
class ProjectManager;

class SolutionTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1
    };

    explicit SolutionTreeModel(ProjectManager *pm, GitService *git, QObject *parent = nullptr);
    ~SolutionTreeModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;
    bool hasChildren(const QModelIndex &parent) const override;

    QString filePath(const QModelIndex &index) const;
    bool isDir(const QModelIndex &index) const;

    /// File / directory exclusion API.
    /// By default, common build artifacts and IDE metadata are hidden.
    void setShowHiddenFiles(bool show);
    bool showHiddenFiles() const { return m_showHiddenFiles; }

    void addExcludedDirPattern(const QString &name);
    void removeExcludedDirPattern(const QString &name);
    QSet<QString> excludedDirPatterns() const { return m_hiddenDirs; }

    void addExcludedFilePattern(const QString &pattern);
    void removeExcludedFilePattern(const QString &pattern);
    QSet<QString> excludedFilePatterns() const { return m_hiddenFilePatterns; }

    /// Check whether a file or directory entry should be excluded from the tree.
    bool shouldHideEntry(const QFileInfo &info) const;

    /// Static defaults for testing / reset.
    static QSet<QString> defaultHiddenDirs();
    static QSet<QString> defaultHiddenFilePatterns();

public slots:
    void refreshGitOverlays();
    void rebuildFromSolution();
    void refreshDirectory(const QModelIndex &index);

private slots:
    void onDirectoryChanged(const QString &path);

private:
    struct TreeNode {
        enum Kind { Root, Solution, Project, Dir, File } kind;
        QString name;
        QString path;
        TreeNode *parent = nullptr;
        std::vector<std::unique_ptr<TreeNode>> children;
        bool childrenFetched = false;
    };

    TreeNode *nodeFromIndex(const QModelIndex &index) const;
    int childIndex(const TreeNode *parent, const TreeNode *child) const;
    void clearTree();
    void buildTree();
    void fetchChildren(TreeNode *node);
    void refreshNode(TreeNode *node);
    TreeNode *findNodeByPath(TreeNode *root, const QString &path) const;
    QModelIndex indexForNode(TreeNode *node) const;
    QColor gitColorForPath(const QString &path) const;
    void emitDataChangedForFetchedNodes(TreeNode *node);

    std::unique_ptr<TreeNode> m_root;
    ProjectManager *m_projectManager;
    GitService *m_gitService;
    QFileIconProvider m_iconProvider;
    QFileSystemWatcher m_watcher;

    QSet<QString> m_hiddenDirs;
    QSet<QString> m_hiddenFilePatterns;  // exact file names or glob suffixes
    bool m_showHiddenFiles = false;
};

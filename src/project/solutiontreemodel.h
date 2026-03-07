#pragma once

#include <QAbstractItemModel>
#include <QFileIconProvider>
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

public slots:
    void refreshGitOverlays();
    void rebuildFromSolution();

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
    QColor gitColorForPath(const QString &path) const;

    std::unique_ptr<TreeNode> m_root;
    ProjectManager *m_projectManager;
    GitService *m_gitService;
    QFileIconProvider m_iconProvider;
};

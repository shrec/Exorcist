#include "solutiontreemodel.h"

#include <QDir>
#include <QFileInfo>
#include <QBrush>
#include <memory>

#include "projectmanager.h"
#include "git/gitservice.h"

SolutionTreeModel::SolutionTreeModel(ProjectManager *pm, GitService *git, QObject *parent)
    : QAbstractItemModel(parent),
      m_root(std::make_unique<TreeNode>(TreeNode{TreeNode::Root, QString(), QString(), nullptr})),
      m_projectManager(pm),
      m_gitService(git)
{
    buildTree();
    connect(m_projectManager, &ProjectManager::solutionChanged,
            this, &SolutionTreeModel::rebuildFromSolution);
}

SolutionTreeModel::~SolutionTreeModel()
{
    clearTree();
}

QModelIndex SolutionTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return {};
    }

    TreeNode *parentNode = nodeFromIndex(parent);
    if (!parentNode || row < 0 || row >= (int)parentNode->children.size()) {
        return {};
    }

    TreeNode *child = parentNode->children.at(row).get();
    return createIndex(row, column, child);
}

QModelIndex SolutionTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return {};
    }

    TreeNode *node = nodeFromIndex(child);
    if (!node || !node->parent || node->parent == m_root.get()) {
        return {};
    }

    TreeNode *parentNode = node->parent;
    TreeNode *grand = parentNode->parent;
    const int row = grand ? childIndex(grand, parentNode) : 0;
    return createIndex(row, 0, parentNode);
}

int SolutionTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }
    TreeNode *node = nodeFromIndex(parent);
    if (!node) {
        return 0;
    }
    return (int)node->children.size();
}

int SolutionTreeModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant SolutionTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    TreeNode *node = nodeFromIndex(index);
    if (!node) {
        return {};
    }

    if (role == Qt::DisplayRole) {
        if (node->kind == TreeNode::File && m_gitService) {
            const QChar sc = m_gitService->statusChar(node->path);
            if (!sc.isNull() && sc != QLatin1Char(' '))
                return QStringLiteral("%1  %2").arg(node->name, sc);
        }
        return node->name;
    }

    if (role == Qt::DecorationRole) {
        if (node->kind == TreeNode::File) {
            return m_iconProvider.icon(QFileInfo(node->path));
        }
        return m_iconProvider.icon(QFileIconProvider::Folder);
    }

    if (role == Qt::ForegroundRole && node->kind == TreeNode::File) {
        const QColor color = gitColorForPath(node->path);
        if (color.isValid()) {
            return QBrush(color);
        }
    }

    if (role == FilePathRole) {
        if (node->kind == TreeNode::File || node->kind == TreeNode::Dir ||
            node->kind == TreeNode::Project)
            return node->path;
    }

    return {};
}

Qt::ItemFlags SolutionTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool SolutionTreeModel::canFetchMore(const QModelIndex &parent) const
{
    TreeNode *node = nodeFromIndex(parent);
    if (!node) {
        return false;
    }
    return (node->kind == TreeNode::Solution ||
            node->kind == TreeNode::Project ||
            node->kind == TreeNode::Dir) && !node->childrenFetched;
}

void SolutionTreeModel::fetchMore(const QModelIndex &parent)
{
    TreeNode *node = nodeFromIndex(parent);
    if (!node || node->childrenFetched) {
        return;
    }

    fetchChildren(node);
}

bool SolutionTreeModel::hasChildren(const QModelIndex &parent) const
{
    TreeNode *node = nodeFromIndex(parent);
    if (!node)
        return false;
    if (node->kind == TreeNode::Root ||
        node->kind == TreeNode::Solution ||
        node->kind == TreeNode::Project ||
        node->kind == TreeNode::Dir)
        return true;
    return !node->children.empty();
}

QString SolutionTreeModel::filePath(const QModelIndex &index) const
{
    return data(index, FilePathRole).toString();
}

bool SolutionTreeModel::isDir(const QModelIndex &index) const
{
    TreeNode *node = nodeFromIndex(index);
    if (!node) {
        return false;
    }
    return node->kind == TreeNode::Root ||
           node->kind == TreeNode::Solution ||
           node->kind == TreeNode::Project ||
           node->kind == TreeNode::Dir;
}

void SolutionTreeModel::refreshGitOverlays()
{
    beginResetModel();
    endResetModel();
}

void SolutionTreeModel::rebuildFromSolution()
{
    beginResetModel();
    clearTree();
    buildTree();
    endResetModel();
}

SolutionTreeModel::TreeNode *SolutionTreeModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return m_root.get();
    }
    return static_cast<TreeNode *>(index.internalPointer());
}

void SolutionTreeModel::clearTree()
{
    if (!m_root) {
        m_root = std::make_unique<TreeNode>(TreeNode{TreeNode::Root, QString(), QString(), nullptr});
        return;
    }

    m_root->children.clear();
}

void SolutionTreeModel::buildTree()
{
    if (!m_root) {
        m_root = std::make_unique<TreeNode>(TreeNode{TreeNode::Root, QString(), QString(), nullptr});
    }

    const ExSolution solution = m_projectManager->solution();
    auto solutionNode = std::make_unique<TreeNode>();
    solutionNode->kind = TreeNode::Solution;
    solutionNode->name = solution.name.isEmpty() ? tr("Solution") : solution.name;
    solutionNode->path = m_projectManager->activeSolutionDir();
    solutionNode->parent = m_root.get();
    solutionNode->childrenFetched = true;
    TreeNode *solutionPtr = solutionNode.get();
    m_root->children.push_back(std::move(solutionNode));

    for (const ExProject &proj : solution.projects) {
        auto projectNode = std::make_unique<TreeNode>();
        projectNode->kind = TreeNode::Project;
        projectNode->name = proj.name;
        projectNode->path = proj.rootPath;
        projectNode->parent = solutionPtr;
        solutionPtr->children.push_back(std::move(projectNode));
    }
}

void SolutionTreeModel::fetchChildren(TreeNode *node)
{
    if (!node || node->path.isEmpty()) {
        node->childrenFetched = true;
        return;
    }

    QDir dir(node->path);
    const QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                                    QDir::DirsFirst | QDir::Name);

    if (!entries.isEmpty()) {
        QModelIndex parentIndex;
        if (node != m_root.get()) {
            TreeNode *parent = node->parent;
            const int row = parent ? childIndex(parent, node) : 0;
            parentIndex = createIndex(row, 0, node);
        }
        const int first = static_cast<int>(node->children.size());
        beginInsertRows(parentIndex, first, first + entries.size() - 1);
        for (const QFileInfo &info : entries) {
            TreeNode::Kind kind = info.isDir() ? TreeNode::Dir : TreeNode::File;
            auto child = std::make_unique<TreeNode>();
            child->kind = kind;
            child->name = info.fileName();
            child->path = info.absoluteFilePath();
            child->parent = node;
            node->children.push_back(std::move(child));
        }
        endInsertRows();
    }

    node->childrenFetched = true;
}

QColor SolutionTreeModel::gitColorForPath(const QString &path) const
{
    if (!m_gitService || !m_gitService->isGitRepo()) {
        return QColor();
    }

    const QChar status = m_gitService->statusChar(path);
    if (status == 'A') return QColor(80, 200, 120);
    if (status == 'M') return QColor(230, 170, 60);
    if (status == 'D') return QColor(220, 80, 80);
    if (status == '?') return QColor(140, 140, 140);
    return QColor();
}

int SolutionTreeModel::childIndex(const TreeNode *parent, const TreeNode *child) const
{
    if (!parent || !child) {
        return 0;
    }
    for (int i = 0; i < (int)parent->children.size(); ++i) {
        if (parent->children[i].get() == child) {
            return i;
        }
    }
    return 0;
}

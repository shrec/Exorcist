#include "remotefilesystemmodel.h"
#include "sshsession.h"

#include <QApplication>
#include <QStyle>

RemoteFileSystemModel::RemoteFileSystemModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    m_root = std::make_unique<Node>();
    m_root->name = QStringLiteral("/");
    m_root->fullPath = QStringLiteral("/");
    m_root->isDirectory = true;
}

void RemoteFileSystemModel::setSession(SshSession *session)
{
    m_session = session;
}

void RemoteFileSystemModel::setRootPath(const QString &path)
{
    beginResetModel();
    m_rootPath = path;
    m_root = std::make_unique<Node>();
    m_root->name = path;
    m_root->fullPath = path;
    m_root->isDirectory = true;
    m_root->loaded = false;
    endResetModel();
}

QString RemoteFileSystemModel::filePath(const QModelIndex &index) const
{
    const Node *n = nodeFromIndex(index);
    return n ? n->fullPath : QString();
}

bool RemoteFileSystemModel::isDir(const QModelIndex &index) const
{
    const Node *n = nodeFromIndex(index);
    return n ? n->isDirectory : false;
}

// ── QAbstractItemModel ────────────────────────────────────────────────────────

QModelIndex RemoteFileSystemModel::index(int row, int column,
                                          const QModelIndex &parent) const
{
    if (column != 0)
        return {};

    const Node *parentNode = parent.isValid() ? nodeFromIndex(parent) : m_root.get();
    if (!parentNode || row < 0 || row >= static_cast<int>(parentNode->children.size()))
        return {};

    return createIndex(row, 0, parentNode->children.at(row).get());
}

QModelIndex RemoteFileSystemModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return {};

    const Node *childNode = nodeFromIndex(child);
    if (!childNode || !childNode->parent || childNode->parent == m_root.get())
        return {};

    return indexFromNode(childNode->parent);
}

int RemoteFileSystemModel::rowCount(const QModelIndex &parent) const
{
    const Node *node = parent.isValid() ? nodeFromIndex(parent) : m_root.get();
    if (!node)
        return 0;
    return static_cast<int>(node->children.size());
}

int RemoteFileSystemModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant RemoteFileSystemModel::data(const QModelIndex &index, int role) const
{
    const Node *node = nodeFromIndex(index);
    if (!node)
        return {};

    switch (role) {
    case Qt::DisplayRole:
        return node->name;
    case Qt::DecorationRole:
        return node->isDirectory ? folderIcon() : fileIcon();
    case Qt::ToolTipRole:
        return node->fullPath;
    default:
        return {};
    }
}

bool RemoteFileSystemModel::hasChildren(const QModelIndex &parent) const
{
    const Node *node = parent.isValid() ? nodeFromIndex(parent) : m_root.get();
    if (!node)
        return false;
    if (!node->isDirectory)
        return false;
    // If not yet loaded, assume it might have children
    if (!node->loaded)
        return true;
    return !node->children.empty();
}

bool RemoteFileSystemModel::canFetchMore(const QModelIndex &parent) const
{
    const Node *node = parent.isValid() ? nodeFromIndex(parent) : m_root.get();
    return node && node->isDirectory && !node->loaded && !node->loading;
}

void RemoteFileSystemModel::fetchMore(const QModelIndex &parent)
{
    if (!m_session)
        return;

    Node *node = parent.isValid() ? nodeFromIndex(parent) : m_root.get();
    if (!node || node->loading || node->loaded)
        return;

    node->loading = true;

    const QString path = node->fullPath;
    m_session->listDirectory(path);

    // Connect once for this listing
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_session, &SshSession::directoryListed,
                    this, [this, node, path, parent, conn]
                    (const QString &listedPath, const QStringList &entries,
                     const QString &error) {
        if (listedPath != path)
            return;

        QObject::disconnect(*conn);
        node->loading = false;

        if (!error.isEmpty()) {
            node->loaded = true;
            emit loadError(path, error);
            return;
        }

        if (!entries.isEmpty()) {
            const QModelIndex idx = parent.isValid() ? parent : QModelIndex();
            beginInsertRows(idx, 0, entries.size() - 1);

            for (const auto &entry : entries) {
                auto child = std::make_unique<Node>();
                child->parent = node;

                // Entries from ls -1aF end with '/' for directories
                if (entry.endsWith(QLatin1Char('/'))) {
                    child->name = entry.chopped(1);
                    child->isDirectory = true;
                } else if (entry.endsWith(QLatin1Char('*'))) {
                    // Executable marker
                    child->name = entry.chopped(1);
                    child->isDirectory = false;
                } else if (entry.endsWith(QLatin1Char('@'))) {
                    // Symlink
                    child->name = entry.chopped(1);
                    child->isDirectory = false;
                } else {
                    child->name = entry;
                    child->isDirectory = false;
                }

                // Build full path
                if (path.endsWith(QLatin1Char('/')))
                    child->fullPath = path + child->name;
                else
                    child->fullPath = path + QLatin1Char('/') + child->name;

                node->children.push_back(std::move(child));
            }

            endInsertRows();
        }

        node->loaded = true;
    });
}

// ── Helpers ───────────────────────────────────────────────────────────────────

RemoteFileSystemModel::Node *RemoteFileSystemModel::nodeFromIndex(
    const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;
    return static_cast<Node *>(index.internalPointer());
}

QModelIndex RemoteFileSystemModel::indexFromNode(Node *node) const
{
    if (!node || !node->parent)
        return {};

    const auto &siblings = node->parent->children;
    for (int i = 0; i < static_cast<int>(siblings.size()); ++i) {
        if (siblings.at(i).get() == node)
            return createIndex(i, 0, node);
    }
    return {};
}

QIcon RemoteFileSystemModel::fileIcon()
{
    static QIcon icon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    return icon;
}

QIcon RemoteFileSystemModel::folderIcon()
{
    static QIcon icon = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
    return icon;
}

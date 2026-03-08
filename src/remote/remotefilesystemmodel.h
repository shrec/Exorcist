#pragma once

#include <QAbstractItemModel>
#include <QIcon>
#include <QMap>
#include <QString>
#include <memory>
#include <vector>

class SshSession;

/// A lazy-loading tree model for browsing remote file systems over SSH.
///
/// Nodes are loaded on-demand when expanded. Each node stores its children
/// and loading state. Directory entries ending with '/' are treated as folders.
class RemoteFileSystemModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit RemoteFileSystemModel(QObject *parent = nullptr);

    /// Set the SSH session to use for file operations.
    void setSession(SshSession *session);

    /// Set the root path to display.
    void setRootPath(const QString &path);

    QString rootPath() const { return m_rootPath; }

    /// Get the full remote path for an index.
    QString filePath(const QModelIndex &index) const;

    /// True if the index represents a directory.
    bool isDir(const QModelIndex &index) const;

    // ── QAbstractItemModel overrides ──────────────────────────────────────

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex &parent = {}) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

signals:
    void loadError(const QString &path, const QString &error);

private:
    struct Node {
        QString   name;
        QString   fullPath;
        bool      isDirectory = false;
        bool      loaded = false;
        bool      loading = false;
        Node     *parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
    };

    Node *nodeFromIndex(const QModelIndex &index) const;
    QModelIndex indexFromNode(Node *node) const;

    std::unique_ptr<Node> m_root;
    SshSession           *m_session = nullptr;
    QString               m_rootPath;

    static QIcon fileIcon();
    static QIcon folderIcon();
};

#pragma once

#include <QObject>
#include <QString>

class QWidget;

// ── Search Service ───────────────────────────────────────────────────────────
//
// Stable SDK interface for workspace search, file indexing, and symbol lookup.
// Implemented by plugins/search/. Registered as "searchService".
//
// Companion services registered by the same plugin:
//   "symbolIndex"       → SymbolIndex* (dynamic_cast)
//   "workspaceIndexer"  → WorkspaceIndexer* (dynamic_cast)

class ISearchService : public QObject
{
    Q_OBJECT

public:
    explicit ISearchService(QObject *parent = nullptr) : QObject(parent) {}
    ~ISearchService() override = default;

    /// Set the workspace root path for file-content search (SearchPanel).
    virtual void setRootPath(const QString &path) = 0;

    /// Focus and activate the search input (Ctrl+Shift+F).
    virtual void activateSearch() = 0;

    /// Start indexing the workspace root asynchronously (WorkspaceIndexer).
    virtual void indexWorkspace(const QString &rootPath) = 0;

    /// Reindex a single file in the workspace index.
    virtual void reindexFile(const QString &filePath) = 0;

    /// Return the search panel widget (used for dock screen grabs).
    virtual QWidget *searchPanel() const = 0;

signals:
    /// Emitted when the user activates a search result.
    void resultActivated(const QString &filePath, int line, int column);
};

#pragma once

#include "../itool.h"

#include <QHash>
#include <QString>

#include <memory>

// ── Atomic Multi-File Transaction Tools ───────────────────────────────────────
//
// These three tools provide all-or-nothing multi-file editing:
//
//   begin_transaction  — snapshot all files that will be modified and open a
//                        transaction. Subsequent write_file / create_file calls
//                        in the same agent turn are tracked automatically.
//
//   commit_transaction — declare success; the current file state is the final
//                        result. Clears the snapshot store.
//
//   rollback_transaction — restore every file modified since begin_transaction
//                          to its pre-transaction content.
//
// The TransactionStore is a shared, heap-allocated object that all three tools
// reference. The agent bootstrap creates one instance and passes it to all three.

struct TransactionStore
{
    bool active = false;

    /// path → original content (UTF-8 text).
    QHash<QString, QByteArray> snapshots;

    /// Register original content of a file before it is first modified.
    /// Subsequent modifications to the same file do not overwrite the snapshot.
    void recordIfNew(const QString &path, const QByteArray &original);

    /// Restore all snapshotted files from disk, then clear the store.
    /// Returns list of files that failed to restore.
    QStringList rollback();

    /// Discard snapshots without touching files.
    void commit();
};

// ── BeginTransactionTool ──────────────────────────────────────────────────────

class BeginTransactionTool : public ITool
{
public:
    explicit BeginTransactionTool(std::shared_ptr<TransactionStore> store);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    std::shared_ptr<TransactionStore> m_store;
};

// ── CommitTransactionTool ─────────────────────────────────────────────────────

class CommitTransactionTool : public ITool
{
public:
    explicit CommitTransactionTool(std::shared_ptr<TransactionStore> store);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    std::shared_ptr<TransactionStore> m_store;
};

// ── RollbackTransactionTool ───────────────────────────────────────────────────

class RollbackTransactionTool : public ITool
{
public:
    explicit RollbackTransactionTool(std::shared_ptr<TransactionStore> store);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    std::shared_ptr<TransactionStore> m_store;
};

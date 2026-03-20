#include "transactiontool.h"

#include <QFile>
#include <QJsonArray>

// ── TransactionStore ──────────────────────────────────────────────────────────

void TransactionStore::recordIfNew(const QString &path, const QByteArray &original)
{
    if (!snapshots.contains(path))
        snapshots.insert(path, original);
}

QStringList TransactionStore::rollback()
{
    QStringList failed;
    for (auto it = snapshots.cbegin(); it != snapshots.cend(); ++it) {
        QFile f(it.key());
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            failed.append(it.key());
            continue;
        }
        f.write(it.value());
        f.close();
    }
    snapshots.clear();
    active = false;
    return failed;
}

void TransactionStore::commit()
{
    snapshots.clear();
    active = false;
}

// ── BeginTransactionTool ──────────────────────────────────────────────────────

BeginTransactionTool::BeginTransactionTool(std::shared_ptr<TransactionStore> store)
    : m_store(std::move(store))
{}

ToolSpec BeginTransactionTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("begin_transaction");
    s.description = QStringLiteral(
        "Begin an atomic multi-file transaction. All file writes made after this call\n"
        "can be rolled back atomically if an error occurs. Call commit_transaction when\n"
        "all changes are complete, or rollback_transaction to undo everything.\n\n"
        "Pattern:\n"
        "  1. begin_transaction()\n"
        "  2. write_file(...), create_file(...) — make changes\n"
        "  3. build / test — verify\n"
        "  4a. commit_transaction() — success, keep changes\n"
        "  4b. rollback_transaction() — failure, restore all files\n\n"
        "Only files that were modified AFTER begin_transaction are tracked.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{}},
        {QStringLiteral("required"), QJsonArray{}}
    };
    return s;
}

ToolExecResult BeginTransactionTool::invoke(const QJsonObject &)
{
    if (m_store->active) {
        // Already active — rollback previous before starting fresh
        m_store->rollback();
        return {true, {},
                QStringLiteral("Previous transaction rolled back. New transaction started."), {}};
    }
    m_store->active = true;
    m_store->snapshots.clear();
    return {true, {}, QStringLiteral("Transaction started. File edits are now tracked."), {}};
}

// ── CommitTransactionTool ─────────────────────────────────────────────────────

CommitTransactionTool::CommitTransactionTool(std::shared_ptr<TransactionStore> store)
    : m_store(std::move(store))
{}

ToolSpec CommitTransactionTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("commit_transaction");
    s.description = QStringLiteral(
        "Commit the current transaction — declare all file changes successful.\n"
        "Clears the undo snapshots. Call this after verifying that all changes work.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{}},
        {QStringLiteral("required"), QJsonArray{}}
    };
    return s;
}

ToolExecResult CommitTransactionTool::invoke(const QJsonObject &)
{
    if (!m_store->active)
        return {false, {}, {}, QStringLiteral("No active transaction. Call begin_transaction first.")};

    const int count = m_store->snapshots.size();
    m_store->commit();
    return {true, {},
            QStringLiteral("Transaction committed. %1 file(s) finalized.").arg(count), {}};
}

// ── RollbackTransactionTool ───────────────────────────────────────────────────

RollbackTransactionTool::RollbackTransactionTool(std::shared_ptr<TransactionStore> store)
    : m_store(std::move(store))
{}

ToolSpec RollbackTransactionTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("rollback_transaction");
    s.description = QStringLiteral(
        "Rollback the current transaction — restore all files to the state they were\n"
        "in when begin_transaction was called.\n"
        "Use this when a refactoring step fails: ensure no half-applied changes remain.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 30000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{}},
        {QStringLiteral("required"), QJsonArray{}}
    };
    return s;
}

ToolExecResult RollbackTransactionTool::invoke(const QJsonObject &)
{
    if (!m_store->active)
        return {false, {}, {}, QStringLiteral("No active transaction. Call begin_transaction first.")};

    const int total = m_store->snapshots.size();
    const QStringList failed = m_store->rollback();

    if (!failed.isEmpty()) {
        return {false, {},
                QStringLiteral("Rollback partially failed. Could not restore %1 file(s):\n%2")
                    .arg(failed.size())
                    .arg(failed.join(QLatin1Char('\n'))),
                {}};
    }

    return {true, {},
            QStringLiteral("Rolled back %1 file(s) to pre-transaction state.").arg(total), {}};
}

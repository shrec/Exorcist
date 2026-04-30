#pragma once

#include "sdk/idebugadapter.h"

#include <QAbstractItemModel>
#include <QList>
#include <QHash>
#include <memory>

class IDebugAdapter;

/// Tree node backing a variable in the Watch / Locals tree.
/// Each node maps 1:1 to a GDB/MI variable object.
struct WatchNode
{
    QString    varName;        // GDB/MI var-object name
    QString    expression;     // display expression
    QString    value;
    QString    type;
    int        numChildren = 0;
    bool       childrenFetched = false;
    bool       changed = false;

    WatchNode *parent = nullptr;
    std::vector<std::unique_ptr<WatchNode>> children;
};

/// A QAbstractItemModel that lazily expands variable objects via
/// GDB/MI `-var-create` / `-var-list-children`.
///
/// Used for both the Watch panel (user expressions) and the Locals panel.
class WatchTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column { ColName = 0, ColValue, ColType, ColumnCount };

    explicit WatchTreeModel(QObject *parent = nullptr);

    /// Bind to a debug adapter. Connects var-obj signals.
    void setAdapter(IDebugAdapter *adapter);

    // ── Public API ────────────────────────────────────────────────────────

    /// Add a top-level watch expression. Creates a var-object via the adapter.
    void addWatch(const QString &expression);

    /// Remove a top-level watch by its expression string.
    void removeWatch(const QString &expression);

    /// Clear all watches and delete their var-objects.
    void clearAll();

    /// Replace top-level entries with the given locals snapshot. Populates
    /// the tree synchronously (no async var-object round-trip), so the
    /// Locals panel shows values immediately when GDB stops. Use this for
    /// frame-locals; addWatch() is still appropriate for user-typed
    /// expressions that need expandable structure via var-objects.
    void setLocals(const QList<DebugVariable> &locals);

    /// Called when the debugger stops — issues `-var-update *`.
    void onDebuggerStopped();

    /// Called when the debug session ends — resets model.
    void onSessionEnded();

    /// Returns all current top-level expressions.
    QStringList watchExpressions() const;

    /// Allow editing the Value column.
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // ── QAbstractItemModel interface ──────────────────────────────────────

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex &parent = {}) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

private slots:
    void onVarObjectCreated(const DebugVarObj &varObj);
    void onVarChildrenListed(const QString &parentVarName,
                             const QList<DebugVarObj> &children);
    void onVarObjectsUpdated(const QList<DebugVarChange> &changes);
    void onVarObjectDeleted(const QString &varName);
    void onVarValueAssigned(const QString &varName, const QString &newValue);
    void onVarObjectError(const QString &expression, const QString &message);

private:
    WatchNode *nodeFromIndex(const QModelIndex &index) const;
    QModelIndex indexForNode(WatchNode *node, int column = 0) const;
    WatchNode *findNodeByVarName(const QString &varName) const;
    WatchNode *findNodeByVarNameRecursive(WatchNode *node, const QString &varName) const;

    /// Heuristic: does this type look complex (struct/class/array/pointer/reference)
    /// and therefore worth creating a var-object for so it can be expanded?
    static bool typeLooksExpandable(const QString &type);

    IDebugAdapter *m_adapter = nullptr;

    /// Virtual root that holds all top-level watch nodes.
    std::unique_ptr<WatchNode> m_root;

    /// Quick lookup: var-object name → WatchNode*
    QHash<QString, WatchNode *> m_varNameMap;

    /// Pending creates: expression → (waiting for varObjectCreated)
    QHash<QString, bool> m_pendingCreates;
};

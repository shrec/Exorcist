#include "watchtreemodel.h"

#include <QBrush>
#include <QFont>

WatchTreeModel::WatchTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_root(std::make_unique<WatchNode>())
{
}

void WatchTreeModel::setAdapter(IDebugAdapter *adapter)
{
    if (m_adapter)
        disconnect(m_adapter, nullptr, this, nullptr);

    m_adapter = adapter;
    if (!m_adapter) return;

    connect(m_adapter, &IDebugAdapter::varObjectCreated,
            this, &WatchTreeModel::onVarObjectCreated);
    connect(m_adapter, &IDebugAdapter::varChildrenListed,
            this, &WatchTreeModel::onVarChildrenListed);
    connect(m_adapter, &IDebugAdapter::varObjectsUpdated,
            this, &WatchTreeModel::onVarObjectsUpdated);
    connect(m_adapter, &IDebugAdapter::varObjectDeleted,
            this, &WatchTreeModel::onVarObjectDeleted);
    connect(m_adapter, &IDebugAdapter::varValueAssigned,
            this, &WatchTreeModel::onVarValueAssigned);
    connect(m_adapter, &IDebugAdapter::varObjectError,
            this, &WatchTreeModel::onVarObjectError);
}

// ── Public API ────────────────────────────────────────────────────────────────

void WatchTreeModel::addWatch(const QString &expression)
{
    if (!m_adapter || expression.isEmpty()) return;

    // Don't add duplicates
    for (auto &child : m_root->children) {
        if (child->expression == expression) return;
    }

    m_pendingCreates.insert(expression, true);
    m_adapter->createVarObject(expression);
}

void WatchTreeModel::removeWatch(const QString &expression)
{
    for (size_t i = 0; i < m_root->children.size(); ++i) {
        if (m_root->children[i]->expression == expression) {
            const QString varName = m_root->children[i]->varName;
            beginRemoveRows(QModelIndex(), static_cast<int>(i), static_cast<int>(i));
            // Remove from map recursively
            std::function<void(WatchNode *)> removeFromMap = [&](WatchNode *n) {
                m_varNameMap.remove(n->varName);
                for (auto &c : n->children)
                    removeFromMap(c.get());
            };
            removeFromMap(m_root->children[i].get());
            m_root->children.erase(m_root->children.begin() + static_cast<long>(i));
            endRemoveRows();

            // Delete the var-object on the GDB side
            if (m_adapter && !varName.isEmpty())
                m_adapter->deleteVarObject(varName);
            return;
        }
    }
}

void WatchTreeModel::clearAll()
{
    if (m_root->children.empty()) return;

    beginResetModel();
    // Delete all var-objects on GDB side
    if (m_adapter) {
        for (auto &child : m_root->children) {
            if (!child->varName.isEmpty())
                m_adapter->deleteVarObject(child->varName);
        }
    }
    m_root->children.clear();
    m_varNameMap.clear();
    m_pendingCreates.clear();
    endResetModel();
}

void WatchTreeModel::onDebuggerStopped()
{
    if (m_adapter) {
        // Re-create var objects for all watches (they may have gone out of scope)
        if (m_root->children.empty()) return;
        m_adapter->updateVarObjects();
    }
}

void WatchTreeModel::onSessionEnded()
{
    beginResetModel();
    // Keep expressions but clear var-object state
    for (auto &child : m_root->children) {
        child->varName.clear();
        child->value.clear();
        child->type.clear();
        child->numChildren = 0;
        child->childrenFetched = false;
        child->changed = false;
        child->children.clear();
    }
    m_varNameMap.clear();
    m_pendingCreates.clear();
    endResetModel();
}

QStringList WatchTreeModel::watchExpressions() const
{
    QStringList result;
    for (auto &child : m_root->children)
        result.append(child->expression);
    return result;
}

// ── QAbstractItemModel ───────────────────────────────────────────────────────

QModelIndex WatchTreeModel::index(int row, int column,
                                   const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    WatchNode *parentNode = parent.isValid()
        ? nodeFromIndex(parent)
        : m_root.get();

    if (row < 0 || row >= static_cast<int>(parentNode->children.size()))
        return {};

    return createIndex(row, column, parentNode->children[static_cast<size_t>(row)].get());
}

QModelIndex WatchTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) return {};

    auto *childNode = nodeFromIndex(child);
    if (!childNode || !childNode->parent || childNode->parent == m_root.get())
        return {};

    auto *parentNode = childNode->parent;
    auto *grandParent = parentNode->parent ? parentNode->parent : m_root.get();

    // Find row of parent in grandparent
    for (size_t i = 0; i < grandParent->children.size(); ++i) {
        if (grandParent->children[i].get() == parentNode)
            return createIndex(static_cast<int>(i), 0, parentNode);
    }

    return {};
}

int WatchTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;

    WatchNode *node = parent.isValid() ? nodeFromIndex(parent) : m_root.get();
    return node ? static_cast<int>(node->children.size()) : 0;
}

int WatchTreeModel::columnCount(const QModelIndex &/*parent*/) const
{
    return ColumnCount;
}

QVariant WatchTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};

    auto *node = nodeFromIndex(index);
    if (!node) return {};

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case ColName:  return node->expression;
        case ColValue: return node->value;
        case ColType:  return node->type;
        }
    }

    // Highlight changed values in red
    if (role == Qt::ForegroundRole && index.column() == ColValue && node->changed)
        return QBrush(QColor(0xE5, 0x39, 0x35)); // red-ish

    // Monospace font for value column
    if (role == Qt::FontRole && index.column() == ColValue) {
        QFont f(QStringLiteral("Consolas"));
        f.setStyleHint(QFont::Monospace);
        return f;
    }

    return {};
}

QVariant WatchTreeModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ColName:  return tr("Name");
    case ColValue: return tr("Value");
    case ColType:  return tr("Type");
    }
    return {};
}

bool WatchTreeModel::hasChildren(const QModelIndex &parent) const
{
    WatchNode *node = parent.isValid() ? nodeFromIndex(parent) : m_root.get();
    if (!node) return false;

    // Has children if numChildren > 0, even if not yet fetched
    return node->numChildren > 0 || !node->children.empty();
}

bool WatchTreeModel::canFetchMore(const QModelIndex &parent) const
{
    WatchNode *node = parent.isValid() ? nodeFromIndex(parent) : nullptr;
    if (!node) return false;

    return node->numChildren > 0 && !node->childrenFetched;
}

void WatchTreeModel::fetchMore(const QModelIndex &parent)
{
    WatchNode *node = parent.isValid() ? nodeFromIndex(parent) : nullptr;
    if (!node || !m_adapter) return;

    if (node->numChildren > 0 && !node->childrenFetched) {
        node->childrenFetched = true; // prevent multiple fetches
        m_adapter->listVarChildren(node->varName);
    }
}

Qt::ItemFlags WatchTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    auto f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    // Allow editing the value column
    if (index.column() == ColValue)
        f |= Qt::ItemIsEditable;

    return f;
}

bool WatchTreeModel::setData(const QModelIndex &index, const QVariant &value,
                              int role)
{
    if (!index.isValid() || role != Qt::EditRole || index.column() != ColValue)
        return false;

    auto *node = nodeFromIndex(index);
    if (!node || !m_adapter || node->varName.isEmpty())
        return false;

    const QString newVal = value.toString();
    m_adapter->assignVarValue(node->varName, newVal);
    return true; // will be confirmed via varValueAssigned signal
}

// ── Adapter signal handlers ──────────────────────────────────────────────────

void WatchTreeModel::onVarObjectCreated(const DebugVarObj &varObj)
{
    // Check if this is a response to one of our pending creates
    if (!m_pendingCreates.remove(varObj.expression)) {
        // Not ours — might be from a different model (e.g. hover tooltip)
        // Still register if we have the expression
        bool found = false;
        for (auto &child : m_root->children) {
            if (child->expression == varObj.expression) {
                found = true;
                break;
            }
        }
        if (!found) return;
    }

    // Check if we already have a node for this expression (re-create scenario)
    for (size_t i = 0; i < m_root->children.size(); ++i) {
        auto *existing = m_root->children[i].get();
        if (existing->expression == varObj.expression) {
            existing->varName = varObj.varName;
            existing->value = varObj.value;
            existing->type = varObj.type;
            existing->numChildren = varObj.numChildren;
            existing->childrenFetched = false;
            existing->changed = false;
            existing->children.clear();
            m_varNameMap.insert(varObj.varName, existing);
            const QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, index(static_cast<int>(i), ColumnCount - 1));
            return;
        }
    }

    // New top-level entry
    const int row = static_cast<int>(m_root->children.size());
    beginInsertRows(QModelIndex(), row, row);

    auto node = std::make_unique<WatchNode>();
    node->varName     = varObj.varName;
    node->expression  = varObj.expression;
    node->value       = varObj.value;
    node->type        = varObj.type;
    node->numChildren = varObj.numChildren;
    node->parent      = m_root.get();

    m_varNameMap.insert(varObj.varName, node.get());
    m_root->children.push_back(std::move(node));

    endInsertRows();
}

void WatchTreeModel::onVarChildrenListed(const QString &parentVarName,
                                          const QList<DebugVarObj> &children)
{
    auto *parentNode = findNodeByVarName(parentVarName);
    if (!parentNode) return;

    const QModelIndex parentIdx = indexForNode(parentNode);

    if (!children.isEmpty()) {
        const int first = static_cast<int>(parentNode->children.size());
        const int last  = first + children.size() - 1;
        beginInsertRows(parentIdx, first, last);

        for (const auto &child : children) {
            auto node = std::make_unique<WatchNode>();
            node->varName     = child.varName;
            node->expression  = child.expression;
            node->value       = child.value;
            node->type        = child.type;
            node->numChildren = child.numChildren;
            node->parent      = parentNode;

            m_varNameMap.insert(child.varName, node.get());
            parentNode->children.push_back(std::move(node));
        }

        endInsertRows();
    }
}

void WatchTreeModel::onVarObjectsUpdated(const QList<DebugVarChange> &changes)
{
    // First, clear all change flags
    for (auto it = m_varNameMap.begin(); it != m_varNameMap.end(); ++it)
        it.value()->changed = false;

    for (const auto &chg : changes) {
        auto *node = findNodeByVarName(chg.varName);
        if (!node) continue;

        if (!chg.inScope) {
            // Variable went out of scope
            node->value = tr("<out of scope>");
            node->changed = true;
        } else {
            node->value   = chg.newValue;
            node->changed = true;

            if (chg.typeChanged || chg.newNumChildren >= 0) {
                // Children changed — need re-fetch
                const QModelIndex idx = indexForNode(node);
                if (!node->children.empty()) {
                    beginRemoveRows(idx, 0, static_cast<int>(node->children.size()) - 1);
                    // Remove children from map
                    std::function<void(WatchNode *)> removeFromMap = [&](WatchNode *n) {
                        m_varNameMap.remove(n->varName);
                        for (auto &c : n->children)
                            removeFromMap(c.get());
                    };
                    for (auto &c : node->children)
                        removeFromMap(c.get());
                    node->children.clear();
                    endRemoveRows();
                }
                node->childrenFetched = false;
                if (chg.newNumChildren >= 0)
                    node->numChildren = chg.newNumChildren;
            }
        }

        const QModelIndex idx = indexForNode(node);
        if (idx.isValid())
            emit dataChanged(idx, indexForNode(node, ColumnCount - 1));
    }
}

void WatchTreeModel::onVarObjectDeleted(const QString &varName)
{
    m_varNameMap.remove(varName);
}

void WatchTreeModel::onVarValueAssigned(const QString &varName, const QString &newValue)
{
    auto *node = findNodeByVarName(varName);
    if (!node) return;

    node->value   = newValue;
    node->changed = true;

    const QModelIndex idx = indexForNode(node);
    if (idx.isValid())
        emit dataChanged(idx, indexForNode(node, ColumnCount - 1));
}

void WatchTreeModel::onVarObjectError(const QString &expression,
                                       const QString &message)
{
    m_pendingCreates.remove(expression);

    // If there's an existing node for this expression, show the error
    for (size_t i = 0; i < m_root->children.size(); ++i) {
        if (m_root->children[i]->expression == expression) {
            m_root->children[i]->value = tr("<error: %1>").arg(message);
            const QModelIndex idx = index(static_cast<int>(i), ColValue);
            emit dataChanged(idx, idx);
            return;
        }
    }
}

// ── Helpers ──────────────────────────────────────────────────────────────────

WatchNode *WatchTreeModel::nodeFromIndex(const QModelIndex &index) const
{
    return index.isValid()
        ? static_cast<WatchNode *>(index.internalPointer())
        : nullptr;
}

QModelIndex WatchTreeModel::indexForNode(WatchNode *node, int column) const
{
    if (!node || node == m_root.get()) return {};

    auto *parentNode = node->parent ? node->parent : m_root.get();
    for (size_t i = 0; i < parentNode->children.size(); ++i) {
        if (parentNode->children[i].get() == node)
            return createIndex(static_cast<int>(i), column, node);
    }
    return {};
}

WatchNode *WatchTreeModel::findNodeByVarName(const QString &varName) const
{
    return m_varNameMap.value(varName, nullptr);
}

WatchNode *WatchTreeModel::findNodeByVarNameRecursive(WatchNode *node,
                                                       const QString &varName) const
{
    if (!node) return nullptr;
    if (node->varName == varName) return node;
    for (auto &c : node->children) {
        if (auto *found = findNodeByVarNameRecursive(c.get(), varName))
            return found;
    }
    return nullptr;
}

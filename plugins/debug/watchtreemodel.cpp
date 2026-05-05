#include "watchtreemodel.h"

#include <QBrush>
#include <QFont>
#include <QSet>

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

    // SIGNAL/SLOT string syntax — PMF connect fails across DLL boundary
    // (IDebugAdapter MOC is in exe, WatchTreeModel is in libdebug.dll).
    connect(m_adapter, SIGNAL(varObjectCreated(DebugVarObj)),
            this, SLOT(onVarObjectCreated(DebugVarObj)));
    connect(m_adapter, SIGNAL(varChildrenListed(QString,QList<DebugVarObj>)),
            this, SLOT(onVarChildrenListed(QString,QList<DebugVarObj>)));
    connect(m_adapter, SIGNAL(varObjectsUpdated(QList<DebugVarChange>)),
            this, SLOT(onVarObjectsUpdated(QList<DebugVarChange>)));
    connect(m_adapter, SIGNAL(varObjectDeleted(QString)),
            this, SLOT(onVarObjectDeleted(QString)));
    connect(m_adapter, SIGNAL(varValueAssigned(QString,QString)),
            this, SLOT(onVarValueAssigned(QString,QString)));
    connect(m_adapter, SIGNAL(varObjectError(QString,QString)),
            this, SLOT(onVarObjectError(QString,QString)));
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

void WatchTreeModel::setLocals(const QList<DebugVariable> &locals)
{
    beginResetModel();

    // Delete any prior var-objects on GDB side (best-effort cleanup).
    if (m_adapter) {
        for (auto &child : m_root->children) {
            if (!child->varName.isEmpty())
                m_adapter->deleteVarObject(child->varName);
        }
    }
    m_root->children.clear();
    m_varNameMap.clear();
    m_pendingCreates.clear();

    // Populate directly with the snapshot — no async round-trip.
    // For complex-typed locals (struct/class/array/pointer/reference) we
    // additionally fire `createVarObject` so the user can lazy-expand the
    // node via `-var-list-children`. The flat value/type from the snapshot
    // is shown immediately; the var-object name is attached when the
    // adapter responds (see onVarObjectCreated).
    QStringList complexExpressions;
    for (const DebugVariable &v : locals) {
        if (v.name.isEmpty()) continue;
        auto node = std::make_unique<WatchNode>();
        node->expression = v.name;
        node->value      = v.value;
        node->type       = v.type;
        node->parent     = m_root.get();

        if (m_adapter && typeLooksExpandable(v.type)) {
            m_pendingCreates.insert(v.name, true);
            complexExpressions.append(v.name);
        }

        m_root->children.push_back(std::move(node));
    }

    endResetModel();

    // Fire the var-object creates AFTER endResetModel so the adapter's
    // synchronous-style replies (if any) update an already-visible tree.
    if (m_adapter) {
        for (const QString &expr : complexExpressions)
            m_adapter->createVarObject(expr);
    }
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

    // Already populated children — definitely has children.
    if (!node->children.empty()) return true;

    // GDB confirmed there are children to fetch.
    if (node->numChildren > 0) return true;

    // Var-object exists but the create response is still in flight or the
    // children count hasn't been resolved yet — assume there might be
    // children so the expand arrow shows. If the type turns out to be
    // primitive (numChildren == 0), the arrow disappears once the listing
    // completes.
    if (!node->varName.isEmpty()) return true;

    return false;
}

bool WatchTreeModel::canFetchMore(const QModelIndex &parent) const
{
    if (!parent.isValid()) return false;
    WatchNode *node = nodeFromIndex(parent);
    if (!node) return false;

    // Need a live var-object to ask GDB for children.
    if (node->varName.isEmpty()) return false;
    if (node->childrenFetched)   return false;
    return node->numChildren > 0;
}

void WatchTreeModel::fetchMore(const QModelIndex &parent)
{
    if (!parent.isValid() || !m_adapter) return;
    WatchNode *node = nodeFromIndex(parent);
    if (!node || node->varName.isEmpty()) return;

    if (node->numChildren > 0 && !node->childrenFetched) {
        node->childrenFetched = true; // prevent multiple fetches
        m_adapter->listVarChildren(node->varName, 0, node->numChildren);
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
    // Whether this varObj corresponds to a pending create we issued.
    const bool wasPending = m_pendingCreates.remove(varObj.expression);

    // First, try to attach to an existing top-level node with the same
    // expression. This covers BOTH paths:
    //   - addWatch():   row was inserted with empty varName at addWatch time
    //                   (currently skipped — see below) or via a re-create.
    //   - setLocals():  row was inserted synchronously (with type/value but
    //                   no varName); now we attach the GDB var-obj name and
    //                   record numChildren so the expand arrow works.
    for (size_t i = 0; i < m_root->children.size(); ++i) {
        auto *existing = m_root->children[i].get();
        if (existing->expression != varObj.expression) continue;

        // If the existing node already has a varName different from this
        // one, drop the old mapping so we don't leak it.
        if (!existing->varName.isEmpty() && existing->varName != varObj.varName)
            m_varNameMap.remove(existing->varName);

        existing->varName     = varObj.varName;
        // Prefer the var-object's value/type if the snapshot didn't carry
        // them; otherwise keep the snapshot strings (they were just shown
        // to the user and refreshing them on every var-create would flicker).
        if (existing->value.isEmpty() || !varObj.value.isEmpty())
            existing->value = varObj.value;
        if (existing->type.isEmpty() || !varObj.type.isEmpty())
            existing->type = varObj.type;
        existing->numChildren     = varObj.numChildren;
        existing->childrenFetched = false;
        existing->changed         = false;
        existing->children.clear();

        m_varNameMap.insert(varObj.varName, existing);

        const QModelIndex topLeft  = index(static_cast<int>(i), 0);
        const QModelIndex botRight = index(static_cast<int>(i), ColumnCount - 1);
        emit dataChanged(topLeft, botRight);
        return;
    }

    // No existing node — only insert a new row if this var-obj was created
    // as a response to a pending request from us. Otherwise the var-obj
    // was meant for a different model (e.g. hover tooltip) and we ignore.
    if (!wasPending) return;

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

bool WatchTreeModel::typeLooksExpandable(const QString &type)
{
    if (type.isEmpty()) return false;

    // Pointers, references, and arrays are always expandable in GDB.
    if (type.contains(QLatin1Char('*'))) return true;
    if (type.contains(QLatin1Char('&'))) return true;
    if (type.contains(QLatin1Char('['))) return true;

    // Heuristic: anything that's not a known scalar/primitive is treated as
    // expandable. GDB will set numChildren=0 for the var-object if the type
    // turns out to be primitive after all, and the expand arrow disappears.
    static const QSet<QString> primitives = {
        QStringLiteral("char"),
        QStringLiteral("signed char"),
        QStringLiteral("unsigned char"),
        QStringLiteral("short"),
        QStringLiteral("unsigned short"),
        QStringLiteral("short int"),
        QStringLiteral("unsigned short int"),
        QStringLiteral("int"),
        QStringLiteral("unsigned int"),
        QStringLiteral("unsigned"),
        QStringLiteral("long"),
        QStringLiteral("unsigned long"),
        QStringLiteral("long int"),
        QStringLiteral("unsigned long int"),
        QStringLiteral("long long"),
        QStringLiteral("unsigned long long"),
        QStringLiteral("long long int"),
        QStringLiteral("unsigned long long int"),
        QStringLiteral("float"),
        QStringLiteral("double"),
        QStringLiteral("long double"),
        QStringLiteral("bool"),
        QStringLiteral("void"),
        QStringLiteral("size_t"),
        QStringLiteral("ssize_t"),
        QStringLiteral("ptrdiff_t"),
        QStringLiteral("int8_t"),
        QStringLiteral("uint8_t"),
        QStringLiteral("int16_t"),
        QStringLiteral("uint16_t"),
        QStringLiteral("int32_t"),
        QStringLiteral("uint32_t"),
        QStringLiteral("int64_t"),
        QStringLiteral("uint64_t"),
    };

    const QString trimmed = type.trimmed();
    if (primitives.contains(trimmed)) return false;

    // Anything else (struct, class, union, std::string, custom typedefs,
    // template instantiations) — let GDB tell us via numChildren.
    return true;
}

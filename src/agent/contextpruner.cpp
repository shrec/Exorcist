#include "contextpruner.h"

#include <algorithm>

ContextPruner::ContextPruner(QObject *parent)
    : QObject(parent)
{
}

void ContextPruner::addItem(const ContextItem &item)
{
    // Replace if key already exists
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].key == item.key) {
            m_items[i] = item;
            emit itemsChanged();
            return;
        }
    }
    m_items.append(item);
    emit itemsChanged();
}

void ContextPruner::removeItem(const QString &key)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].key == key) {
            m_items.removeAt(i);
            emit itemsChanged();
            return;
        }
    }
}

void ContextPruner::pinItem(const QString &key, bool pin)
{
    for (auto &item : m_items) {
        if (item.key == key) {
            item.pinned = pin;
            emit itemsChanged();
            return;
        }
    }
}

void ContextPruner::setPriority(const QString &key, int priority)
{
    for (auto &item : m_items) {
        if (item.key == key) {
            item.priority = priority;
            emit itemsChanged();
            return;
        }
    }
}

int ContextPruner::totalTokens() const
{
    int total = 0;
    for (const auto &item : m_items)
        total += item.tokens;
    return total;
}

QList<ContextPruner::ContextItem> ContextPruner::prune() const
{
    if (totalTokens() <= m_maxTokens)
        return m_items;

    // Sort: pinned first, then by priority (ascending = higher), then by token count
    QList<ContextItem> sorted = m_items;
    std::sort(sorted.begin(), sorted.end(), [](const ContextItem &a, const ContextItem &b) {
        if (a.pinned != b.pinned) return a.pinned;
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.tokens < b.tokens; // prefer smaller items when priority ties
    });

    QList<ContextItem> result;
    int running = 0;
    for (const auto &item : std::as_const(sorted)) {
        if (running + item.tokens <= m_maxTokens) {
            result.append(item);
            running += item.tokens;
        }
        // Skip items that don't fit — even pinned items if they'd exceed budget
    }
    return result;
}

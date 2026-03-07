#pragma once

#include <QObject>
#include <QString>
#include <QList>

class ContextPruner : public QObject
{
    Q_OBJECT
public:
    explicit ContextPruner(QObject *parent = nullptr);

    struct ContextItem {
        QString key;       // unique identifier (e.g. "file:path" or "terminal")
        QString content;
        int     tokens = 0;
        int     priority = 5; // 1=highest, 10=lowest
        bool    pinned = false;
    };

    void setMaxTokens(int max) { m_maxTokens = max; }
    int  maxTokens() const { return m_maxTokens; }

    void addItem(const ContextItem &item);
    void removeItem(const QString &key);
    void pinItem(const QString &key, bool pin);
    void setPriority(const QString &key, int priority);

    // Prune to fit within maxTokens, removing lowest-priority unpinned items first
    QList<ContextItem> prune() const;

    // Current items before pruning
    QList<ContextItem> items() const { return m_items; }
    int totalTokens() const;

signals:
    void itemsChanged();

private:
    QList<ContextItem> m_items;
    int m_maxTokens = 120000;
};

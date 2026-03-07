#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QObject>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// ContextEditor — UI for editing context items before sending to model.
//
// Shows a list of context pieces (file content, selection, diagnostics, etc.)
// with checkboxes to include/exclude and token counts. Users can remove
// individual items or pin/unpin them.
// ─────────────────────────────────────────────────────────────────────────────

struct EditableContextItem {
    QString key;         // unique identifier
    QString label;       // display name (e.g. "src/main.cpp")
    QString type;        // "file", "selection", "diagnostics", "terminal", "git"
    QString content;     // actual content
    int tokens = 0;      // estimated token count
    bool included = true;
    bool pinned = false;
};

class ContextEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ContextEditor(QWidget *parent = nullptr);

    /// Set context items
    void setItems(const QList<EditableContextItem> &items);

    /// Get included items only
    QList<EditableContextItem> includedItems() const;

    /// Get all items
    QList<EditableContextItem> allItems() const { return m_items; }

    /// Total token count for included items
    int totalTokens() const;

signals:
    void itemsChanged();
    void itemRemoved(const QString &key);
    void itemPinToggled(const QString &key, bool pinned);

private:
    void buildUI();
    void refresh();

    QVBoxLayout *m_layout = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_tokenLabel = nullptr;
    QListWidget *m_listWidget = nullptr;

    QList<EditableContextItem> m_items;
};

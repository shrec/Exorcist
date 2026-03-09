#pragma once

#include <QFrame>
#include <QJsonArray>

class QListWidget;
class QListWidgetItem;
class EditorView;

// ── CompletionPopup ───────────────────────────────────────────────────────────
//
// Floating completion list shown at the cursor position in an EditorView.
// Keyboard handled via event filter on the editor:
//   Up/Down  — navigate
//   Enter/Tab — accept
//   Escape   — dismiss
//   Any other printable char — forward to editor + re-filter

class CompletionPopup : public QFrame
{
    Q_OBJECT

public:
    explicit CompletionPopup(QWidget *parent = nullptr);

    // Show at cursor position in `editor`, fill with LSP completion items.
    void showForEditor(EditorView *editor, const QJsonArray &items,
                       bool isIncomplete = false);

    // Update the visible list when the user has typed more chars since show().
    void filterByPrefix(const QString &prefix);

    bool isVisible() const;
    void dismiss();

    // True when the last server response was incomplete (more results available).
    bool lastResponseIncomplete() const { return m_isIncomplete; }

signals:
    // Emitted when user accepts an item. insertText is what should be inserted.
    void itemAccepted(const QString &insertText, const QString &filterText);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void repositionForEditor(EditorView *editor);
    void acceptCurrent();

    QListWidget *m_list;
    EditorView  *m_editor = nullptr;

    // Full item set (label, insertText, kind)
    struct Item {
        QString label;
        QString insertText;
        QString filterText;
        int     kind = 0;  // LSP CompletionItemKind
    };
    QVector<Item> m_items;
    bool m_isIncomplete = false;
};

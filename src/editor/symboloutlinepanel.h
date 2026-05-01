#pragma once

#include <QHash>
#include <QJsonArray>
#include <QWidget>

class QAction;
class QLineEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

// ── SymbolOutlinePanel ────────────────────────────────────────────────────────
//
// Left dock panel that shows document symbols (textDocument/documentSymbol).
// Supports both hierarchical DocumentSymbol and flat SymbolInformation.
// Double-click navigates to the symbol in the active editor.
//
// UI features:
//   * Filter QLineEdit (live-filter; matching items + their parents stay visible)
//   * Toolbar buttons: Expand All / Collapse All / Sort toggle (Position ↔ Name)
//   * Kind-aware icons (colored Unicode glyphs, dark-theme friendly)
//   * Right-click: Go to Definition, Find References

class SymbolOutlinePanel : public QWidget
{
    Q_OBJECT

public:
    explicit SymbolOutlinePanel(QWidget *parent = nullptr);

    void setSymbols(const QJsonArray &symbols);
    void clear();

signals:
    void symbolActivated(int line, int character);
    /// Emitted when user picks "Find References" from context menu.
    /// Wire-up is optional — MainWindow may ignore if not connected.
    void findReferencesRequested(int line, int character);

private slots:
    void onExpandAll();
    void onCollapseAll();
    void onToggleSort();
    void onContextMenu(const QPoint &pos);

private:
    QLineEdit    *m_filter   = nullptr;
    QTreeWidget  *m_tree     = nullptr;
    QToolButton  *m_btnExpand   = nullptr;
    QToolButton  *m_btnCollapse = nullptr;
    QToolButton  *m_btnSort     = nullptr;

    // Cached symbol payload so we can rebuild on sort-toggle without losing
    // the original LSP response.
    QJsonArray    m_symbols;
    bool          m_sortByName = false;

    void buildUi();
    void populate(QTreeWidgetItem *parent, const QJsonArray &symbols);
    void applyFilter(const QString &text);

    // Returns true if `item` or any descendant matches `text`.
    static bool filterRecursive(QTreeWidgetItem *item, const QString &text);

    static QString kindLabel(int kind);
    static QString kindGlyph(int kind);
    static QString kindColor(int kind);
};

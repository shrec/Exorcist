#pragma once

#include <QJsonArray>
#include <QWidget>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

// ── SymbolOutlinePanel ────────────────────────────────────────────────────────
//
// Left dock panel that shows document symbols (textDocument/documentSymbol).
// Supports both hierarchical DocumentSymbol and flat SymbolInformation.
// Double-click navigates to the symbol in the active editor.

class SymbolOutlinePanel : public QWidget
{
    Q_OBJECT

public:
    explicit SymbolOutlinePanel(QWidget *parent = nullptr);

    void setSymbols(const QJsonArray &symbols);
    void clear();

signals:
    void symbolActivated(int line, int character);

private:
    QLineEdit    *m_filter;
    QTreeWidget  *m_tree;

    void populate(QTreeWidgetItem *parent, const QJsonArray &symbols);
    void applyFilter(const QString &text);

    static QString kindLabel(int kind);
};

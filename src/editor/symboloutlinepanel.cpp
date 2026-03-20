#include "symboloutlinepanel.h"

#include <QJsonObject>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

SymbolOutlinePanel::SymbolOutlinePanel(QWidget *parent)
    : QWidget(parent),
      m_filter(new QLineEdit(this)),
      m_tree(new QTreeWidget(this))
{
    m_filter->setPlaceholderText(tr("Filter symbols…"));
    m_filter->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #3c3c3c; color: #d4d4d4; border: 1px solid #555558;"
        "  padding: 3px 6px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #007acc; }"
    ));

    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);
    m_tree->setUniformRowHeights(true);
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget { background: #1e1e1e; color: #d4d4d4; border: none; font-size: 12px; }"
        "QTreeWidget::item { padding: 2px 0; border: none; }"
        "QTreeWidget::item:hover { background: #2a2d2e; }"
        "QTreeWidget::item:selected { background: #094771; color: #ffffff; }"
        "QScrollBar:vertical { background: #1e1e1e; width: 10px; border: none; }"
        "QScrollBar::handle:vertical { background: #424242; min-height: 20px; border-radius: 5px; }"
        "QScrollBar::handle:vertical:hover { background: #686868; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(m_filter);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeWidget::itemActivated,
            this, [this](QTreeWidgetItem *item) {
        emit symbolActivated(item->data(0, Qt::UserRole).toInt(),
                             item->data(0, Qt::UserRole + 1).toInt());
    });
    connect(m_filter, &QLineEdit::textChanged,
            this, &SymbolOutlinePanel::applyFilter);
}

void SymbolOutlinePanel::setSymbols(const QJsonArray &symbols)
{
    m_tree->clear();
    if (symbols.isEmpty()) return;
    populate(nullptr, symbols);
    m_tree->expandAll();
    applyFilter(m_filter->text());
}

void SymbolOutlinePanel::clear()
{
    m_tree->clear();
}

void SymbolOutlinePanel::populate(QTreeWidgetItem *parent,
                                  const QJsonArray &symbols)
{
    for (const QJsonValue &v : symbols) {
        const QJsonObject sym  = v.toObject();
        const QString name     = sym["name"].toString();
        const int kind         = sym["kind"].toInt();

        // Resolve position — DocumentSymbol uses selectionRange,
        // SymbolInformation uses location.range
        QJsonObject range;
        if (sym.contains("selectionRange"))
            range = sym["selectionRange"].toObject();
        else if (sym.contains("location"))
            range = sym["location"].toObject()["range"].toObject();
        else
            range = sym["range"].toObject();

        const int line = range["start"].toObject()["line"].toInt();
        const int col  = range["start"].toObject()["character"].toInt();

        const QString label =
            QStringLiteral("[%1] %2").arg(kindLabel(kind), name);

        auto *item = parent
            ? new QTreeWidgetItem(parent,   QStringList{label})
            : new QTreeWidgetItem(m_tree,   QStringList{label});

        item->setData(0, Qt::UserRole,     line);
        item->setData(0, Qt::UserRole + 1, col);

        if (sym.contains("children"))
            populate(item, sym["children"].toArray());
    }
}

void SymbolOutlinePanel::applyFilter(const QString &text)
{
    // Iterate all items (via invisible root children) and show/hide based on text
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        QTreeWidgetItem *item = *it;
        const bool match = text.isEmpty() ||
                           item->text(0).contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
        ++it;
    }
}

QString SymbolOutlinePanel::kindLabel(int kind)
{
    switch (kind) {
    case  2: return "mod";
    case  3: return "ns";
    case  4: return "pkg";
    case  5: return "class";
    case  6: return "method";
    case  7: return "prop";
    case  8: return "field";
    case  9: return "ctor";
    case 10: return "enum";
    case 11: return "iface";
    case 12: return "fn";
    case 13: return "var";
    case 14: return "const";
    case 22: return "member";
    case 23: return "struct";
    case 25: return "op";
    case 26: return "tparam";
    default: return "·";
    }
}

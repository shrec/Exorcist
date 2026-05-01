#include "symboloutlinepanel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {

// LSP SymbolKind → role for sorting (groups parents before locals, etc.)
constexpr int kRoleLine     = Qt::UserRole;       // line  (LSP 0-based)
constexpr int kRoleChar     = Qt::UserRole + 1;   // column
constexpr int kRoleKind     = Qt::UserRole + 2;   // SymbolKind int
constexpr int kRoleSortKey  = Qt::UserRole + 3;   // pre-baked sort string

// VS-Code-ish dark theme tokens (kept inline — UI module is off-limits in this scope).
constexpr auto kBgPanel    = "#1e1e1e";
constexpr auto kFgText     = "#d4d4d4";
constexpr auto kBorder     = "#3c3c3c";
constexpr auto kBorderActive = "#007acc";
constexpr auto kHover      = "#2a2d2e";
constexpr auto kSelected   = "#094771";
constexpr auto kInputBg    = "#3c3c3c";

QString styledScrollbar()
{
    return QStringLiteral(
        "QScrollBar:vertical { background: #1e1e1e; width: 10px; border: none; }"
        "QScrollBar::handle:vertical { background: #424242; min-height: 20px; border-radius: 5px; }"
        "QScrollBar::handle:vertical:hover { background: #686868; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    );
}

} // namespace

// ── ctor / UI ────────────────────────────────────────────────────────────────

SymbolOutlinePanel::SymbolOutlinePanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

void SymbolOutlinePanel::buildUi()
{
    // Toolbar (Expand / Collapse / Sort) ─────────────────────────────────────
    auto *toolbar = new QWidget(this);
    auto *toolLayout = new QHBoxLayout(toolbar);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->setSpacing(2);

    auto makeBtn = [this](const QString &glyph, const QString &tip) {
        auto *b = new QToolButton(this);
        b->setText(glyph);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        b->setFixedSize(22, 22);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QStringLiteral(
            "QToolButton { background: transparent; color: #d4d4d4;"
            "  border: 1px solid transparent; border-radius: 3px;"
            "  padding: 0; font-size: 13px; }"
            "QToolButton:hover { background: #2a2d2e; border-color: #3c3c3c; }"
            "QToolButton:pressed, QToolButton:checked"
            "  { background: #094771; border-color: #007acc; color: #ffffff; }"
        ));
        return b;
    };

    m_btnExpand   = makeBtn(QStringLiteral("⊞"), tr("Expand All"));
    m_btnCollapse = makeBtn(QStringLiteral("⊟"), tr("Collapse All"));
    m_btnSort     = makeBtn(QStringLiteral("↕"), tr("Sort by Name (toggle)"));
    m_btnSort->setCheckable(true);

    toolLayout->addWidget(m_btnExpand);
    toolLayout->addWidget(m_btnCollapse);
    toolLayout->addWidget(m_btnSort);
    toolLayout->addStretch(1);

    // Filter input ───────────────────────────────────────────────────────────
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Filter symbols…"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid #555558;"
        "  border-radius: 2px; padding: 3px 6px; font-size: 12px; }"
        "QLineEdit:focus { border-color: %3; }"
    ).arg(kInputBg, kFgText, kBorderActive));

    // Tree ───────────────────────────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);
    m_tree->setUniformRowHeights(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget { background: %1; color: %2; border: none; font-size: 12px; }"
        "QTreeWidget::item { padding: 2px 0; border: none; }"
        "QTreeWidget::item:hover { background: %3; }"
        "QTreeWidget::item:selected { background: %4; color: #ffffff; }"
        "%5"
    ).arg(kBgPanel, kFgText, kHover, kSelected, styledScrollbar()));

    // Layout ─────────────────────────────────────────────────────────────────
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(toolbar);
    layout->addWidget(m_filter);
    layout->addWidget(m_tree, 1);

    // Wire signals ───────────────────────────────────────────────────────────
    connect(m_tree, &QTreeWidget::itemActivated,
            this, [this](QTreeWidgetItem *item) {
        if (!item) return;
        emit symbolActivated(item->data(0, kRoleLine).toInt(),
                             item->data(0, kRoleChar).toInt());
    });
    connect(m_filter, &QLineEdit::textChanged,
            this, &SymbolOutlinePanel::applyFilter);
    connect(m_btnExpand, &QToolButton::clicked,
            this, &SymbolOutlinePanel::onExpandAll);
    connect(m_btnCollapse, &QToolButton::clicked,
            this, &SymbolOutlinePanel::onCollapseAll);
    connect(m_btnSort, &QToolButton::toggled,
            this, [this](bool on) { m_sortByName = on; onToggleSort(); });
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &SymbolOutlinePanel::onContextMenu);
}

// ── Public API (unchanged) ────────────────────────────────────────────────────

void SymbolOutlinePanel::setSymbols(const QJsonArray &symbols)
{
    m_symbols = symbols;
    m_tree->clear();
    if (symbols.isEmpty()) return;
    populate(nullptr, symbols);
    m_tree->expandAll();
    applyFilter(m_filter->text());
}

void SymbolOutlinePanel::clear()
{
    m_symbols = QJsonArray();
    m_tree->clear();
}

// ── Populate ─────────────────────────────────────────────────────────────────

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

        // Kind glyph + tinted color, then symbol name.
        // We render the kind glyph into the item text (no SVG icons available
        // for symbol kinds in resources.qrc). The glyph is monochrome but the
        // QTreeWidget stylesheet keeps everything readable.
        const QString glyph = kindGlyph(kind);
        const QString label = QStringLiteral("%1  %2").arg(glyph, name);

        auto *item = parent
            ? new QTreeWidgetItem(parent, QStringList{label})
            : new QTreeWidgetItem(m_tree, QStringList{label});

        item->setData(0, kRoleLine, line);
        item->setData(0, kRoleChar, col);
        item->setData(0, kRoleKind, kind);
        // Sort key for Position-mode: pad line so lexicographic ordering works.
        item->setData(0, kRoleSortKey,
                      QStringLiteral("%1:%2")
                          .arg(line, 8, 10, QLatin1Char('0'))
                          .arg(col,  6, 10, QLatin1Char('0')));

        item->setForeground(0, QColor(kindColor(kind)));
        item->setToolTip(0, QStringLiteral("[%1] %2  (line %3)")
                                 .arg(kindLabel(kind), name)
                                 .arg(line + 1));

        if (sym.contains("children"))
            populate(item, sym["children"].toArray());
    }
}

// ── Filtering ────────────────────────────────────────────────────────────────

bool SymbolOutlinePanel::filterRecursive(QTreeWidgetItem *item, const QString &text)
{
    if (!item) return false;

    bool selfMatch = text.isEmpty() ||
                     item->text(0).contains(text, Qt::CaseInsensitive);

    bool childMatch = false;
    for (int i = 0; i < item->childCount(); ++i)
        if (filterRecursive(item->child(i), text))
            childMatch = true;

    const bool visible = selfMatch || childMatch;
    item->setHidden(!visible);
    if (visible && !text.isEmpty() && childMatch)
        item->setExpanded(true);
    return visible;
}

void SymbolOutlinePanel::applyFilter(const QString &text)
{
    // Walk top-level items recursively so that parents of matches stay visible.
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        filterRecursive(m_tree->topLevelItem(i), text);
}

// ── Toolbar slots ────────────────────────────────────────────────────────────

void SymbolOutlinePanel::onExpandAll()
{
    m_tree->expandAll();
}

void SymbolOutlinePanel::onCollapseAll()
{
    m_tree->collapseAll();
}

void SymbolOutlinePanel::onToggleSort()
{
    if (m_sortByName) {
        m_tree->sortItems(0, Qt::AscendingOrder);
        m_btnSort->setToolTip(tr("Sort: by Name (click to switch to Position)"));
    } else {
        // Restore source-order: rebuild from cached payload.
        m_tree->clear();
        if (!m_symbols.isEmpty()) {
            populate(nullptr, m_symbols);
            m_tree->expandAll();
        }
        m_btnSort->setToolTip(tr("Sort: by Position (click to switch to Name)"));
    }
    applyFilter(m_filter->text());
}

// ── Context menu ─────────────────────────────────────────────────────────────

void SymbolOutlinePanel::onContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item) return;

    const int line = item->data(0, kRoleLine).toInt();
    const int col  = item->data(0, kRoleChar).toInt();

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #252526; color: %1; border: 1px solid %2; }"
        "QMenu::item { padding: 4px 18px; }"
        "QMenu::item:selected { background: %3; color: #ffffff; }"
    ).arg(kFgText, kBorder, kSelected));

    auto *aGoto = menu.addAction(tr("Go to Definition"));
    auto *aRefs = menu.addAction(tr("Find References"));
    menu.addSeparator();
    auto *aExpand   = menu.addAction(tr("Expand All"));
    auto *aCollapse = menu.addAction(tr("Collapse All"));

    QAction *picked = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!picked) return;

    if (picked == aGoto)
        emit symbolActivated(line, col);
    else if (picked == aRefs)
        emit findReferencesRequested(line, col);
    else if (picked == aExpand)
        onExpandAll();
    else if (picked == aCollapse)
        onCollapseAll();
}

// ── Kind metadata ────────────────────────────────────────────────────────────
//
// LSP SymbolKind enum (1-based):
//   1 File, 2 Module, 3 Namespace, 4 Package, 5 Class, 6 Method, 7 Property,
//   8 Field, 9 Constructor, 10 Enum, 11 Interface, 12 Function, 13 Variable,
//   14 Constant, 15 String, 16 Number, 17 Boolean, 18 Array, 19 Object,
//   20 Key, 21 Null, 22 EnumMember, 23 Struct, 24 Event, 25 Operator,
//   26 TypeParameter

QString SymbolOutlinePanel::kindLabel(int kind)
{
    switch (kind) {
    case  1: return QStringLiteral("file");
    case  2: return QStringLiteral("mod");
    case  3: return QStringLiteral("ns");
    case  4: return QStringLiteral("pkg");
    case  5: return QStringLiteral("class");
    case  6: return QStringLiteral("method");
    case  7: return QStringLiteral("prop");
    case  8: return QStringLiteral("field");
    case  9: return QStringLiteral("ctor");
    case 10: return QStringLiteral("enum");
    case 11: return QStringLiteral("iface");
    case 12: return QStringLiteral("fn");
    case 13: return QStringLiteral("var");
    case 14: return QStringLiteral("const");
    case 15: return QStringLiteral("str");
    case 16: return QStringLiteral("num");
    case 17: return QStringLiteral("bool");
    case 18: return QStringLiteral("array");
    case 19: return QStringLiteral("obj");
    case 20: return QStringLiteral("key");
    case 21: return QStringLiteral("null");
    case 22: return QStringLiteral("member");
    case 23: return QStringLiteral("struct");
    case 24: return QStringLiteral("event");
    case 25: return QStringLiteral("op");
    case 26: return QStringLiteral("tparam");
    default: return QStringLiteral("·");
    }
}

QString SymbolOutlinePanel::kindGlyph(int kind)
{
    // Compact monospace-ish glyphs that read well at 12px in dark theme.
    switch (kind) {
    case  1: return QStringLiteral("📄");   // File
    case  2: return QStringLiteral("◫");   // Module
    case  3: return QStringLiteral("⌬");   // Namespace
    case  4: return QStringLiteral("⌬");   // Package
    case  5: return QStringLiteral("◯");   // Class
    case  6: return QStringLiteral("ƒ");    // Method
    case  7: return QStringLiteral("◆");   // Property
    case  8: return QStringLiteral("▢");   // Field
    case  9: return QStringLiteral("✱");    // Constructor
    case 10: return QStringLiteral("∑");    // Enum
    case 11: return QStringLiteral("◉");   // Interface
    case 12: return QStringLiteral("ƒ");    // Function
    case 13: return QStringLiteral("▢");   // Variable
    case 14: return QStringLiteral("π");    // Constant
    case 15: return QStringLiteral("\"");   // String
    case 16: return QStringLiteral("#");    // Number
    case 17: return QStringLiteral("◐");   // Boolean
    case 18: return QStringLiteral("[]");   // Array
    case 19: return QStringLiteral("{}");   // Object
    case 20: return QStringLiteral("⚿");    // Key
    case 21: return QStringLiteral("∅");    // Null
    case 22: return QStringLiteral("•");    // EnumMember
    case 23: return QStringLiteral("◫");   // Struct
    case 24: return QStringLiteral("⚡");    // Event
    case 25: return QStringLiteral("±");    // Operator
    case 26: return QStringLiteral("τ");    // TypeParameter
    default: return QStringLiteral("·");
    }
}

QString SymbolOutlinePanel::kindColor(int kind)
{
    // VS Code dark theme symbol colors (codicon palette).
    switch (kind) {
    case  5: // Class
    case 11: // Interface
    case 23: // Struct
        return QStringLiteral("#4ec9b0");   // teal — types
    case  6: // Method
    case  9: // Constructor
    case 12: // Function
        return QStringLiteral("#dcdcaa");   // yellow — callables
    case  7: // Property
    case  8: // Field
    case 22: // EnumMember
        return QStringLiteral("#9cdcfe");   // light blue — members
    case 13: // Variable
        return QStringLiteral("#9cdcfe");   // light blue
    case 14: // Constant
    case 15: // String
    case 16: // Number
    case 17: // Boolean
        return QStringLiteral("#b5cea8");   // green — literals
    case  2: // Module
    case  3: // Namespace
    case  4: // Package
        return QStringLiteral("#c586c0");   // purple — containers
    case 10: // Enum
        return QStringLiteral("#4ec9b0");   // teal
    case 24: // Event
        return QStringLiteral("#dcdcaa");   // yellow
    case 25: // Operator
    case 26: // TypeParameter
        return QStringLiteral("#d7ba7d");   // tan
    case  1: // File
        return QStringLiteral("#cccccc");
    default:
        return QStringLiteral("#d4d4d4");
    }
}

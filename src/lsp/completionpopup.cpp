#include "completionpopup.h"

#include <QEvent>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QListWidget>
#include <QRect>
#include <QScreen>
#include <QVBoxLayout>

#include "../editor/editorview.h"

// ── Kind icons (text fallback) ────────────────────────────────────────────────
static QString kindLabel(int kind)
{
    switch (kind) {
    case  2: return "\u2B21";  // Method
    case  3: return "\u2B21";  // Function
    case  4: return "\u2B21";  // Constructor
    case  5: return "\u25A1";  // Field
    case  6: return "\u25C7";  // Variable
    case  7: return "\u25C8";  // Class
    case  8: return "\u25C7";  // Interface
    case  9: return "\u25C9";  // Module
    case 14: return "\u2736";  // Keyword
    default: return "\u00B7";
    }
}

// ── CompletionPopup ───────────────────────────────────────────────────────────

CompletionPopup::CompletionPopup(QWidget *parent)
    : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint),
      m_list(new QListWidget(this))
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window,     QColor(0x25, 0x25, 0x26));
    pal.setColor(QPalette::WindowText, QColor(0xCC, 0xCC, 0xCC));
    setPalette(pal);

    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setStyleSheet(
        "QListWidget { background:#252526; color:#cccccc; border:none; }"
        "QListWidget::item:selected { background:#094771; color:#ffffff; }"
        "QListWidget::item:hover    { background:#2a2d2e; }");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    layout->addWidget(m_list);

    setMinimumWidth(300);
    setMaximumHeight(240);
    hide();
}

// ── Public API ────────────────────────────────────────────────────────────────

void CompletionPopup::showForEditor(EditorView *editor, const QJsonArray &items)
{
    if (m_editor && m_editor != editor)
        m_editor->removeEventFilter(this);

    m_editor = editor;
    m_editor->installEventFilter(this);

    m_items.clear();
    for (const QJsonValue &v : items) {
        const QJsonObject obj = v.toObject();
        Item item;
        item.label      = obj["label"].toString();
        item.insertText = obj.contains("insertText")
                              ? obj["insertText"].toString()
                              : item.label;
        item.filterText = obj.contains("filterText")
                              ? obj["filterText"].toString()
                              : item.label;
        item.kind       = obj["kind"].toInt(1);
        m_items.append(item);
    }

    filterByPrefix({});
    if (m_list->count() == 0) { dismiss(); return; }

    // Position below the cursor
    const QRect cursorRect = editor->cursorRect();
    const QPoint globalPos = editor->viewport()->mapToGlobal(
        cursorRect.bottomLeft());
    const QScreen *scr = QGuiApplication::primaryScreen();
    const QRect avail  = scr ? scr->availableGeometry() : QRect(0,0,1920,1080);
    const int x = qMin(globalPos.x(), avail.right() - minimumWidth());
    const int popupH = qMin(240, m_list->count() * m_list->sizeHintForRow(0) + 4);
    int y = globalPos.y() + 2;
    if (y + popupH > avail.bottom())
        y = globalPos.y() - cursorRect.height() - popupH - 2;
    move(x, qMax(avail.top(), y));

    show();
    m_list->setCurrentRow(0);
}

void CompletionPopup::filterByPrefix(const QString &prefix)
{
    m_list->clear();
    const QString lp = prefix.toLower();
    for (const Item &item : std::as_const(m_items)) {
        if (lp.isEmpty() || item.filterText.toLower().startsWith(lp)) {
            auto *wi = new QListWidgetItem(
                kindLabel(item.kind) + "  " + item.label, m_list);
            wi->setData(Qt::UserRole,     item.insertText);
            wi->setData(Qt::UserRole + 1, item.filterText);
        }
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

bool CompletionPopup::isVisible() const
{
    return QFrame::isVisible();
}

void CompletionPopup::dismiss()
{
    if (m_editor) {
        m_editor->removeEventFilter(this);
        m_editor = nullptr;
    }
    hide();
}

// ── Event filter ──────────────────────────────────────────────────────────────

bool CompletionPopup::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_editor || event->type() != QEvent::KeyPress)
        return false;

    auto *ke = static_cast<QKeyEvent *>(event);

    if (ke->key() == Qt::Key_Escape) {
        dismiss();
        return false;
    }
    if (ke->key() == Qt::Key_Up) {
        const int row = m_list->currentRow();
        if (row > 0) m_list->setCurrentRow(row - 1);
        return true;
    }
    if (ke->key() == Qt::Key_Down) {
        const int row = m_list->currentRow();
        if (row < m_list->count() - 1) m_list->setCurrentRow(row + 1);
        return true;
    }
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Tab) {
        acceptCurrent();
        return true;
    }
    // Any other key: let editor handle it, bridge will re-filter afterward
    return false;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void CompletionPopup::acceptCurrent()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item) { dismiss(); return; }

    const QString insertText = item->data(Qt::UserRole).toString();
    const QString filterText = item->data(Qt::UserRole + 1).toString();
    dismiss();
    emit itemAccepted(insertText, filterText);
}

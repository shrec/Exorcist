#include "widgetpalette.h"

#include <QApplication>
#include <QDrag>
#include <QFont>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>

namespace exo::forms {
namespace {

// ── Default widget set ───────────────────────────────────────────────────────
//
// Order is meaningful: the categories render in the sequence we list them, and
// within a category items are sorted by label after we fill the list.  Keep
// this list short — Phase 1 deliberately ships ~25 widgets, not 200.
QList<PaletteEntry> defaultEntries() {
    return {
        // Buttons
        {"QPushButton",  "Push Button",      "Buttons"},
        {"QToolButton",  "Tool Button",      "Buttons"},
        {"QRadioButton", "Radio Button",     "Buttons"},
        {"QCheckBox",    "Check Box",        "Buttons"},
        // Inputs
        {"QLineEdit",      "Line Edit",      "Inputs"},
        {"QPlainTextEdit", "Plain Text Edit","Inputs"},
        {"QTextEdit",      "Text Edit",      "Inputs"},
        {"QComboBox",      "Combo Box",      "Inputs"},
        {"QSpinBox",       "Spin Box",       "Inputs"},
        {"QDoubleSpinBox", "Double Spin Box","Inputs"},
        {"QSlider",        "Slider",         "Inputs"},
        {"QDial",          "Dial",           "Inputs"},
        // Display
        {"QLabel",        "Label",           "Display"},
        {"QProgressBar",  "Progress Bar",    "Display"},
        // Item Views
        {"QListView",   "List View",         "Item Views"},
        {"QTreeView",   "Tree View",         "Item Views"},
        {"QTableView",  "Table View",        "Item Views"},
        // Containers
        {"QGroupBox",     "Group Box",       "Containers"},
        {"QFrame",        "Frame",           "Containers"},
        {"QTabWidget",    "Tab Widget",      "Containers"},
        {"QStackedWidget","Stacked Widget",  "Containers"},
        {"QScrollArea",   "Scroll Area",     "Containers"},
        {"QSplitter",     "Splitter",        "Containers"},
        {"QMainWindow",   "Main Window",     "Containers"},
        {"QDialog",       "Dialog",          "Containers"},
    };
}

// Subclass of QListWidget that initiates a QDrag with our custom MIME type.
// We need the className survive the drag → so we stamp it on the QMimeData.
class PaletteList : public QListWidget {
public:
    explicit PaletteList(QWidget *parent = nullptr) : QListWidget(parent) {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
    }

protected:
    void startDrag(Qt::DropActions /*supported*/) override {
        QListWidgetItem *it = currentItem();
        if (!it) return;
        const QString cls = it->data(Qt::UserRole).toString();
        if (cls.isEmpty()) return;  // category header

        auto *mime = new QMimeData();
        mime->setData(kFormWidgetMime, cls.toUtf8());
        mime->setText(cls);   // for tooltip / debug logs

        // Tiny preview pixmap so the cursor shows what's being dragged.
        QPixmap pm(140, 22);
        pm.fill(QColor("#2d2d30"));
        QPainter p(&pm);
        p.setPen(QColor("#d4d4d4"));
        p.drawText(pm.rect().adjusted(6, 0, -6, 0),
                   Qt::AlignVCenter | Qt::AlignLeft, cls);
        p.setPen(QColor("#3c3c3c"));
        p.drawRect(pm.rect().adjusted(0, 0, -1, -1));
        p.end();

        auto *drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->setPixmap(pm);
        drag->setHotSpot(QPoint(8, 11));
        drag->exec(Qt::CopyAction);
    }
};

} // namespace

WidgetPalette::WidgetPalette(QWidget *parent) : QWidget(parent) {
    buildUi();
    rebuild(defaultEntries());
}

void WidgetPalette::buildUi() {
    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(6, 6, 6, 6);
    col->setSpacing(4);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search widgets..."));
    m_search->setClearButtonEnabled(true);
    m_search->setStyleSheet(
        "QLineEdit{background:#252526;color:#d4d4d4;border:1px solid #3c3c3c;"
        "padding:3px 6px;border-radius:2px;}"
        "QLineEdit:focus{border:1px solid #007acc;}");
    col->addWidget(m_search);

    m_list = new PaletteList(this);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setStyleSheet(
        "QListWidget{background:#1e1e1e;color:#d4d4d4;border:none;outline:none;}"
        "QListWidget::item{padding:3px 6px;}"
        "QListWidget::item:selected{background:#094771;color:white;}"
        "QListWidget::item:hover{background:#2a2d2e;}");
    col->addWidget(m_list, 1);

    connect(m_search, &QLineEdit::textChanged,
            this,     &WidgetPalette::applyFilter);

    connect(m_list, &QListWidget::itemDoubleClicked,
            this,   [this](QListWidgetItem *it){
        const QString cls = it ? it->data(Qt::UserRole).toString() : QString();
        if (!cls.isEmpty()) emit widgetActivated(cls);
    });

    setStyleSheet("QWidget{background:#1e1e1e;}");
}

void WidgetPalette::rebuild(const QList<PaletteEntry> &entries) {
    m_entries = entries;
    applyFilter(m_search ? m_search->text() : QString());
}

void WidgetPalette::applyFilter(const QString &needle) {
    m_list->clear();
    const QString n = needle.trimmed().toLower();

    // Group entries by category preserving the input order's category sequence.
    QStringList catOrder;
    QMap<QString, QList<PaletteEntry>> byCat;
    for (const auto &e : m_entries) {
        if (!byCat.contains(e.category)) catOrder << e.category;
        byCat[e.category].append(e);
    }

    for (const QString &cat : catOrder) {
        QList<PaletteEntry> bucket = byCat.value(cat);
        QList<PaletteEntry> filtered;
        for (const auto &e : bucket) {
            if (n.isEmpty()
                || e.label.toLower().contains(n)
                || e.className.toLower().contains(n)) {
                filtered.append(e);
            }
        }
        if (filtered.isEmpty()) continue;

        // Header (non-selectable, italic, dim).
        auto *hdr = new QListWidgetItem(cat.toUpper());
        QFont hf = hdr->font();
        hf.setBold(true);
        hf.setPointSizeF(hf.pointSizeF() * 0.85);
        hdr->setFont(hf);
        hdr->setForeground(QColor("#858585"));
        hdr->setFlags(Qt::ItemIsEnabled);  // not selectable, not draggable
        hdr->setData(Qt::UserRole, QString());
        m_list->addItem(hdr);

        for (const auto &e : filtered) {
            auto *it = new QListWidgetItem("  " + e.label);
            it->setData(Qt::UserRole, e.className);
            it->setToolTip(e.className);
            m_list->addItem(it);
        }
    }
}

} // namespace exo::forms

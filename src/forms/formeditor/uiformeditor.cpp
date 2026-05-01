#include "uiformeditor.h"

#include "formcanvas.h"
#include "propertyinspector.h"
#include "uixmlio.h"
#include "widgetpalette.h"

#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWidget>

namespace exo::forms {

UiFormEditor::UiFormEditor(QWidget *parent) : QWidget(parent) {
    m_undo = new QUndoStack(this);
    buildUi();

    connect(m_palette, &WidgetPalette::widgetActivated, this,
            [this](const QString &cls){
                if (!m_canvas) return;
                const QPoint c = m_canvas->rect().center();
                m_canvas->insertWidget(cls, c);
            });
    connect(m_canvas, &FormCanvas::modified,
            this,     &UiFormEditor::onCanvasModified);
    connect(m_canvas, &FormCanvas::selectionChanged,
            this,     &UiFormEditor::onSelectionChanged);
    connect(m_inspector, &PropertyInspector::propertyChanged, this,
            [this](QWidget *, const QString &){ setModified(true); });

    onSelectionChanged();    // initial breadcrumb / inspector state
}

void UiFormEditor::buildUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ─────────────────────────────────────────────────────────────
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setStyleSheet(
        "QToolBar{background:#2d2d30;border-bottom:1px solid #3c3c3c;spacing:2px;padding:2px;}"
        "QToolButton{color:#d4d4d4;background:transparent;border:1px solid transparent;"
        " padding:3px 6px;border-radius:2px;}"
        "QToolButton:hover{background:#3e3e42;border:1px solid #3c3c3c;}"
        "QToolButton:checked{background:#094771;border:1px solid #007acc;color:white;}"
        "QToolBar::separator{background:#3c3c3c;width:1px;margin:4px 4px;}");
    buildToolbar(m_toolbar);
    root->addWidget(m_toolbar);

    // ── Three-pane splitter ────────────────────────────────────────────────
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);
    m_splitter->setStyleSheet(
        "QSplitter::handle{background:#3c3c3c;}"
        "QSplitter::handle:hover{background:#007acc;}");

    m_palette   = new WidgetPalette(m_splitter);
    m_canvas    = new FormCanvas(m_splitter);
    m_inspector = new PropertyInspector(m_splitter);

    m_canvas->setUndoStack(m_undo);

    m_splitter->addWidget(m_palette);
    m_splitter->addWidget(m_canvas);
    m_splitter->addWidget(m_inspector);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setSizes({200, 800, 280});
    root->addWidget(m_splitter, 1);

    // ── Bottom status bar ──────────────────────────────────────────────────
    auto *statusRow = new QWidget(this);
    statusRow->setStyleSheet(
        "QWidget{background:#007acc;color:white;}"
        "QLabel{color:white;padding:2px 8px;}");
    auto *statusLayout = new QHBoxLayout(statusRow);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(0);
    m_breadcrumb = new QLabel(tr("(no selection)"), statusRow);
    m_status     = new QLabel(QString(), statusRow);
    statusLayout->addWidget(m_breadcrumb, 1);
    statusLayout->addWidget(m_status,     0);
    root->addWidget(statusRow);
}

void UiFormEditor::buildToolbar(QToolBar *bar) {
    // Edit / Preview toggle.  Spacebar (window-scoped) flips it.
    auto *previewAct = bar->addAction(tr("Preview"));
    previewAct->setCheckable(true);
    previewAct->setShortcut(QKeySequence(Qt::Key_F5));
    previewAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    previewAct->setToolTip(tr("Toggle live preview (F5)"));
    addAction(previewAct);
    connect(previewAct, &QAction::toggled, this, &UiFormEditor::onTogglePreview);

    bar->addSeparator();

    // Undo / Redo
    auto *undoAct = m_undo->createUndoAction(this, tr("Undo"));
    undoAct->setShortcut(QKeySequence::Undo);
    undoAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(undoAct);
    bar->addAction(undoAct);
    auto *redoAct = m_undo->createRedoAction(this, tr("Redo"));
    redoAct->setShortcut(QKeySequence::Redo);
    redoAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(redoAct);
    bar->addAction(redoAct);

    bar->addSeparator();

    // Align row.  Each button maps to an alignment-style int the canvas
    // uses to position the selection relative to the primary widget.
    struct AlignDef { const char *label; int kind; };
    const AlignDef aligns[] = {
        {"⇤", 0}, {"↔", 1}, {"⇥", 2},   // left / hcenter / right
        {"⇡", 3}, {"⇕", 4}, {"⇣", 5},   // top  / vcenter / bottom
    };
    for (const auto &a : aligns) {
        auto *act = bar->addAction(QString::fromUtf8(a.label));
        act->setToolTip(tr("Align selection"));
        connect(act, &QAction::triggered, this, [this, k = a.kind](){ onAlign(k); });
    }

    bar->addSeparator();

    auto *distH = bar->addAction(tr("Dist H"));
    distH->setToolTip(tr("Distribute horizontally"));
    connect(distH, &QAction::triggered, this, &UiFormEditor::onDistributeH);
    auto *distV = bar->addAction(tr("Dist V"));
    distV->setToolTip(tr("Distribute vertically"));
    connect(distV, &QAction::triggered, this, &UiFormEditor::onDistributeV);

    bar->addSeparator();

    // Save (Ctrl+S, widget-scoped so we don't hijack other editors)
    auto *saveAct = bar->addAction(tr("Save"));
    saveAct->setShortcut(QKeySequence::Save);
    saveAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(saveAct);
    connect(saveAct, &QAction::triggered, this, &UiFormEditor::onSave);
}

// ── File I/O ─────────────────────────────────────────────────────────────────

bool UiFormEditor::loadFromFile(const QString &path) {
    QWidget *tree = UiXmlIO::load(path);
    if (!tree) {
        if (m_status) m_status->setText(tr("Failed to parse: %1")
                                            .arg(QFileInfo(path).fileName()));
        return false;
    }
    m_path = path;
    m_canvas->setRoot(tree);
    setModified(false);
    if (m_status) m_status->setText(tr("Loaded %1").arg(QFileInfo(path).fileName()));
    updateBreadcrumb();
    return true;
}

bool UiFormEditor::saveToFile(const QString &path) {
    const QString target = path.isEmpty() ? m_path : path;
    if (target.isEmpty()) return false;
    if (!UiXmlIO::save(target, m_canvas->formRoot())) {
        if (m_status) m_status->setText(tr("Save failed: %1")
                                            .arg(QFileInfo(target).fileName()));
        return false;
    }
    m_path = target;
    setModified(false);
    if (m_status) m_status->setText(tr("Saved %1").arg(QFileInfo(target).fileName()));
    return true;
}

void UiFormEditor::onSave() { saveToFile(); }

void UiFormEditor::onCanvasModified() { setModified(true); }

void UiFormEditor::onSelectionChanged() {
    QWidget *primary = m_canvas->primary();
    m_inspector->setTarget(primary);
    updateBreadcrumb();
}

void UiFormEditor::onTogglePreview(bool on) {
    m_canvas->setPreviewMode(on);
    // Disable palette + inspector input while previewing — visually obvious
    // "you can't edit right now" cue without hiding them entirely.
    m_palette->setEnabled(!on);
    m_inspector->setEnabled(!on);
    if (m_status) m_status->setText(on ? tr("Preview mode (F5 to exit)") : QString());
}

// ── Align / distribute (operate on canvas selection) ────────────────────────
//
// Designer's align is a long static utility; we inline a simpler version that
// uses the primary widget as the anchor.  All other selected widgets snap
// their corresponding edge to the primary's edge.
void UiFormEditor::onAlign(int kind) {
    QWidget *primary = m_canvas->primary();
    if (!primary) return;
    const QList<QWidget *> sel = m_canvas->selection();
    if (sel.size() < 2) return;
    const QRect anchor = primary->geometry();
    for (QWidget *w : sel) {
        if (w == primary) continue;
        QRect r = w->geometry();
        switch (kind) {
        case 0: r.moveLeft(anchor.left());                              break;
        case 1: r.moveLeft(anchor.center().x() - r.width()/2);          break;
        case 2: r.moveRight(anchor.right());                            break;
        case 3: r.moveTop(anchor.top());                                break;
        case 4: r.moveTop(anchor.center().y() - r.height()/2);          break;
        case 5: r.moveBottom(anchor.bottom());                          break;
        default: break;
        }
        w->setGeometry(r);
    }
    setModified(true);
    m_canvas->update();
}

void UiFormEditor::onDistributeH() {
    QList<QWidget *> sel = m_canvas->selection();
    if (sel.size() < 3) return;
    std::sort(sel.begin(), sel.end(), [](QWidget *a, QWidget *b){
        return a->x() < b->x();
    });
    const int leftMost  = sel.first()->x();
    const int rightMost = sel.last()->x() + sel.last()->width();
    int totalW = 0;
    for (QWidget *w : sel) totalW += w->width();
    const int gap = (rightMost - leftMost - totalW) / (sel.size() - 1);
    int x = leftMost;
    for (QWidget *w : sel) {
        w->move(x, w->y());
        x += w->width() + gap;
    }
    setModified(true);
    m_canvas->update();
}

void UiFormEditor::onDistributeV() {
    QList<QWidget *> sel = m_canvas->selection();
    if (sel.size() < 3) return;
    std::sort(sel.begin(), sel.end(), [](QWidget *a, QWidget *b){
        return a->y() < b->y();
    });
    const int topMost    = sel.first()->y();
    const int bottomMost = sel.last()->y() + sel.last()->height();
    int totalH = 0;
    for (QWidget *w : sel) totalH += w->height();
    const int gap = (bottomMost - topMost - totalH) / (sel.size() - 1);
    int y = topMost;
    for (QWidget *w : sel) {
        w->move(w->x(), y);
        y += w->height() + gap;
    }
    setModified(true);
    m_canvas->update();
}

void UiFormEditor::setModified(bool m) {
    if (m == m_modified) return;
    m_modified = m;
    emit modificationChanged(m);
}

void UiFormEditor::updateBreadcrumb() {
    if (!m_breadcrumb) return;
    QWidget *primary = m_canvas->primary();
    if (!primary) {
        m_breadcrumb->setText(tr("Form: %1")
            .arg(m_canvas->formRoot() ? m_canvas->formRoot()->objectName()
                                      : QStringLiteral("Form")));
        return;
    }
    // Walk parent chain inside the form to build a "Form › groupBox › button"
    // style breadcrumb.  Stops at the canvas (which is not part of the form).
    QStringList parts;
    QWidget *w = primary;
    while (w && w != m_canvas) {
        const QString name = w->objectName().isEmpty()
            ? QString::fromLatin1(w->metaObject()->className())
            : w->objectName();
        parts.prepend(name);
        w = w->parentWidget();
    }
    m_breadcrumb->setText(parts.join(QStringLiteral(" › ")));
}

} // namespace exo::forms

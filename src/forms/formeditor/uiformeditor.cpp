#include "uiformeditor.h"

#include "formcanvas.h"
#include "promotedialog.h"
#include "propertyinspector.h"
#include "signalsloteditor.h"
#include "uixmlio.h"
#include "widgetpalette.h"
#include <QPointer>
#include <QTimer>

#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
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
    connect(m_canvas, &FormCanvas::contextMenuOnWidget,
            this,     &UiFormEditor::onCanvasContextMenu);
    connect(m_inspector, &PropertyInspector::propertyChanged, this,
            [this](QWidget *, const QString &){ setModified(true); });

    // Phase 2: pass undo stack to inspector for fine-grained property undo.
    m_inspector->setUndoStack(m_undo);

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

    // ── Phase 2: layout commands ────────────────────────────────────────────
    // Tooltips include the keyboard shortcut (Principle 1: Keyboard-first).
    auto addLayoutAction = [&](const QString &text, const QString &tip,
                               const QKeySequence &keys, auto slot) {
        auto *act = bar->addAction(text);
        act->setToolTip(tip + QStringLiteral(" (")
                            + keys.toString(QKeySequence::NativeText)
                            + QStringLiteral(")"));
        act->setShortcut(keys);
        act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        addAction(act);
        connect(act, &QAction::triggered, this, slot);
    };
    addLayoutAction(QStringLiteral("⮃H"),  tr("Lay Out Horizontally"),
                    QKeySequence(Qt::CTRL | Qt::Key_1),
                    &UiFormEditor::onLayoutHorizontal);
    addLayoutAction(QStringLiteral("⮃V"),  tr("Lay Out Vertically"),
                    QKeySequence(Qt::CTRL | Qt::Key_2),
                    &UiFormEditor::onLayoutVertical);
    addLayoutAction(QStringLiteral("▦"),   tr("Lay Out in Grid"),
                    QKeySequence(Qt::CTRL | Qt::Key_3),
                    &UiFormEditor::onLayoutGrid);
    addLayoutAction(QStringLiteral("⊟"),   tr("Lay Out in Form"),
                    QKeySequence(Qt::CTRL | Qt::Key_4),
                    &UiFormEditor::onLayoutForm);
    addLayoutAction(QStringLiteral("⊘"),   tr("Break Layout"),
                    QKeySequence(Qt::CTRL | Qt::Key_0),
                    &UiFormEditor::onBreakLayout);

    bar->addSeparator();

    // Phase 2: signal/slot editor (Ctrl+Alt+S, modeless dialog)
    auto *sigSlot = bar->addAction(tr("⚡ Signals"));
    sigSlot->setToolTip(tr("Edit signal-slot connections (Ctrl+Alt+S)"));
    sigSlot->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    sigSlot->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(sigSlot);
    connect(sigSlot, &QAction::triggered, this,
            &UiFormEditor::onOpenSignalSlotEditor);

    bar->addSeparator();

    // Save (Ctrl+S, widget-scoped so we don't hijack other editors)
    auto *saveAct = bar->addAction(tr("Save"));
    saveAct->setToolTip(tr("Save form (Ctrl+S)"));
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
    // Phase 2: rehydrate signal-slot connections from the .ui's <connections>.
    m_canvas->setConnections(UiXmlIO::loadConnections(path));
    if (m_signalSlot) m_signalSlot->rebuildFromCanvas();

    // Phase 3: rehydrate <customwidgets> by joining the promoted-class
    // table (className → entry) with the per-widget class declarations
    // (objectName → declared class).  Whenever a live widget's declared
    // class is in the promoted set, register a PromotionInfo for it.
    const QHash<QString, UiCustomWidget> byClass = UiXmlIO::loadCustomWidgets(path);
    if (!byClass.isEmpty()) {
        const QHash<QString, QString> classByName = UiXmlIO::loadWidgetClassByName(path);
        QHash<QString, FormCanvas::PromotionInfo> byName;
        for (auto it = classByName.begin(); it != classByName.end(); ++it) {
            auto cIt = byClass.find(it.value());
            if (cIt == byClass.end()) continue;
            FormCanvas::PromotionInfo info;
            info.className     = cIt.value().promotedName;
            info.headerFile    = cIt.value().headerFile;
            info.globalInclude = cIt.value().globalInclude;
            byName.insert(it.key(), info);
        }
        m_canvas->setPromotionsByName(byName);
    }

    setModified(false);
    if (m_status) m_status->setText(tr("Loaded %1").arg(QFileInfo(path).fileName()));
    updateBreadcrumb();
    return true;
}

bool UiFormEditor::saveToFile(const QString &path) {
    const QString target = path.isEmpty() ? m_path : path;
    if (target.isEmpty()) return false;

    // Phase 3: build the (objectName → UiCustomWidget) promotion map the
    // writer needs.  We derive baseClass from each promoted widget's live
    // metaObject — that's the runtime base type, which is what Designer's
    // <extends> tag should record.
    UiPromotionMap promoMap;
    const auto live = m_canvas->promotions();
    for (auto it = live.begin(); it != live.end(); ++it) {
        QWidget *w = it.key();
        if (!w || w->objectName().isEmpty()) continue;
        UiCustomWidget cw;
        cw.promotedName   = it.value().className;
        cw.baseClass      = QString::fromLatin1(w->metaObject()->className());
        cw.headerFile     = it.value().headerFile;
        cw.globalInclude  = it.value().globalInclude;
        promoMap.insert(w->objectName(), cw);
    }

    if (!UiXmlIO::save(target, m_canvas->formRoot(),
                       m_canvas->connections(), promoMap)) {
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

// ── Phase 2: layout command slots ────────────────────────────────────────────

void UiFormEditor::onLayoutHorizontal() {
    m_canvas->applyLayoutToSelection(FormCanvas::LayoutHorizontal);
    if (m_status) m_status->setText(tr("Laid out horizontally"));
}
void UiFormEditor::onLayoutVertical() {
    m_canvas->applyLayoutToSelection(FormCanvas::LayoutVertical);
    if (m_status) m_status->setText(tr("Laid out vertically"));
}
void UiFormEditor::onLayoutGrid() {
    m_canvas->applyLayoutToSelection(FormCanvas::LayoutGrid);
    if (m_status) m_status->setText(tr("Laid out in grid"));
}
void UiFormEditor::onLayoutForm() {
    m_canvas->applyLayoutToSelection(FormCanvas::LayoutForm);
    if (m_status) m_status->setText(tr("Laid out in form"));
}
void UiFormEditor::onBreakLayout() {
    m_canvas->applyLayoutToSelection(FormCanvas::LayoutBreak);
    if (m_status) m_status->setText(tr("Layout broken"));
}

// ── Phase 2: signal-slot editor ──────────────────────────────────────────────

void UiFormEditor::onOpenSignalSlotEditor() {
    if (!m_signalSlot) {
        m_signalSlot = new SignalSlotEditor(m_canvas, this);
    } else {
        // Refresh in case the form was reloaded or new widgets were added.
        m_signalSlot->rebuildFromCanvas();
    }
    m_signalSlot->show();
    m_signalSlot->raise();
    m_signalSlot->activateWindow();
}

// Right-click on canvas → small context menu with shortcuts shown alongside.
// One entry: "Add Signal-Slot Connection..." which opens the editor with the
// clicked widget pre-selected as the sender.
void UiFormEditor::onCanvasContextMenu(QWidget *target, const QPoint &globalPos) {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu{background:#252526;color:#d4d4d4;border:1px solid #3c3c3c;}"
        "QMenu::item{padding:4px 24px 4px 12px;}"
        "QMenu::item:selected{background:#094771;}"
        "QMenu::separator{background:#3c3c3c;height:1px;margin:4px 8px;}");

    auto *editTextAct = menu.addAction(tr("Edit Text (F2)"));
    editTextAct->setEnabled(target != nullptr);
    QObject::connect(editTextAct, &QAction::triggered, this, [this, target](){
        if (target) m_canvas->beginInlineEdit(target);
    });
    menu.addSeparator();
    auto *sigSlotAct = menu.addAction(tr("Add Signal-Slot Connection..."));
    QObject::connect(sigSlotAct, &QAction::triggered, this, [this, target](){
        onOpenSignalSlotEditor();
        if (target && m_signalSlot) m_signalSlot->presetSender(target);
    });
    menu.addSeparator();

    // Phase 3: custom-widget promotion.  Show "Promote to Custom Widget…"
    // when the target isn't promoted yet, "Demote" when it is — never both.
    // Disabled when no widget was right-clicked (you can't promote the
    // canvas itself).  Tooltip and status-bar message reinforce visibility
    // (UX principle 9).
    if (target) {
        const bool already = m_canvas->isPromoted(target);
        if (!already) {
            auto *promoAct = menu.addAction(tr("Promote to Custom Widget..."));
            promoAct->setToolTip(tr("Use a custom QWidget subclass at runtime "
                                    "(writes <customwidgets> in the .ui)"));
            QObject::connect(promoAct, &QAction::triggered, this,
                             [this, target](){ promoteWidget(target); });
        } else {
            const auto info = m_canvas->promotionFor(target);
            auto *demoteAct = menu.addAction(tr("Demote (currently: %1)")
                                                 .arg(info.className));
            demoteAct->setToolTip(tr("Remove the custom-class promotion and "
                                     "restore the base widget type"));
            QObject::connect(demoteAct, &QAction::triggered, this,
                             [this, target](){ demoteWidget(target); });
            // Re-promote to allow editing the existing entry.  Designer
            // calls this "Promoted Widgets…"; we give it a clearer label.
            auto *editPromoAct = menu.addAction(tr("Edit Promotion..."));
            QObject::connect(editPromoAct, &QAction::triggered, this,
                             [this, target](){ promoteWidget(target); });
        }
        menu.addSeparator();
    }

    auto *delAct = menu.addAction(tr("Delete (Del)"));
    delAct->setEnabled(target != nullptr);
    QObject::connect(delAct, &QAction::triggered, this, [this](){
        m_canvas->deleteSelection();
    });
    menu.exec(globalPos);
}

// Phase 3: open the PromoteDialog for `target` (with current promotion as
// initial values when re-editing) and apply the result.  We set the status
// bar message and schedule a 3-second auto-dismiss to match the project's
// UX principle for transient status updates.
void UiFormEditor::promoteWidget(QWidget *target) {
    if (!target || !m_canvas) return;
    const QString base = QString::fromLatin1(target->metaObject()->className());
    const auto current = m_canvas->promotionFor(target);

    PromoteDialog dlg(base, current.className,
                      current.headerFile, current.globalInclude, this);
    if (dlg.exec() != QDialog::Accepted) return;

    FormCanvas::PromotionInfo info;
    info.className     = dlg.className();
    info.headerFile    = dlg.headerFile();
    info.globalInclude = dlg.globalInclude();
    if (info.className.isEmpty() || info.headerFile.isEmpty()) return;

    m_canvas->setPromotion(target, info);
    setModified(true);
    flashStatus(tr("Promoted to %1").arg(info.className));
}

void UiFormEditor::demoteWidget(QWidget *target) {
    if (!target || !m_canvas) return;
    if (!m_canvas->isPromoted(target)) return;
    const QString prevName = m_canvas->promotionFor(target).className;
    m_canvas->clearPromotion(target);
    setModified(true);
    flashStatus(tr("Demoted from %1").arg(prevName));
}

// Status-bar helper with 3-second auto-dismiss.  We track the last shown
// message via a slot id on the QPointer so a fresh flash supersedes the
// previous timer instead of clearing a newer message prematurely.
void UiFormEditor::flashStatus(const QString &text) {
    if (!m_status) return;
    m_status->setText(text);
    static int gen = 0;
    const int my = ++gen;
    QPointer<QLabel> label(m_status);
    QTimer::singleShot(3000, this, [label, my](){
        if (!label) return;
        if (my != gen) return;     // a newer flash replaced us; leave it alone
        label->setText(QString());
    });
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

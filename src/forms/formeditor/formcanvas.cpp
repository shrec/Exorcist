#include "formcanvas.h"

#include "inlinelabeleditor.h"
#include "widgetpalette.h"   // kFormWidgetMime

#include <QApplication>
#include <QChildEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLayout>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QRect>
#include <QResizeEvent>
#include <QTimer>
#include <QUiLoader>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWidget>

namespace exo::forms {

// ── Undo commands ────────────────────────────────────────────────────────────

namespace {

class InsertCommand : public QUndoCommand {
public:
    InsertCommand(FormCanvas *canvas, QWidget *parentForm,
                  QWidget *child, const QPoint &topLeft)
        : QUndoCommand(QObject::tr("Insert %1")
              .arg(QString::fromLatin1(child->metaObject()->className())))
        , m_canvas(canvas), m_parent(parentForm)
        , m_child(child), m_topLeft(topLeft) {}

    ~InsertCommand() override { if (m_owns) m_child->deleteLater(); }

    void redo() override {
        m_child->setParent(m_parent);
        m_child->move(m_topLeft);
        m_child->show();
        m_owns = false;
        m_canvas->selectOnly(m_child);
    }
    void undo() override {
        m_canvas->clearSelection();
        m_child->setParent(nullptr);
        m_child->hide();
        m_owns = true;
    }

private:
    FormCanvas *m_canvas;
    QWidget    *m_parent;
    QWidget    *m_child;
    QPoint      m_topLeft;
    bool        m_owns = true;
};

class DeleteCommand : public QUndoCommand {
public:
    DeleteCommand(FormCanvas *canvas, const QList<QWidget *> &victims)
        : QUndoCommand(QObject::tr("Delete %1 widget(s)").arg(victims.size()))
        , m_canvas(canvas) {
        for (QWidget *w : victims)
            m_entries.append({w, w->parentWidget(), w->geometry()});
    }

    ~DeleteCommand() override {
        if (m_detached) for (auto &e : m_entries) e.widget->deleteLater();
    }

    void redo() override {
        m_canvas->clearSelection();
        for (auto &e : m_entries) {
            e.widget->setParent(nullptr);
            e.widget->hide();
        }
        m_detached = true;
    }
    void undo() override {
        for (auto &e : m_entries) {
            e.widget->setParent(e.parent);
            e.widget->setGeometry(e.geom);
            e.widget->show();
        }
        m_detached = false;
    }

private:
    struct Entry { QWidget *widget; QWidget *parent; QRect geom; };
    FormCanvas *m_canvas;
    QList<Entry> m_entries;
    bool m_detached = false;
};

class MoveCommand : public QUndoCommand {
public:
    MoveCommand(const QHash<QWidget *, QPoint> &before,
                const QHash<QWidget *, QPoint> &after)
        : QUndoCommand(QObject::tr("Move widget(s)"))
        , m_before(before), m_after(after) {}
    void redo() override {
        for (auto it = m_after.begin(); it != m_after.end(); ++it)
            it.key()->move(it.value());
    }
    void undo() override {
        for (auto it = m_before.begin(); it != m_before.end(); ++it)
            it.key()->move(it.value());
    }
private:
    QHash<QWidget *, QPoint> m_before, m_after;
};

// Phase 2: Wrap selected widgets in a new container with the requested
// layout.  Stores enough state to reverse the operation: the original
// parent + geometry of every wrapped child, plus the container we created.
class LayoutCommand : public QUndoCommand {
public:
    LayoutCommand(FormCanvas *canvas,
                  QWidget *parentForm,
                  const QList<QWidget *> &children,
                  FormCanvas::LayoutKind kind)
        : QUndoCommand(layoutLabel(kind))
        , m_canvas(canvas), m_parent(parentForm), m_kind(kind) {
        for (QWidget *w : children)
            m_entries.append({w, w->parentWidget(), w->geometry()});
    }

    ~LayoutCommand() override {
        // If we still own the container (i.e. command undone), clean up.
        if (m_container && !m_active) m_container->deleteLater();
    }

    void redo() override {
        if (m_entries.isEmpty()) return;

        // Compute the bounding box of the selection in parent coords; the
        // container is created spanning that area.
        QRect bounds = m_entries.first().geom;
        for (const auto &e : m_entries) bounds = bounds.united(e.geom);

        if (!m_container) {
            m_container = new QWidget(m_parent);
            m_container->setObjectName(generateContainerName(m_parent));
            m_container->setStyleSheet(
                "QWidget{background:transparent;border:1px dashed #3c3c3c;}");
        }
        m_container->setParent(m_parent);
        m_container->setGeometry(bounds);

        // Sort entries left-to-right, top-to-bottom for sensible ordering.
        QList<Entry> sorted = m_entries;
        std::sort(sorted.begin(), sorted.end(), [](const Entry &a, const Entry &b){
            if (qAbs(a.geom.y() - b.geom.y()) > 8) return a.geom.y() < b.geom.y();
            return a.geom.x() < b.geom.x();
        });

        QLayout *layout = nullptr;
        switch (m_kind) {
        case FormCanvas::LayoutHorizontal: {
            auto *h = new QHBoxLayout(m_container);
            for (const auto &e : sorted) {
                e.widget->setParent(m_container);
                h->addWidget(e.widget);
                e.widget->show();
            }
            layout = h; break;
        }
        case FormCanvas::LayoutVertical: {
            auto *v = new QVBoxLayout(m_container);
            for (const auto &e : sorted) {
                e.widget->setParent(m_container);
                v->addWidget(e.widget);
                e.widget->show();
            }
            layout = v; break;
        }
        case FormCanvas::LayoutGrid: {
            auto *g = new QGridLayout(m_container);
            // Heuristic: infer rows/cols from x/y clustering.
            const int n = sorted.size();
            const int cols = qMax(1, int(qRound(qSqrt(double(n)))));
            for (int i = 0; i < n; ++i) {
                sorted[i].widget->setParent(m_container);
                g->addWidget(sorted[i].widget, i / cols, i % cols);
                sorted[i].widget->show();
            }
            layout = g; break;
        }
        case FormCanvas::LayoutForm: {
            auto *f = new QFormLayout(m_container);
            // Pair widgets sequentially (label, field).  If odd count, last
            // is field-only.
            for (int i = 0; i + 1 < sorted.size(); i += 2) {
                sorted[i].widget->setParent(m_container);
                sorted[i+1].widget->setParent(m_container);
                f->addRow(sorted[i].widget, sorted[i+1].widget);
                sorted[i].widget->show();
                sorted[i+1].widget->show();
            }
            if (sorted.size() % 2) {
                QWidget *w = sorted.last().widget;
                w->setParent(m_container);
                f->addRow(w);
                w->show();
            }
            layout = f; break;
        }
        case FormCanvas::LayoutBreak:
            // Handled by BreakLayoutCommand instead; keep symmetry.
            break;
        }
        if (layout) layout->setContentsMargins(4, 4, 4, 4);
        m_container->show();
        m_canvas->selectOnly(m_container);
        m_active = true;
    }

    void undo() override {
        if (!m_container) return;
        // Take children out of the layout, restore their original parent +
        // geometry.
        for (const auto &e : m_entries) {
            if (!e.widget) continue;
            e.widget->setParent(e.parent);
            e.widget->setGeometry(e.geom);
            e.widget->show();
        }
        // Detach + drop the container so it stops claiming geometry.
        m_container->setParent(nullptr);
        m_container->hide();
        m_canvas->clearSelection();
        m_active = false;
    }

private:
    struct Entry { QWidget *widget; QWidget *parent; QRect geom; };

    static QString layoutLabel(FormCanvas::LayoutKind k) {
        switch (k) {
        case FormCanvas::LayoutHorizontal: return QObject::tr("Lay Out Horizontally");
        case FormCanvas::LayoutVertical:   return QObject::tr("Lay Out Vertically");
        case FormCanvas::LayoutGrid:       return QObject::tr("Lay Out in Grid");
        case FormCanvas::LayoutForm:       return QObject::tr("Lay Out in Form");
        case FormCanvas::LayoutBreak:      return QObject::tr("Break Layout");
        }
        return QObject::tr("Layout");
    }

    static QString generateContainerName(QWidget *parent) {
        QString stem = QStringLiteral("layoutWidget");
        QString name = stem;
        int idx = 2;
        while (parent && parent->findChild<QWidget *>(name, Qt::FindDirectChildrenOnly))
            name = stem + QStringLiteral("_") + QString::number(idx++);
        return name;
    }

    FormCanvas              *m_canvas;
    QWidget                 *m_parent;
    QList<Entry>             m_entries;
    FormCanvas::LayoutKind   m_kind;
    QWidget                 *m_container = nullptr;
    bool                     m_active    = false;
};

// Phase 2: Break Layout — opposite of LayoutCommand.  Removes the layout
// from `target`'s parent (the container), reparents children directly to
// the grandparent at their absolute positions, and discards the container.
class BreakLayoutCommand : public QUndoCommand {
public:
    BreakLayoutCommand(FormCanvas *canvas, QWidget *container)
        : QUndoCommand(QObject::tr("Break Layout"))
        , m_canvas(canvas), m_container(container) {
        if (!container) return;
        m_grandparent = container->parentWidget();
        m_containerGeom = container->geometry();
        for (QObject *o : container->children()) {
            QWidget *w = qobject_cast<QWidget *>(o);
            if (!w) continue;
            // Map child's pos to grandparent coords (its current pos in
            // container + container's pos in grandparent).
            const QPoint absPos = w->pos() + container->pos();
            m_entries.append({w, w->geometry(), absPos, w->size()});
        }
    }

    ~BreakLayoutCommand() override {
        if (m_container && m_broken) m_container->deleteLater();
    }

    void redo() override {
        if (!m_container || !m_grandparent) return;
        // Detach layout, reparent children up, position them absolutely.
        if (auto *lay = m_container->layout()) {
            // Setting layout to nullptr is non-trivial in Qt — easiest is to
            // delete the layout.  Children stay parented to the container
            // until we reparent them.
            delete lay;
        }
        for (auto &e : m_entries) {
            if (!e.widget) continue;
            e.widget->setParent(m_grandparent);
            e.widget->setGeometry(QRect(e.absolutePos, e.size));
            e.widget->show();
        }
        m_container->setParent(nullptr);
        m_container->hide();
        m_canvas->clearSelection();
        m_broken = true;
    }

    void undo() override {
        if (!m_container || !m_grandparent) return;
        m_container->setParent(m_grandparent);
        m_container->setGeometry(m_containerGeom);
        // The layout is gone; we can't easily reconstruct it without storing
        // its kind, so we re-layout vertically as a safe default — this is
        // acknowledged as imperfect, but rare (user already asked to break it).
        auto *layout = new QVBoxLayout(m_container);
        layout->setContentsMargins(4, 4, 4, 4);
        for (auto &e : m_entries) {
            if (!e.widget) continue;
            e.widget->setParent(m_container);
            layout->addWidget(e.widget);
            e.widget->show();
        }
        m_container->show();
        m_canvas->selectOnly(m_container);
        m_broken = false;
    }

private:
    struct Entry { QWidget *widget; QRect originalGeom; QPoint absolutePos; QSize size; };

    FormCanvas  *m_canvas;
    QWidget     *m_container;
    QWidget     *m_grandparent  = nullptr;
    QRect        m_containerGeom;
    QList<Entry> m_entries;
    bool         m_broken = false;
};

} // namespace

// ── FormCanvas ───────────────────────────────────────────────────────────────

FormCanvas::FormCanvas(QWidget *parent) : QWidget(parent) {
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(true);
    setStyleSheet("FormCanvas{background:#252526;}");

    auto *blank = new QWidget(this);
    blank->setObjectName("Form");
    blank->setGeometry(20, 20, 640, 480);
    blank->setAutoFillBackground(true);
    blank->setStyleSheet(
        "QWidget#Form{background:#1e1e1e;border:1px solid #3c3c3c;}");
    m_root = blank;

    // Phase 2: build inline editor lazily so we hand it the canvas only once.
    m_inlineEditor = new InlineLabelEditor(this);
}

void FormCanvas::setRoot(QWidget *newRoot) {
    if (!newRoot || newRoot == m_root) return;
    clearSelection();
    if (m_root) m_root->deleteLater();
    m_root = newRoot;
    m_root->setParent(this);
    if (!m_root->geometry().isValid() || m_root->size().isEmpty())
        m_root->setGeometry(20, 20, 640, 480);
    else if (m_root->pos().isNull())
        m_root->move(20, 20);
    m_root->show();
    update();
    emit selectionChanged();
}

void FormCanvas::setUndoStack(QUndoStack *stack) { m_undo = stack; }

QList<QWidget *> FormCanvas::selection() const { return m_selection.values(); }

void FormCanvas::clearSelection() {
    if (m_selection.isEmpty() && !m_primary) return;
    m_selection.clear();
    m_primary = nullptr;
    update();
    emit selectionChanged();
}

void FormCanvas::selectOnly(QWidget *w) {
    m_selection.clear();
    if (w && w != m_root) m_selection.insert(w);
    m_primary = w;
    update();
    emit selectionChanged();
}

void FormCanvas::selectAll() {
    if (!m_root) return;
    m_selection.clear();
    for (QObject *o : m_root->children()) {
        QWidget *w = qobject_cast<QWidget *>(o);
        if (w && !w->property("exo_isHandle").toBool()) m_selection.insert(w);
    }
    m_primary = m_selection.isEmpty() ? nullptr : *m_selection.begin();
    update();
    emit selectionChanged();
}

void FormCanvas::setPreviewMode(bool on) {
    if (m_preview == on) return;
    m_preview = on;
    if (on) {
        clearSelection();
        if (m_inlineEditor && m_inlineEditor->isEditing())
            m_inlineEditor->cancelEdit();
    }
    update();
}

QWidget *FormCanvas::hitTest(const QPoint &p) const {
    if (!m_root) return nullptr;
    if (!m_root->geometry().contains(p)) return nullptr;
    const QPoint inRoot = p - m_root->pos();
    const QList<QObject *> kids = m_root->children();
    for (auto it = kids.rbegin(); it != kids.rend(); ++it) {
        QWidget *w = qobject_cast<QWidget *>(*it);
        if (!w || !w->isVisible()) continue;
        if (w->property("exo_isHandle").toBool()) continue;
        if (w->geometry().contains(inRoot)) return w;
    }
    return m_root;
}

QWidget *FormCanvas::insertWidget(const QString &className, const QPoint &where) {
    if (!m_root) return nullptr;
    QUiLoader loader;
    QWidget *w = loader.createWidget(className, m_root);
    if (!w) {
        qWarning("FormCanvas::insertWidget: QUiLoader could not create %s",
                 qUtf8Printable(className));
        return nullptr;
    }
    if (w->objectName().isEmpty()) {
        QString stem = className;
        if (stem.startsWith('Q')) stem = stem.mid(1);
        stem[0] = stem[0].toLower();
        QString name = stem;
        int idx = 2;
        while (m_root->findChild<QWidget *>(name, Qt::FindDirectChildrenOnly))
            name = stem + QStringLiteral("_") + QString::number(idx++);
        w->setObjectName(name);
    }
    w->resize(w->sizeHint().expandedTo(QSize(80, 24)));
    const QPoint topLeft = where - m_root->pos() - QPoint(w->width()/2, w->height()/2);

    if (m_undo) m_undo->push(new InsertCommand(this, m_root, w, topLeft));
    else { w->setParent(m_root); w->move(topLeft); w->show(); selectOnly(w); }

    emit modified();
    return w;
}

void FormCanvas::deleteSelection() {
    if (m_selection.isEmpty()) return;
    QList<QWidget *> victims = m_selection.values();
    if (m_undo) m_undo->push(new DeleteCommand(this, victims));
    else {
        for (QWidget *w : victims) w->deleteLater();
        clearSelection();
    }
    emit modified();
}

void FormCanvas::duplicateSelection() {
    if (m_selection.isEmpty() || !m_root) return;
    QList<QWidget *> originals = m_selection.values();
    QUiLoader loader;
    QList<QWidget *> created;
    for (QWidget *src : originals) {
        const QString cls = QString::fromLatin1(src->metaObject()->className());
        QWidget *copy = loader.createWidget(cls, m_root);
        if (!copy) continue;
        copy->setGeometry(src->geometry().translated(12, 12));
        if (auto *bSrc = qobject_cast<QPushButton *>(src))
            if (auto *bDst = qobject_cast<QPushButton *>(copy))
                bDst->setText(bSrc->text());
        copy->show();
        created << copy;
    }
    m_selection = QSet<QWidget *>(created.begin(), created.end());
    m_primary = created.isEmpty() ? nullptr : created.last();
    update();
    emit selectionChanged();
    emit modified();
}

void FormCanvas::nudgeSelection(int dx, int dy) {
    if (m_selection.isEmpty()) return;
    QHash<QWidget *, QPoint> before, after;
    for (QWidget *w : m_selection) {
        before[w] = w->pos();
        after[w]  = w->pos() + QPoint(dx, dy);
    }
    if (m_undo) m_undo->push(new MoveCommand(before, after));
    else for (auto it = after.begin(); it != after.end(); ++it) it.key()->move(it.value());
    update();
    emit modified();
}

// ── Phase 2: layout commands ─────────────────────────────────────────────────

void FormCanvas::applyLayoutToSelection(LayoutKind kind) {
    if (!m_root) return;
    if (kind == LayoutBreak) {
        // For Break: target is the primary widget if it's a layout container,
        // OR the parent of the primary if the primary lives inside a layout.
        QWidget *target = m_primary;
        if (!target) return;
        if (!target->layout()) target = target->parentWidget();
        if (!target || !target->layout() || target == m_root) return;
        if (m_undo) m_undo->push(new BreakLayoutCommand(this, target));
        emit modified();
        return;
    }

    QList<QWidget *> sel = m_selection.values();
    if (sel.size() < 2) return;
    // All selected widgets must share a parent for the wrap to make sense.
    QWidget *parent = sel.first()->parentWidget();
    for (QWidget *w : sel)
        if (w->parentWidget() != parent) return;
    if (m_undo) m_undo->push(new LayoutCommand(this, parent, sel, kind));
    emit modified();
}

// ── Phase 2: inline label edit ───────────────────────────────────────────────

void FormCanvas::beginInlineEdit(QWidget *target) {
    if (!target || !m_inlineEditor) return;
    if (m_inlineEditor->isEditing()) m_inlineEditor->commitEdit();
    m_inlineEditor->beginEdit(target, m_undo);
}

// ── Phase 2: connection storage ──────────────────────────────────────────────

void FormCanvas::setConnections(const QList<FormConnection> &c) {
    m_connections = c;
    emit modified();
}

QWidget *FormCanvas::findNamedWidget(const QString &name) const {
    if (!m_root || name.isEmpty()) return nullptr;
    if (m_root->objectName() == name) return m_root;
    return m_root->findChild<QWidget *>(name);
}

// ── Phase 2: flash highlight ─────────────────────────────────────────────────

void FormCanvas::flashHighlight(QWidget *w) {
    if (!w) return;
    m_flashTarget = w;
    m_flashFrame  = 0;
    update();
    // 6-frame pulse over ~600 ms.  Each tick repaints with decreasing alpha.
    auto *timer = new QTimer(this);
    timer->setInterval(100);
    connect(timer, &QTimer::timeout, this, [this, timer](){
        ++m_flashFrame;
        update();
        if (m_flashFrame >= 6) {
            m_flashTarget = nullptr;
            timer->deleteLater();
        }
    });
    timer->start();
}

void FormCanvas::notifyExternalChange(QWidget *) {
    update();
    emit modified();
    emit selectionChanged();   // refresh inspector
}

// ── Phase 3: custom-widget promotion ────────────────────────────────────────

bool FormCanvas::isPromoted(QWidget *w) const {
    return w && m_promotions.contains(w);
}

FormCanvas::PromotionInfo FormCanvas::promotionFor(QWidget *w) const {
    return w ? m_promotions.value(w) : PromotionInfo{};
}

void FormCanvas::setPromotion(QWidget *w, const PromotionInfo &info) {
    if (!w) return;
    if (info.className.isEmpty()) {
        m_promotions.remove(w);
    } else {
        m_promotions.insert(w, info);
        // Tag widget with a dynamic property too — keeps the info visible
        // in property-browser tools and survives moc round-trips for
        // future tests that introspect the widget.
        w->setProperty("exo_promoted_class",  info.className);
        w->setProperty("exo_promoted_header", info.headerFile);
        w->setProperty("exo_promoted_global", info.globalInclude);
        // Tooltip surfaces the header file so hovering the chip (or the
        // widget itself) tells the user where the type lives — UX
        // principle 9, visual feedback without clicking.
        const QString quote = info.globalInclude ? QStringLiteral("&lt;%1&gt;")
                                                 : QStringLiteral("\"%1\"");
        w->setToolTip(QStringLiteral("Promoted: %1  —  #include %2")
                          .arg(info.className, quote.arg(info.headerFile)));
    }
    update();
    emit modified();
    emit selectionChanged();   // inspector should refresh badge
}

void FormCanvas::clearPromotion(QWidget *w) {
    if (!w) return;
    if (!m_promotions.remove(w)) return;
    w->setProperty("exo_promoted_class",  QVariant());
    w->setProperty("exo_promoted_header", QVariant());
    w->setProperty("exo_promoted_global", QVariant());
    w->setToolTip(QString());
    update();
    emit modified();
    emit selectionChanged();
}

void FormCanvas::setPromotionsByName(const QHash<QString, PromotionInfo> &byName) {
    m_promotions.clear();
    if (!m_root || byName.isEmpty()) { update(); return; }
    // Walk every descendant of m_root and resolve names → live widgets.
    // We use findChildren so this works regardless of layout depth.
    const QList<QWidget *> all = m_root->findChildren<QWidget *>();
    for (QWidget *w : all) {
        const QString name = w->objectName();
        if (name.isEmpty()) continue;
        auto it = byName.find(name);
        if (it == byName.end()) continue;
        m_promotions.insert(w, it.value());
        w->setProperty("exo_promoted_class",  it.value().className);
        w->setProperty("exo_promoted_header", it.value().headerFile);
        w->setProperty("exo_promoted_global", it.value().globalInclude);
        const QString quote = it.value().globalInclude
                                  ? QStringLiteral("&lt;%1&gt;")
                                  : QStringLiteral("\"%1\"");
        w->setToolTip(QStringLiteral("Promoted: %1  —  #include %2")
                          .arg(it.value().className,
                               quote.arg(it.value().headerFile)));
    }
    update();
}

// ── Painting ─────────────────────────────────────────────────────────────────

void FormCanvas::paintEvent(QPaintEvent *ev) {
    QWidget::paintEvent(ev);
    if (m_preview) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    paintLayoutCells();

    // Phase 3: promotion badges paint underneath selection so the selection
    // accent always wins on the active widget.
    paintPromotionBadges();

    for (QWidget *w : m_selection) paintSelectionOverlay(w);

    if (m_dragging && !m_guideLines.isEmpty()) {
        QPen guide(QColor("#007acc"), 1, Qt::SolidLine);
        p.setPen(guide);
        for (const Guide &g : m_guideLines) {
            const int origin = m_root ? (g.vertical ? m_root->x() : m_root->y()) : 0;
            const int abs = origin + g.pos;
            if (g.vertical) p.drawLine(abs, 0, abs, height());
            else            p.drawLine(0, abs, width(), abs);
        }
    }

    if (m_marquee) {
        QRect r = QRect(m_marqueeStart, m_marqueeEnd).normalized();
        QColor fill("#007acc"); fill.setAlpha(40);
        p.fillRect(r, fill);
        p.setPen(QPen(QColor("#007acc"), 1, Qt::DashLine));
        p.drawRect(r);
    }

    paintFlashOverlay();
}

void FormCanvas::paintSelectionOverlay(QWidget *w) {
    if (!w || !m_root) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    // Map widget rect to canvas coords by walking up the parent chain to
    // m_root; this works for nested widgets inside layout containers too.
    QRect r = w->geometry();
    QWidget *cur = w->parentWidget();
    while (cur && cur != m_root) {
        r.translate(cur->pos());
        cur = cur->parentWidget();
    }
    r.translate(m_root->pos());
    p.setPen(QPen(QColor("#007acc"), 1, Qt::SolidLine));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(0, 0, -1, -1));

    const int G = 6;
    const QPoint mid = r.center();
    QList<QPoint> pts = {
        r.topLeft(), QPoint(mid.x(), r.top()), r.topRight(),
        QPoint(r.left(), mid.y()),             QPoint(r.right(), mid.y()),
        r.bottomLeft(), QPoint(mid.x(), r.bottom()), r.bottomRight(),
    };
    p.setBrush(QColor("#007acc"));
    p.setPen(QPen(QColor("#ffffff"), 1));
    for (const QPoint &c : pts) {
        QRect g(c.x() - G/2, c.y() - G/2, G, G);
        p.drawRect(g);
    }
}

// Phase 2: visualize layout cell boundaries when dragging over a container
// that owns a layout.  We draw thin orange dashed lines marking each cell's
// rect — gives the user a "drop here" signal for layout-aware drops.
void FormCanvas::paintLayoutCells() {
    if (!m_root) return;
    QWidget *target = m_layoutHover ? m_layoutHover.data() : nullptr;
    if (!target && m_dragging && m_primary) {
        // While dragging a free widget, see if cursor is over a layout container.
        const QPoint cursor = mapFromGlobal(QCursor::pos());
        QWidget *hit = hitTest(cursor);
        if (hit && hit->layout()) target = hit;
    }
    if (!target || !target->layout()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen pen(QColor("#d28445"), 1, Qt::DashLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    // Map each child cell to canvas coords.
    QLayout *layout = target->layout();
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem *item = layout->itemAt(i);
        if (!item || !item->widget()) continue;
        QWidget *cw = item->widget();
        QRect r = cw->geometry();
        QWidget *cur = cw->parentWidget();
        while (cur && cur != m_root) {
            r.translate(cur->pos());
            cur = cur->parentWidget();
        }
        if (m_root) r.translate(m_root->pos());
        // Pad each cell slightly so the boundary is visible distinct from
        // the widget edge.
        p.drawRect(r.adjusted(-2, -2, 2, 2));
    }
}

void FormCanvas::paintFlashOverlay() {
    if (!m_flashTarget || !m_root) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    QRect r = m_flashTarget->geometry();
    QWidget *cur = m_flashTarget->parentWidget();
    while (cur && cur != m_root) { r.translate(cur->pos()); cur = cur->parentWidget(); }
    if (m_root) r.translate(m_root->pos());
    const int alpha = qMax(0, 200 - m_flashFrame * 30);
    QColor c("#ffd166"); c.setAlpha(alpha);
    p.setBrush(c);
    p.setPen(QPen(QColor("#ffd166"), 2));
    p.drawRect(r.adjusted(-2, -2, 2, 2));
}

QRect FormCanvas::mapToCanvas(QWidget *w) const {
    if (!w || !m_root) return QRect();
    QRect r = w->geometry();
    QWidget *cur = w->parentWidget();
    while (cur && cur != m_root) {
        r.translate(cur->pos());
        cur = cur->parentWidget();
    }
    r.translate(m_root->pos());
    return r;
}

// Phase 3: paint a thin purple border + small chip with the promoted class
// name in the top-right corner of every promoted widget.  The border colour
// (#c586c0, the VS Code "type" purple) is unmistakably distinct from the
// blue selection accent so the two states never get confused.
void FormCanvas::paintPromotionBadges() {
    if (m_promotions.isEmpty() || !m_root) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QColor purple("#c586c0");
    QFont chipFont = font();
    chipFont.setPointSize(qMax(7, chipFont.pointSize() - 1));
    chipFont.setBold(true);
    p.setFont(chipFont);
    const QFontMetrics fm(chipFont);

    for (auto it = m_promotions.begin(); it != m_promotions.end(); ++it) {
        QWidget *w = it.key();
        if (!w || !w->isVisible()) continue;
        // Skip widgets that aren't actually parented under the form (could
        // happen mid-undo — we keep the info around in case redo restores
        // the widget, but we don't paint it dangling).
        if (!w->parentWidget()) continue;
        const QRect r = mapToCanvas(w);
        if (r.isEmpty()) continue;

        // Thin purple border, dashed to read clearly against the widget's
        // own border (Qt widgets often paint their own 1px frame).
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(purple, 1, Qt::DashLine));
        p.drawRect(r.adjusted(0, 0, -1, -1));

        // Chip: top-right, slightly inset.  Width fits class name + 8px
        // padding; height is fontHeight + 2px.  We position it inside the
        // widget rect so it doesn't overflow when the widget sits flush
        // against the canvas edge.
        const QString text = it.value().className;
        const int textW = fm.horizontalAdvance(text);
        const int chipW = qMin(r.width() - 4, textW + 8);
        if (chipW < 12) continue;       // too narrow to render meaningfully
        const int chipH = fm.height() + 2;
        QRect chip(r.right() - chipW - 2, r.top() + 2, chipW, chipH);

        QPainterPath rounded;
        rounded.addRoundedRect(chip, 2.0, 2.0);
        p.setBrush(purple);
        p.setPen(Qt::NoPen);
        p.drawPath(rounded);

        p.setPen(QColor("#1e1e1e"));
        p.drawText(chip, Qt::AlignCenter, text);
    }
}

// ── Mouse / keyboard ─────────────────────────────────────────────────────────

void FormCanvas::mousePressEvent(QMouseEvent *ev) {
    if (m_preview) { QWidget::mousePressEvent(ev); return; }
    // If inline editor is active, clicking elsewhere commits it.
    if (m_inlineEditor && m_inlineEditor->isEditing())
        m_inlineEditor->commitEdit();

    if (ev->button() != Qt::LeftButton) { QWidget::mousePressEvent(ev); return; }

    setFocus();
    QWidget *hit = hitTest(ev->pos());
    const bool ctrl  = ev->modifiers() & Qt::ControlModifier;
    const bool shift = ev->modifiers() & Qt::ShiftModifier;

    if (!hit || hit == m_root) {
        if (!ctrl && !shift) clearSelection();
        m_marquee = true;
        m_marqueeAdditive = ctrl || shift;
        m_marqueeStart = ev->pos();
        m_marqueeEnd   = ev->pos();
        update();
        return;
    }

    if (ctrl) {
        if (m_selection.contains(hit)) m_selection.remove(hit);
        else                           m_selection.insert(hit);
        m_primary = hit;
    } else if (!m_selection.contains(hit)) {
        m_selection.clear();
        m_selection.insert(hit);
        m_primary = hit;
    } else {
        m_primary = hit;
    }
    emit selectionChanged();

    m_dragging = true;
    m_dragStartPos = ev->pos();
    m_dragOrigin.clear();
    for (QWidget *w : m_selection) m_dragOrigin[w] = w->pos();
    update();
}

void FormCanvas::mouseMoveEvent(QMouseEvent *ev) {
    if (m_marquee) {
        m_marqueeEnd = ev->pos();
        update();
        return;
    }
    if (m_dragging && (ev->buttons() & Qt::LeftButton)) {
        const QPoint delta = ev->pos() - m_dragStartPos;
        for (QWidget *w : m_selection) {
            const QPoint origin = m_dragOrigin.value(w);
            QPoint desired = origin + delta;
            if (w == m_primary) desired = snapTo(w, desired);
            w->move(desired);
        }
        // While dragging, see if we're over a layout container — if so,
        // store it so paintLayoutCells() can highlight cells.
        QWidget *hover = hitTest(ev->pos());
        if (hover && hover->layout() && !m_selection.contains(hover))
            m_layoutHover = hover;
        else
            m_layoutHover = nullptr;
        update();
        return;
    }
    QWidget::mouseMoveEvent(ev);
}

void FormCanvas::mouseReleaseEvent(QMouseEvent *ev) {
    if (m_marquee) {
        m_marquee = false;
        const QRect r = QRect(m_marqueeStart, m_marqueeEnd).normalized();
        if (m_root) {
            const QRect rRoot = r.translated(-m_root->pos());
            if (!m_marqueeAdditive) m_selection.clear();
            for (QObject *o : m_root->children()) {
                QWidget *w = qobject_cast<QWidget *>(o);
                if (!w || !w->isVisible()) continue;
                if (w->property("exo_isHandle").toBool()) continue;
                if (rRoot.intersects(w->geometry())) m_selection.insert(w);
            }
            m_primary = m_selection.isEmpty() ? nullptr : *m_selection.begin();
            emit selectionChanged();
        }
        update();
        return;
    }
    if (m_dragging) {
        m_dragging = false;
        m_layoutHover = nullptr;
        QHash<QWidget *, QPoint> after;
        bool changed = false;
        for (auto it = m_dragOrigin.begin(); it != m_dragOrigin.end(); ++it) {
            after[it.key()] = it.key()->pos();
            if (it.key()->pos() != it.value()) changed = true;
        }
        if (changed && m_undo) m_undo->push(new MoveCommand(m_dragOrigin, after));
        if (changed) emit modified();
        m_dragOrigin.clear();
        m_guideLines.clear();
        update();
        return;
    }
    QWidget::mouseReleaseEvent(ev);
}

void FormCanvas::mouseDoubleClickEvent(QMouseEvent *ev) {
    if (m_preview) { QWidget::mouseDoubleClickEvent(ev); return; }
    QWidget *hit = hitTest(ev->pos());
    if (!hit || hit == m_root) { QWidget::mouseDoubleClickEvent(ev); return; }
    selectOnly(hit);
    // Phase 2: if the widget exposes "text" or "title", begin inline edit.
    const QMetaObject *mo = hit->metaObject();
    if (mo->indexOfProperty("text") >= 0 || mo->indexOfProperty("title") >= 0)
        beginInlineEdit(hit);
}

void FormCanvas::contextMenuEvent(QContextMenuEvent *ev) {
    if (m_preview) return;
    QWidget *hit = hitTest(ev->pos());
    if (hit && hit != m_root) selectOnly(hit);
    emit contextMenuOnWidget(hit && hit != m_root ? hit : nullptr,
                             ev->globalPos());
    ev->accept();
}

void FormCanvas::keyPressEvent(QKeyEvent *ev) {
    if (m_preview) { QWidget::keyPressEvent(ev); return; }
    const int step = (ev->modifiers() & Qt::ShiftModifier) ? 10 : 1;
    switch (ev->key()) {
    case Qt::Key_Left:   nudgeSelection(-step, 0);   ev->accept(); return;
    case Qt::Key_Right:  nudgeSelection( step, 0);   ev->accept(); return;
    case Qt::Key_Up:     nudgeSelection(0, -step);   ev->accept(); return;
    case Qt::Key_Down:   nudgeSelection(0,  step);   ev->accept(); return;
    case Qt::Key_Delete: deleteSelection();          ev->accept(); return;
    case Qt::Key_Escape: clearSelection();           ev->accept(); return;
    case Qt::Key_F2:     // Phase 2: VS-style inline rename
        if (m_primary) { beginInlineEdit(m_primary); ev->accept(); return; }
        break;
    default: break;
    }
    if ((ev->modifiers() & Qt::ControlModifier) && ev->key() == Qt::Key_A) {
        selectAll(); ev->accept(); return;
    }
    if ((ev->modifiers() & Qt::ControlModifier) && ev->key() == Qt::Key_D) {
        duplicateSelection(); ev->accept(); return;
    }
    QWidget::keyPressEvent(ev);
}

void FormCanvas::resizeEvent(QResizeEvent *ev) {
    QWidget::resizeEvent(ev);
    // Reposition inline editor if active (canvas resize would shift target).
    if (m_inlineEditor && m_inlineEditor->isEditing() && m_primary)
        m_inlineEditor->setGeometry(
            m_primary->geometry().translated(m_root ? m_root->pos() : QPoint(0,0)));
}

// ── Drag-and-drop from palette ───────────────────────────────────────────────

void FormCanvas::dragEnterEvent(QDragEnterEvent *ev) {
    if (ev->mimeData()->hasFormat(kFormWidgetMime)) ev->acceptProposedAction();
}
void FormCanvas::dragMoveEvent(QDragMoveEvent *ev) {
    if (ev->mimeData()->hasFormat(kFormWidgetMime)) ev->acceptProposedAction();
}
void FormCanvas::dropEvent(QDropEvent *ev) {
    if (!ev->mimeData()->hasFormat(kFormWidgetMime)) return;
    const QString cls = QString::fromUtf8(ev->mimeData()->data(kFormWidgetMime));
    insertWidget(cls, ev->position().toPoint());
    ev->acceptProposedAction();
    setFocus();
}

// ── Smart guides ─────────────────────────────────────────────────────────────

QPoint FormCanvas::snapTo(QWidget *target, const QPoint &desiredTopLeft) {
    m_guideLines.clear();
    if (!m_root || !target) return desiredTopLeft;

    constexpr int kSnap = 5;
    const QSize sz = target->size();
    QPoint snapped = desiredTopLeft;

    QList<int> vEdges, hEdges;
    for (QObject *o : m_root->children()) {
        QWidget *w = qobject_cast<QWidget *>(o);
        if (!w || w == target || !w->isVisible()) continue;
        if (w->property("exo_isHandle").toBool()) continue;
        const QRect r = w->geometry();
        vEdges << r.left() << r.center().x() << r.right();
        hEdges << r.top()  << r.center().y() << r.bottom();
    }
    const QRect rootRect(QPoint(0, 0), m_root->size());
    vEdges << rootRect.left() << rootRect.center().x() << rootRect.right();
    hEdges << rootRect.top()  << rootRect.center().y() << rootRect.bottom();

    const int targetEdges[3] = {
        snapped.x(), snapped.x() + sz.width()/2, snapped.x() + sz.width()
    };
    for (int i = 0; i < 3; ++i) {
        int best = INT_MAX, bestEdge = -1;
        for (int e : vEdges) {
            int d = qAbs(e - targetEdges[i]);
            if (d < best) { best = d; bestEdge = e; }
        }
        if (best <= kSnap && bestEdge >= 0) {
            snapped.setX(bestEdge - (i == 1 ? sz.width()/2 : (i == 2 ? sz.width() : 0)));
            m_guideLines.append({true, bestEdge});
            break;
        }
    }
    const int targetEdgesY[3] = {
        snapped.y(), snapped.y() + sz.height()/2, snapped.y() + sz.height()
    };
    for (int i = 0; i < 3; ++i) {
        int best = INT_MAX, bestEdge = -1;
        for (int e : hEdges) {
            int d = qAbs(e - targetEdgesY[i]);
            if (d < best) { best = d; bestEdge = e; }
        }
        if (best <= kSnap && bestEdge >= 0) {
            snapped.setY(bestEdge - (i == 1 ? sz.height()/2 : (i == 2 ? sz.height() : 0)));
            m_guideLines.append({false, bestEdge});
            break;
        }
    }
    return snapped;
}

} // namespace exo::forms

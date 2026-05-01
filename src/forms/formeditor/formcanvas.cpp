#include "formcanvas.h"

#include "widgetpalette.h"   // kFormWidgetMime

#include <QApplication>
#include <QChildEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QRect>
#include <QUiLoader>
#include <QUndoCommand>
#include <QUndoStack>
#include <QWidget>

namespace exo::forms {

// ── Undo commands ────────────────────────────────────────────────────────────
//
// We keep three primitive commands: insert, delete, and move/resize.  Each is
// a thin wrapper that mutates the live widget tree.  The pattern follows the
// "store the inputs, replay on redo, reverse on undo" approach used by Qt's
// own QUndoCommand examples — straightforward and easy to audit.

namespace {

class InsertCommand : public QUndoCommand {
public:
    InsertCommand(FormCanvas *canvas, QWidget *parentForm,
                  QWidget *child, const QPoint &topLeft)
        : QUndoCommand(QObject::tr("Insert %1")
              .arg(QString::fromLatin1(child->metaObject()->className())))
        , m_canvas(canvas), m_parent(parentForm)
        , m_child(child), m_topLeft(topLeft) {}

    ~InsertCommand() override {
        // If we own the child (i.e. it's currently undone / detached), delete
        // it to avoid leaks.  When the child is in the form, m_owns is false.
        if (m_owns) m_child->deleteLater();
    }

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
    bool        m_owns = true;  // before redo, we hold the orphan
};

class DeleteCommand : public QUndoCommand {
public:
    DeleteCommand(FormCanvas *canvas, const QList<QWidget *> &victims)
        : QUndoCommand(QObject::tr("Delete %1 widget(s)").arg(victims.size()))
        , m_canvas(canvas) {
        for (QWidget *w : victims) {
            m_entries.append({w, w->parentWidget(), w->geometry()});
        }
    }

    ~DeleteCommand() override {
        if (m_detached) {
            for (auto &e : m_entries) e.widget->deleteLater();
        }
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

} // namespace

// ── FormCanvas ───────────────────────────────────────────────────────────────

FormCanvas::FormCanvas(QWidget *parent) : QWidget(parent) {
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(true);
    setStyleSheet("FormCanvas{background:#252526;}");

    // Default: blank form, 640×480, top-left of the canvas viewport.
    auto *blank = new QWidget(this);
    blank->setObjectName("Form");
    blank->setGeometry(20, 20, 640, 480);
    blank->setAutoFillBackground(true);
    blank->setStyleSheet(
        "QWidget#Form{background:#1e1e1e;border:1px solid #3c3c3c;}");
    m_root = blank;
}

// Replace the entire root.  Existing root is deleted.  Selection cleared.
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

QList<QWidget *> FormCanvas::selection() const {
    return m_selection.values();
}

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
    if (on) clearSelection();
    update();
}

QWidget *FormCanvas::hitTest(const QPoint &p) const {
    if (!m_root) return nullptr;
    if (!m_root->geometry().contains(p)) return nullptr;
    const QPoint inRoot = p - m_root->pos();
    // Reverse iterate so topmost (last drawn) wins on overlap.
    const QList<QObject *> kids = m_root->children();
    for (auto it = kids.rbegin(); it != kids.rend(); ++it) {
        QWidget *w = qobject_cast<QWidget *>(*it);
        if (!w || !w->isVisible()) continue;
        if (w->property("exo_isHandle").toBool()) continue;
        if (w->geometry().contains(inRoot)) return w;
    }
    return m_root;  // clicking the form itself selects it
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
        // Generate a unique name: pushButton, pushButton_2, …
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
        // Copy a few obvious properties — full prop replication is Phase 2.
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

// ── Painting ─────────────────────────────────────────────────────────────────

void FormCanvas::paintEvent(QPaintEvent *ev) {
    QWidget::paintEvent(ev);
    if (m_preview) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Selection overlays + grips
    for (QWidget *w : m_selection) paintSelectionOverlay(w);

    // Smart guides
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

    // Marquee rectangle
    if (m_marquee) {
        QRect r = QRect(m_marqueeStart, m_marqueeEnd).normalized();
        QColor fill("#007acc"); fill.setAlpha(40);
        p.fillRect(r, fill);
        p.setPen(QPen(QColor("#007acc"), 1, Qt::DashLine));
        p.drawRect(r);
    }
}

void FormCanvas::paintSelectionOverlay(QWidget *w) {
    if (!w || !m_root) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QRect r = w->geometry().translated(m_root->pos());
    p.setPen(QPen(QColor("#007acc"), 1, Qt::SolidLine));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(0, 0, -1, -1));

    // 8 grip handles.  4 px squares; midpoints + corners.
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

// ── Mouse / keyboard ─────────────────────────────────────────────────────────

void FormCanvas::mousePressEvent(QMouseEvent *ev) {
    if (m_preview) { QWidget::mousePressEvent(ev); return; }
    if (ev->button() != Qt::LeftButton) { QWidget::mousePressEvent(ev); return; }

    setFocus();
    QWidget *hit = hitTest(ev->pos());
    const bool ctrl  = ev->modifiers() & Qt::ControlModifier;
    const bool shift = ev->modifiers() & Qt::ShiftModifier;

    if (!hit || hit == m_root) {
        // Click on empty area or the root → start marquee selection.
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

    // Begin drag-move
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
            // Snap only the primary; everything else moves rigidly with it
            // (so multi-select drag preserves spacing).
            if (w == m_primary) {
                desired = snapTo(w, desired);
            }
            w->move(desired);
        }
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
        // Only push undo command if we actually moved.
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
    // Inline label editing for buttons / labels — simple "text" property.
    const QMetaObject *mo = hit->metaObject();
    int idx = mo->indexOfProperty("text");
    if (idx < 0) return;
    // Actual inline QLineEdit overlay is Phase 2; for Phase 1 we just emit
    // a selectionChanged() so the property inspector focuses the "text" cell.
    selectOnly(hit);
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
//
// We compare the moving widget's left/centre/right and top/middle/bottom
// edges to every sibling inside m_root.  When |target_edge - sibling_edge|
// is within kSnap, we snap and emit a guide line.  The implementation is
// O(N) per drag tick, which is fine for forms with under ~100 widgets;
// Designer's own implementation is similar and never bottlenecks.

QPoint FormCanvas::snapTo(QWidget *target, const QPoint &desiredTopLeft) {
    m_guideLines.clear();
    if (!m_root || !target) return desiredTopLeft;

    constexpr int kSnap = 5;
    const QSize sz = target->size();
    QPoint snapped = desiredTopLeft;

    // Build edge candidates from siblings.
    QList<int> vEdges, hEdges;
    for (QObject *o : m_root->children()) {
        QWidget *w = qobject_cast<QWidget *>(o);
        if (!w || w == target || !w->isVisible()) continue;
        if (w->property("exo_isHandle").toBool()) continue;
        const QRect r = w->geometry();
        vEdges << r.left() << r.center().x() << r.right();
        hEdges << r.top()  << r.center().y() << r.bottom();
    }
    // Form root edges (left, centre, right) for "centre on form" snaps.
    const QRect rootRect(QPoint(0, 0), m_root->size());
    vEdges << rootRect.left() << rootRect.center().x() << rootRect.right();
    hEdges << rootRect.top()  << rootRect.center().y() << rootRect.bottom();

    // Vertical alignment (X axis snapping).
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
            break;  // one X snap is enough; avoid jitter
        }
    }
    // Horizontal alignment (Y axis snapping).
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

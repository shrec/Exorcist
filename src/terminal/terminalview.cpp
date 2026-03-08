#include "terminalview.h"
#include "terminalscreen.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

static constexpr QRgb kTermSelectionBg = 0xFF264F78;
static constexpr QRgb kTermSelectionFg = 0xFFFFFFFF;

TerminalView::TerminalView(TerminalScreen *screen, QWidget *parent)
    : QAbstractScrollArea(parent),
      m_screen(screen),
      m_blinkTimer(new QTimer(this))
{
    // Monospace font for terminal — Cascadia Mono > Consolas > system fixed
    m_font = QFont(QStringLiteral("Cascadia Mono"), 10);
    if (!QFontInfo(m_font).fixedPitch())
        m_font = QFont(QStringLiteral("Consolas"), 10);
    if (!QFontInfo(m_font).fixedPitch())
        m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_font.setPointSize(10);
    m_font.setStyleStrategy(QFont::PreferAntialias);
    m_fontBold = m_font;
    m_fontBold.setBold(true);

    QFontMetrics fm(m_font);
    m_cellW = fm.horizontalAdvance(QLatin1Char('M'));
    m_cellH = fm.lineSpacing();

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    viewport()->setCursor(Qt::IBeamCursor);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setFocusProxy(this);
    setContextMenuPolicy(Qt::DefaultContextMenu);

    m_blinkTimer->setInterval(530);
    connect(m_blinkTimer, &QTimer::timeout, this, &TerminalView::blinkCursor);
    m_blinkTimer->start();

    connect(m_screen, &TerminalScreen::damaged, this, &TerminalView::onDamaged);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void TerminalView::onDamaged()
{
    updateScrollbar();
    viewport()->update();
}

void TerminalView::blinkCursor()
{
    if (!m_hasFocus) return;
    m_curBlink = !m_curBlink;

    // Repaint only cursor cell to reduce flicker
    const int sbLines    = m_screen->scrollbackLines();
    const int scrolled   = verticalScrollBar()->value();
    const QPoint cur     = m_screen->cursorPos();
    const int    curRow  = sbLines + cur.y() - scrolled;
    if (curRow >= 0 && curRow * m_cellH < viewport()->height())
        viewport()->update(cur.x() * m_cellW, curRow * m_cellH, m_cellW, m_cellH);
}

void TerminalView::updateScrollbar()
{
    const int sb  = m_screen->scrollbackLines();
    const int rows = m_screen->rows();
    verticalScrollBar()->setRange(0, sb);
    verticalScrollBar()->setPageStep(rows);
    // Auto-scroll to bottom if already near the bottom
    if (verticalScrollBar()->value() >= verticalScrollBar()->maximum() - rows)
        verticalScrollBar()->setValue(sb);
}

void TerminalView::scrollToBottom()
{
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

// ── Painting ──────────────────────────────────────────────────────────────────

void TerminalView::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(viewport());

    const int sbLines  = m_screen->scrollbackLines();
    const int scrolled = verticalScrollBar()->value();
    const int cols     = m_screen->cols();
    const int vpH      = viewport()->height();
    const int viewRows = vpH / m_cellH + 2;
    const QPoint cursor = m_screen->cursorPos();

    // Solid background fill
    p.fillRect(viewport()->rect(), QColor(kTermDefaultBg));

    for (int vr = 0; vr < viewRows; ++vr) {
        const int yPx       = vr * m_cellH;
        if (yPx >= vpH) break;

        const int absRow    = scrolled + vr;
        const int screenRow = absRow - sbLines;

        // ── Pass 1: background rectangles ─────────────────────────────────
        for (int c = 0; c < cols; ++c) {
            const TermCell cell = m_screen->cellAt(c, screenRow);
            QRgb bg = cell.rev ? cell.fg : cell.bg;
            const bool sel = isCellSelected(c, absRow);
            if (sel) bg = kTermSelectionBg;
            if (bg != kTermDefaultBg || sel)
                p.fillRect(c * m_cellW, yPx, m_cellW, m_cellH, QColor(bg));
        }

        // ── Pass 1.5: search highlight ────────────────────────────────────
        if (!m_searchNeedle.isEmpty()) {
            const int needleLen = m_searchNeedle.length();
            for (int i = 0; i < m_searchHits.size(); ++i) {
                if (m_searchHits[i].first != absRow) continue;
                const int startCol = m_searchHits[i].second;
                const bool isCurrent = (i == m_searchCurrent);
                const QColor hlBg = isCurrent ? QColor(0xFFE8A317)
                                              : QColor(0x60E8A317);
                for (int k = 0; k < needleLen && (startCol + k) < cols; ++k) {
                    p.fillRect((startCol + k) * m_cellW, yPx,
                               m_cellW, m_cellH, hlBg);
                }
            }
        }

        // ── Pass 2: text ──────────────────────────────────────────────────
        for (int c = 0; c < cols; ++c) {
            const TermCell cell = m_screen->cellAt(c, screenRow);
            if (cell.cp <= U' ') continue;

            QRgb fg = cell.rev ? cell.bg : cell.fg;
            if (isCellSelected(c, absRow)) fg = kTermSelectionFg;

            // Set font only when attributes change (minor optimisation)
            if (cell.bold || cell.ul) {
                QFont f = cell.bold ? m_fontBold : m_font;
                if (cell.ul) f.setUnderline(true);
                p.setFont(f);
            } else {
                p.setFont(m_font);
            }

            p.setPen(QColor(fg));
            const QRect cr(c * m_cellW, yPx, m_cellW, m_cellH);
            char32_t cp = cell.cp;
            p.drawText(cr, Qt::AlignVCenter | Qt::AlignLeft,
                       QString::fromUcs4(&cp, 1));
        }
    }

    // ── Cursor ────────────────────────────────────────────────────────────
    if (m_screen->cursorVisible()) {
        const int curVr = sbLines + cursor.y() - scrolled;
        if (curVr >= 0 && curVr * m_cellH < vpH) {
            const QRect cr(cursor.x() * m_cellW, curVr * m_cellH,
                           m_cellW, m_cellH);
            if (m_hasFocus && m_curBlink) {
                // Filled block cursor
                p.fillRect(cr, QColor(kTermDefaultFg));
                const TermCell cell = m_screen->cellAt(cursor.x(), cursor.y());
                if (cell.cp > U' ') {
                    p.setFont(m_font);
                    p.setPen(QColor(kTermDefaultBg));
                    char32_t cp = cell.cp;
                    p.drawText(cr, Qt::AlignVCenter | Qt::AlignLeft,
                               QString::fromUcs4(&cp, 1));
                }
            } else {
                // Hollow rectangle cursor
                p.setPen(QColor(kTermDefaultFg));
                p.drawRect(cr.adjusted(0, 0, -1, -1));
            }
        }
    }
}

// ── Resize ────────────────────────────────────────────────────────────────────

void TerminalView::resizeEvent(QResizeEvent * /*event*/)
{
    const int cols = qMax(1, viewport()->width()  / m_cellW);
    const int rows = qMax(1, viewport()->height() / m_cellH);
    m_screen->resize(cols, rows);
    emit sizeChanged(cols, rows);
    updateScrollbar();
}

// ── Keyboard ──────────────────────────────────────────────────────────────────

void TerminalView::keyPressEvent(QKeyEvent *event)
{
    const bool ctrl  = event->modifiers() & Qt::ControlModifier;
    const bool shift = event->modifiers() & Qt::ShiftModifier;

    // Ctrl+Shift+C → copy selection
    if (ctrl && shift && event->key() == Qt::Key_C) {
        const QString text = selectedText();
        if (!text.isEmpty())
            QApplication::clipboard()->setText(text);
        return;
    }
    // Ctrl+Shift+V → paste
    if (ctrl && shift && event->key() == Qt::Key_V) {
        const QString text = QApplication::clipboard()->text();
        if (!text.isEmpty())
            emit inputData(text.toUtf8());
        return;
    }
    // Ctrl+Shift+F → find in scrollback
    if (ctrl && shift && event->key() == Qt::Key_F) {
        showFindBar();
        return;
    }
    // Escape → hide find bar if visible
    if (event->key() == Qt::Key_Escape && m_findBar && m_findBar->isVisible()) {
        hideFindBar();
        return;
    }
    // Font size control
    if (ctrl && (event->key() == Qt::Key_Equal || event->key() == Qt::Key_Plus)) {
        setFontSize(m_fontSize + 1);
        return;
    }
    if (ctrl && event->key() == Qt::Key_Minus) {
        setFontSize(m_fontSize - 1);
        return;
    }
    if (ctrl && event->key() == Qt::Key_0) {
        setFontSize(10);
        return;
    }

    // Common control keys
    if (ctrl && !shift) {
        switch (event->key()) {
        case Qt::Key_C:  emit inputData("\x03"); return;  // SIGINT
        case Qt::Key_D:  emit inputData("\x04"); return;  // EOF
        case Qt::Key_Z:  emit inputData("\x1A"); return;  // SIGTSTP
        case Qt::Key_L:  emit inputData("\x0C"); return;  // clear
        case Qt::Key_A:  emit inputData("\x01"); return;
        case Qt::Key_E:  emit inputData("\x05"); return;
        case Qt::Key_U:  emit inputData("\x15"); return;
        case Qt::Key_K:  emit inputData("\x0B"); return;
        case Qt::Key_W:  emit inputData("\x17"); return;
        case Qt::Key_R:  emit inputData("\x12"); return;
        default: break;
        }
    }

    QByteArray seq;

    switch (event->key()) {
    case Qt::Key_Return:    case Qt::Key_Enter:  seq = "\r";      break;
    case Qt::Key_Backspace: seq = ctrl ? "\x08" : "\x7F";        break;
    case Qt::Key_Escape:    seq = "\x1B";                        break;
    case Qt::Key_Tab:       seq = shift ? "\x1B[Z" : "\t";       break;
    case Qt::Key_Up:        seq = "\x1B[A";                      break;
    case Qt::Key_Down:      seq = "\x1B[B";                      break;
    case Qt::Key_Right:     seq = "\x1B[C";                      break;
    case Qt::Key_Left:      seq = "\x1B[D";                      break;
    case Qt::Key_Home:      seq = "\x1B[H";                      break;
    case Qt::Key_End:       seq = "\x1B[F";                      break;
    case Qt::Key_Insert:    seq = "\x1B[2~";                     break;
    case Qt::Key_Delete:    seq = "\x1B[3~";                     break;
    case Qt::Key_PageUp:    seq = "\x1B[5~";                     break;
    case Qt::Key_PageDown:  seq = "\x1B[6~";                     break;
    case Qt::Key_F1:   seq = "\x1BOP";   break;
    case Qt::Key_F2:   seq = "\x1BOQ";   break;
    case Qt::Key_F3:   seq = "\x1BOR";   break;
    case Qt::Key_F4:   seq = "\x1BOS";   break;
    case Qt::Key_F5:   seq = "\x1B[15~"; break;
    case Qt::Key_F6:   seq = "\x1B[17~"; break;
    case Qt::Key_F7:   seq = "\x1B[18~"; break;
    case Qt::Key_F8:   seq = "\x1B[19~"; break;
    case Qt::Key_F9:   seq = "\x1B[20~"; break;
    case Qt::Key_F10:  seq = "\x1B[21~"; break;
    case Qt::Key_F11:  seq = "\x1B[23~"; break;
    case Qt::Key_F12:  seq = "\x1B[24~"; break;
    default:
        if (!event->text().isEmpty())
            seq = event->text().toUtf8();
        break;
    }

    if (!seq.isEmpty()) {
        emit inputData(seq);
        // Reset blink phase and scroll to bottom on any keystroke
        m_curBlink = true;
        m_blinkTimer->start();
        scrollToBottom();
    }
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void TerminalView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const int col    = qBound(0, event->pos().x() / m_cellW, m_screen->cols() - 1);
        const int absRow = event->pos().y() / m_cellH + verticalScrollBar()->value();
        m_selStart  = {col, absRow};
        m_selEnd    = m_selStart;
        m_selecting = true;
        viewport()->update();
    } else if (event->button() == Qt::RightButton) {
        // PuTTY-style: right-click → paste
        const QString text = QApplication::clipboard()->text();
        if (!text.isEmpty())
            emit inputData(text.toUtf8());
    }
    setFocus();
}

void TerminalView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting) {
        const int col    = qBound(0, event->pos().x() / m_cellW, m_screen->cols() - 1);
        const int absRow = event->pos().y() / m_cellH + verticalScrollBar()->value();
        m_selEnd = {col, absRow};
        viewport()->update();
    }
}

void TerminalView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        // PuTTY-style: auto-copy selection to clipboard on release
        const QString text = selectedText();
        if (!text.isEmpty())
            QApplication::clipboard()->setText(text);
    }
}

void TerminalView::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y() / 40;
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta);
    event->accept();
}

// ── Focus ─────────────────────────────────────────────────────────────────────

void TerminalView::focusInEvent(QFocusEvent * /*event*/)
{
    m_hasFocus = true;
    m_curBlink = true;
    m_blinkTimer->start();
    viewport()->update();
}

void TerminalView::focusOutEvent(QFocusEvent * /*event*/)
{
    m_hasFocus = false;
    m_blinkTimer->stop();
    m_curBlink = false;
    viewport()->update();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QPoint TerminalView::pixelToCell(const QPoint &pos) const
{
    return {pos.x() / m_cellW, pos.y() / m_cellH};
}

bool TerminalView::isCellSelected(int col, int absRow) const
{
    if (m_selStart == m_selEnd) return false;
    QPoint s = m_selStart, e = m_selEnd;
    if (s.y() > e.y() || (s.y() == e.y() && s.x() > e.x())) {
        s = m_selEnd;
        e = m_selStart;
    }
    if (absRow < s.y() || absRow > e.y()) return false;
    if (absRow == s.y() && absRow == e.y())
        return col >= s.x() && col <= e.x();
    if (absRow == s.y()) return col >= s.x();
    if (absRow == e.y()) return col <= e.x();
    return true;
}

QString TerminalView::selectedText() const
{
    if (m_selStart == m_selEnd) return {};
    QPoint s = m_selStart, e = m_selEnd;
    if (s.y() > e.y() || (s.y() == e.y() && s.x() > e.x())) {
        s = m_selEnd;
        e = m_selStart;
    }

    const int sbLines = m_screen->scrollbackLines();
    const int cols    = m_screen->cols();
    QString result;

    for (int absRow = s.y(); absRow <= e.y(); ++absRow) {
        const int screenRow = absRow - sbLines;
        const int cStart = (absRow == s.y()) ? s.x() : 0;
        const int cEnd   = (absRow == e.y()) ? e.x() : cols - 1;

        QString line;
        for (int c = cStart; c <= cEnd; ++c) {
            const TermCell cell = m_screen->cellAt(c, screenRow);
            if (cell.cp > U' ')
                line += QString::fromUcs4(&cell.cp, 1);
            else
                line += QLatin1Char(' ');
        }
        // Trim trailing spaces
        while (line.endsWith(QLatin1Char(' ')))
            line.chop(1);
        if (absRow > s.y()) result += QLatin1Char('\n');
        result += line;
    }
    return result;
}

// ── Context Menu ──────────────────────────────────────────────────────────────

void TerminalView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    auto *copyAction = menu.addAction(tr("Copy\tCtrl+Shift+C"));
    copyAction->setEnabled(!selectedText().isEmpty());
    connect(copyAction, &QAction::triggered, this, [this]() {
        const QString text = selectedText();
        if (!text.isEmpty())
            QApplication::clipboard()->setText(text);
    });

    auto *pasteAction = menu.addAction(tr("Paste\tCtrl+Shift+V"));
    pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());
    connect(pasteAction, &QAction::triggered, this, [this]() {
        const QString text = QApplication::clipboard()->text();
        if (!text.isEmpty())
            emit inputData(text.toUtf8());
    });

    menu.addSeparator();

    auto *selectAllAction = menu.addAction(tr("Select All"));
    connect(selectAllAction, &QAction::triggered, this, [this]() {
        const int sbLines = m_screen->scrollbackLines();
        m_selStart = {0, 0};
        m_selEnd = {m_screen->cols() - 1, sbLines + m_screen->rows() - 1};
        viewport()->update();
    });

    auto *clearAction = menu.addAction(tr("Clear"));
    connect(clearAction, &QAction::triggered, this, [this]() {
        m_screen->feed("\x1B[2J\x1B[H");
        viewport()->update();
    });

    menu.addSeparator();

    auto *findAction = menu.addAction(tr("Find...\tCtrl+Shift+F"));
    connect(findAction, &QAction::triggered, this, &TerminalView::showFindBar);

    menu.addSeparator();

    auto *zoomInAction = menu.addAction(tr("Zoom In\tCtrl++"));
    connect(zoomInAction, &QAction::triggered, this, [this]() {
        setFontSize(m_fontSize + 1);
    });

    auto *zoomOutAction = menu.addAction(tr("Zoom Out\tCtrl+-"));
    connect(zoomOutAction, &QAction::triggered, this, [this]() {
        setFontSize(m_fontSize - 1);
    });

    auto *resetZoomAction = menu.addAction(tr("Reset Zoom\tCtrl+0"));
    connect(resetZoomAction, &QAction::triggered, this, [this]() {
        setFontSize(10);
    });

    menu.exec(event->globalPos());
}

// ── Font Size ─────────────────────────────────────────────────────────────────

void TerminalView::setFontSize(int pointSize)
{
    if (pointSize < 6 || pointSize > 48)
        return;
    m_fontSize = pointSize;
    m_font.setPointSize(m_fontSize);
    m_fontBold.setPointSize(m_fontSize);
    QFontMetrics fm(m_font);
    m_cellW = fm.horizontalAdvance(QLatin1Char('M'));
    m_cellH = fm.lineSpacing();
    // Re-grid the screen
    const int cols = qMax(1, viewport()->width()  / m_cellW);
    const int rows = qMax(1, viewport()->height() / m_cellH);
    m_screen->resize(cols, rows);
    emit sizeChanged(cols, rows);
    updateScrollbar();
    viewport()->update();
}

// ── Find Bar ──────────────────────────────────────────────────────────────────

void TerminalView::showFindBar()
{
    if (!m_findBar) {
        m_findBar = new QWidget(this);
        auto *lay = new QHBoxLayout(m_findBar);
        lay->setContentsMargins(6, 4, 6, 4);
        lay->setSpacing(4);

        m_findInput = new QLineEdit(m_findBar);
        m_findInput->setPlaceholderText(tr("Find in terminal..."));
        m_findInput->setStyleSheet(
            QStringLiteral("QLineEdit { background:#313131; color:#cccccc;"
                          " border:1px solid #3e3e42; border-radius:3px;"
                          " padding:2px 6px; font-size:12px; }"));

        m_findCount = new QLabel(m_findBar);
        m_findCount->setStyleSheet(
            QStringLiteral("color:#9d9d9d; font-size:11px;"));

        auto *prevBtn = new QToolButton(m_findBar);
        prevBtn->setText(QStringLiteral("\u25B2"));
        prevBtn->setToolTip(tr("Previous match"));
        prevBtn->setAutoRaise(true);

        auto *nextBtn = new QToolButton(m_findBar);
        nextBtn->setText(QStringLiteral("\u25BC"));
        nextBtn->setToolTip(tr("Next match"));
        nextBtn->setAutoRaise(true);

        auto *closeBtn = new QToolButton(m_findBar);
        closeBtn->setText(QStringLiteral("\u2715"));
        closeBtn->setToolTip(tr("Close"));
        closeBtn->setAutoRaise(true);

        lay->addWidget(m_findInput, 1);
        lay->addWidget(m_findCount);
        lay->addWidget(prevBtn);
        lay->addWidget(nextBtn);
        lay->addWidget(closeBtn);

        m_findBar->setStyleSheet(
            QStringLiteral("background:#1f1f1f; border-bottom:1px solid #3e3e42;"));

        connect(m_findInput, &QLineEdit::textChanged,
                this, &TerminalView::updateSearchHighlight);
        connect(m_findInput, &QLineEdit::returnPressed,
                this, &TerminalView::findNext);
        connect(nextBtn, &QToolButton::clicked, this, &TerminalView::findNext);
        connect(prevBtn, &QToolButton::clicked, this, &TerminalView::findPrev);
        connect(closeBtn, &QToolButton::clicked, this, &TerminalView::hideFindBar);
    }

    // Position find bar at the top of the viewport
    m_findBar->setGeometry(0, 0, width(), 32);
    m_findBar->show();
    m_findBar->raise();
    m_findInput->setFocus();
    m_findInput->selectAll();
}

void TerminalView::hideFindBar()
{
    if (m_findBar) {
        m_findBar->hide();
        m_searchNeedle.clear();
        m_searchHits.clear();
        m_searchCurrent = -1;
        viewport()->update();
        setFocus();
    }
}

void TerminalView::updateSearchHighlight()
{
    m_searchNeedle = m_findInput ? m_findInput->text() : QString();
    m_searchHits.clear();
    m_searchCurrent = -1;

    if (m_searchNeedle.isEmpty()) {
        if (m_findCount) m_findCount->clear();
        viewport()->update();
        return;
    }

    const int sbLines = m_screen->scrollbackLines();
    const int totalRows = sbLines + m_screen->rows();
    const int cols = m_screen->cols();

    for (int absRow = 0; absRow < totalRows; ++absRow) {
        const int screenRow = absRow - sbLines;
        // Build the text of this row
        QString line;
        for (int c = 0; c < cols; ++c) {
            const TermCell cell = m_screen->cellAt(c, screenRow);
            line += (cell.cp > U' ') ? QString::fromUcs4(&cell.cp, 1)
                                     : QStringLiteral(" ");
        }
        // Search for all occurrences in this row
        int pos = 0;
        while ((pos = line.indexOf(m_searchNeedle, pos, Qt::CaseInsensitive)) >= 0) {
            m_searchHits.append({absRow, pos});
            ++pos;
        }
    }

    if (m_findCount) {
        if (m_searchHits.isEmpty())
            m_findCount->setText(tr("No matches"));
        else
            m_findCount->setText(tr("%1 matches").arg(m_searchHits.size()));
    }

    // Jump to first match closest to current scroll position
    if (!m_searchHits.isEmpty()) {
        const int scrolled = verticalScrollBar()->value();
        m_searchCurrent = 0;
        for (int i = 0; i < m_searchHits.size(); ++i) {
            if (m_searchHits[i].first >= scrolled) {
                m_searchCurrent = i;
                break;
            }
        }
        // Scroll to the match
        verticalScrollBar()->setValue(m_searchHits[m_searchCurrent].first);
    }

    viewport()->update();
}

void TerminalView::findNext()
{
    if (m_searchHits.isEmpty()) return;
    m_searchCurrent = (m_searchCurrent + 1) % m_searchHits.size();
    verticalScrollBar()->setValue(m_searchHits[m_searchCurrent].first);
    if (m_findCount) {
        m_findCount->setText(tr("%1 of %2")
            .arg(m_searchCurrent + 1)
            .arg(m_searchHits.size()));
    }
    viewport()->update();
}

void TerminalView::findPrev()
{
    if (m_searchHits.isEmpty()) return;
    m_searchCurrent = (m_searchCurrent - 1 + m_searchHits.size()) % m_searchHits.size();
    verticalScrollBar()->setValue(m_searchHits[m_searchCurrent].first);
    if (m_findCount) {
        m_findCount->setText(tr("%1 of %2")
            .arg(m_searchCurrent + 1)
            .arg(m_searchHits.size()));
    }
    viewport()->update();
}

int TerminalView::lineMatchCount(int absRow, const QString &needle) const
{
    Q_UNUSED(absRow)
    Q_UNUSED(needle)
    return 0; // Unused — kept for potential future use
}

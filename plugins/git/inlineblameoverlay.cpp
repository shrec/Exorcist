#include "inlineblameoverlay.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDateTime>
#include <QDateTime>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolTip>
#include <QVBoxLayout>

namespace {
// Dim gray per task spec.
constexpr const char *kAnnotationColor = "#6e7681";
// Trailing margin between source-line end and annotation start (in px).
constexpr int kLeadingPadPx = 16;
constexpr int kSubjectMaxChars = 60;
} // namespace

InlineBlameOverlay::InlineBlameOverlay(QPlainTextEdit *editor,
                                       GitService *gitService,
                                       QWidget *parent)
    : QWidget(parent ? parent : (editor ? editor->viewport() : nullptr)),
      m_editor(editor),
      m_git(gitService)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(false);

    if (m_editor) {
        // Match viewport geometry; reposition on resize/scroll.
        repositionToViewport();
        m_editor->viewport()->installEventFilter(this);
        m_editor->installEventFilter(this);
        connect(m_editor, &QPlainTextEdit::cursorPositionChanged,
                this, &InlineBlameOverlay::onCursorOrViewportChanged);
        connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &InlineBlameOverlay::onCursorOrViewportChanged);
        connect(m_editor->horizontalScrollBar(), &QScrollBar::valueChanged,
                this, &InlineBlameOverlay::onCursorOrViewportChanged);
        connect(m_editor->document(), &QTextDocument::contentsChange,
                this, [this](int, int, int) {
                    // File contents changed → invalidate per-line cache for
                    // this file (lines shift). We just re-request lazily.
                    if (m_git && !m_filePath.isEmpty())
                        m_git->invalidateBlameCache(m_filePath);
                    onCursorOrViewportChanged();
                });
    }

    if (m_git) {
        // SIGNAL/SLOT string-based connect — safe across DLLs.
        connect(m_git, SIGNAL(blameLineReady(QString,int,GitService::BlameLineInfo)),
                this, SLOT(onBlameLineReady(QString,int,GitService::BlameLineInfo)));
    }
}

void InlineBlameOverlay::setFilePath(const QString &path)
{
    if (m_filePath == path) return;
    m_filePath = path;
    m_lastRequestedLine = -1;
    m_lastShownInfo = {};
    update();
    onCursorOrViewportChanged();
}

void InlineBlameOverlay::setEnabled(bool on)
{
    if (m_enabled == on) return;
    m_enabled = on;
    setVisible(on);
    if (on) onCursorOrViewportChanged();
    update();
}

void InlineBlameOverlay::setMode(Mode m)
{
    if (m_mode == m) return;
    m_mode = m;
    update();
}

void InlineBlameOverlay::repositionToViewport()
{
    if (!m_editor) return;
    QWidget *vp = m_editor->viewport();
    if (parentWidget() != vp)
        setParent(vp);
    setGeometry(vp->rect());
    raise();
}

bool InlineBlameOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_editor)
        return QWidget::eventFilter(watched, event);
    QWidget *vp = m_editor->viewport();

    if (watched == vp) {
        switch (event->type()) {
        case QEvent::Resize:
            repositionToViewport();
            update();
            break;
        case QEvent::Paint:
            // After viewport painted, schedule overlay update.
            update();
            break;
        case QEvent::MouseButtonRelease: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton
                && m_lastShownInfo.valid
                && !m_lastShownInfo.uncommitted
                && m_lastAnnotationRect.contains(me->pos())) {
                showCommitPopover(m_lastShownInfo,
                                  vp->mapToGlobal(me->pos()));
                me->accept();
                return true;
            }
            break;
        }
        case QEvent::ToolTip: {
            auto *he = static_cast<QHelpEvent *>(event);
            if (m_lastShownInfo.valid
                && m_lastAnnotationRect.contains(he->pos())) {
                const QString tip =
                    QStringLiteral("<b>%1</b><br/>%2 &nbsp;<span style='color:#888'>%3</span><br/>%4")
                        .arg(m_lastShownInfo.summary.toHtmlEscaped(),
                             m_lastShownInfo.author.toHtmlEscaped(),
                             m_lastShownInfo.commitHashShort,
                             QDateTime::fromSecsSinceEpoch(m_lastShownInfo.authorTime)
                                 .toString(Qt::ISODate));
                QToolTip::showText(he->globalPos(), tip, vp);
                return true;
            }
            QToolTip::hideText();
            break;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void InlineBlameOverlay::onCursorOrViewportChanged()
{
    if (!m_enabled) return;
    repositionToViewport();
    requestBlameForCurrentLine();
    update();
}

void InlineBlameOverlay::requestBlameForCurrentLine()
{
    if (!m_editor || !m_git || m_filePath.isEmpty()) return;
    const int line = m_editor->textCursor().blockNumber() + 1; // 1-based
    if (line == m_lastRequestedLine) return;
    m_lastRequestedLine = line;

    // Try the cache first to avoid spinning up a process when we already know.
    const auto cached = m_git->cachedBlameLine(m_filePath, line);
    if (cached.valid) {
        m_lastShownInfo = cached;
        update();
        return;
    }
    // Async; will arrive via blameLineReady.
    m_git->blameLineAsync(m_filePath, line);
}

void InlineBlameOverlay::onBlameLineReady(const QString &filePath, int line,
                                          const GitService::BlameLineInfo &info)
{
    if (filePath != m_filePath) return;
    if (!m_editor) return;
    const int currentLine = m_editor->textCursor().blockNumber() + 1;
    if (line != currentLine) return;
    m_lastShownInfo = info;
    update();
}

QString InlineBlameOverlay::relativeTime(qint64 unixSeconds)
{
    if (unixSeconds <= 0) return {};
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 diff = now - unixSeconds;
    if (diff < 60)            return QObject::tr("just now");
    if (diff < 3600)          return QObject::tr("%1 min ago").arg(diff / 60);
    if (diff < 86400)         return QObject::tr("%1 hr ago").arg(diff / 3600);
    if (diff < 86400 * 7)     return QObject::tr("%1 days ago").arg(diff / 86400);
    if (diff < 86400 * 30)    return QObject::tr("%1 wk ago").arg(diff / (86400 * 7));
    if (diff < 86400 * 365)   return QObject::tr("%1 mo ago").arg(diff / (86400 * 30));
    return QObject::tr("%1 yr ago").arg(diff / (86400 * 365));
}

QString InlineBlameOverlay::formatAnnotation(const GitService::BlameLineInfo &info) const
{
    if (!info.valid) return {};
    if (info.uncommitted) return {}; // empty state per spec

    QString subject = info.summary;
    if (subject.size() > kSubjectMaxChars)
        subject = subject.left(kSubjectMaxChars - 1) + QChar(0x2026); // ellipsis
    const QString rel = relativeTime(info.authorTime);
    const QString author = info.author.isEmpty()
                               ? QStringLiteral("?")
                               : info.author;
    return QStringLiteral("  %1, %2  %3 %4")
        .arg(author,
             rel.isEmpty() ? QStringLiteral("recently") : rel,
             QString(QChar(0x00B7)),  // middle dot
             subject);
}

QRect InlineBlameOverlay::annotationRectForLine(int line, const QString &text) const
{
    if (!m_editor || text.isEmpty()) return {};
    QTextDocument *doc = m_editor->document();
    if (!doc) return {};
    QTextBlock block = doc->findBlockByNumber(line - 1);
    if (!block.isValid()) return {};

    // Use the public cursorRect() API (relative to viewport) to find the
    // top of the line. For current-line we use the actual cursor; for
    // arbitrary lines we synthesize a cursor at the start of the block.
    QTextCursor c(block);
    const QRect lineRect = m_editor->cursorRect(c);
    if (!lineRect.isValid()) return {};

    QFont annFont = m_editor->font();
    int pt = annFont.pointSize();
    if (pt > 0) annFont.setPointSize(qMax(6, pt - 1)); // ~10% smaller
    annFont.setItalic(true);
    QFontMetrics fm(annFont);

    // Anchor X: place after the end of the visible line text.
    QTextCursor end(block);
    end.movePosition(QTextCursor::EndOfBlock);
    const QRect endRect = m_editor->cursorRect(end);
    const int x = (endRect.isValid() ? endRect.right() : lineRect.right())
                   + kLeadingPadPx;
    const int y = lineRect.top();
    const int h = lineRect.height();
    const int textW = fm.horizontalAdvance(text);
    return QRect(x, y, textW, h);
}

void InlineBlameOverlay::paintEvent(QPaintEvent *)
{
    if (!m_enabled || !m_editor) return;
    if (m_filePath.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    QFont annFont = m_editor->font();
    int pt = annFont.pointSize();
    if (pt > 0) annFont.setPointSize(qMax(6, pt - 1));
    annFont.setItalic(true);
    p.setFont(annFont);
    p.setPen(QColor(kAnnotationColor));

    auto drawForLine = [&](int line) -> QRect {
        const auto info = m_git ? m_git->cachedBlameLine(m_filePath, line)
                                : GitService::BlameLineInfo{};
        const QString text = formatAnnotation(info);
        if (text.isEmpty()) return {};
        const QRect r = annotationRectForLine(line, text);
        if (r.isEmpty()) return {};
        // Clip to viewport bounds (don't bleed outside).
        if (!rect().intersects(r)) return {};
        p.drawText(r, Qt::AlignVCenter | Qt::TextSingleLine, text);
        return r;
    };

    if (m_mode == CurrentLineOnly) {
        const int line = m_editor->textCursor().blockNumber() + 1;
        // Use the cached info if available, otherwise the most recent fetch.
        QRect r = drawForLine(line);
        if (r.isEmpty() && m_lastShownInfo.valid && m_lastShownInfo.line == line) {
            const QString text = formatAnnotation(m_lastShownInfo);
            r = annotationRectForLine(line, text);
            if (!r.isEmpty() && rect().intersects(r))
                p.drawText(r, Qt::AlignVCenter | Qt::TextSingleLine, text);
        }
        m_lastAnnotationRect = r;
    } else {
        // All visible lines mode — iterate over blocks whose cursorRect()
        // falls within the viewport. We use only public QPlainTextEdit API.
        QTextDocument *doc = m_editor->document();
        if (!doc) return;
        const int viewportTop    = m_editor->viewport()->rect().top();
        const int viewportBottom = m_editor->viewport()->rect().bottom();
        QTextBlock block = doc->firstBlock();
        while (block.isValid()) {
            QTextCursor c(block);
            const QRect r = m_editor->cursorRect(c);
            if (r.isValid()) {
                if (r.top() > viewportBottom) break;
                if (r.bottom() >= viewportTop)
                    drawForLine(block.blockNumber() + 1);
            }
            block = block.next();
        }
    }
}

void InlineBlameOverlay::showCommitPopover(const GitService::BlameLineInfo &info,
                                           const QPoint &globalPos)
{
    if (!info.valid || info.uncommitted) return;
    auto *popup = new QLabel(nullptr,
                             Qt::ToolTip | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose, true);
    popup->setTextFormat(Qt::RichText);
    popup->setStyleSheet(
        QStringLiteral("QLabel { background:#252526; color:#d4d4d4; "
                       "border:1px solid #3e3e42; padding:8px; }"));
    popup->setText(QStringLiteral(
        "<div style='font-family:\"Cascadia Mono\",Consolas,monospace;'>"
        "<b style='color:#e2c08d;'>%1</b><br/>"
        "<span style='color:#9cdcfe;'>%2</span> "
        "&nbsp;<span style='color:#888;'>%3</span><br/><br/>"
        "%4"
        "</div>")
        .arg(info.commitHashShort,
             info.author.toHtmlEscaped(),
             QDateTime::fromSecsSinceEpoch(info.authorTime).toString(Qt::ISODate),
             info.summary.toHtmlEscaped()));
    popup->adjustSize();
    popup->move(globalPos + QPoint(8, 8));
    popup->show();
}

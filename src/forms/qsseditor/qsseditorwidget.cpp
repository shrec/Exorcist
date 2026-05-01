#include "qsseditorwidget.h"

#include "qsspreviewpane.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>
#include <QShortcut>
#include <QSplitter>
#include <QTextBlock>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

namespace exo::forms {

// ─────────────────────────────────────────────────────────────────────────────
// QssSyntaxHighlighter
// ─────────────────────────────────────────────────────────────────────────────

QssSyntaxHighlighter::QssSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // VS-Code-ish dark palette per task spec.
    m_selectorFormat.setForeground(QColor(0x56, 0x9c, 0xd6));   // blue
    m_selectorFormat.setFontWeight(QFont::DemiBold);

    m_propertyFormat.setForeground(QColor(0xc5, 0x86, 0xc0));   // purple

    m_colorFormat.setForeground(QColor(0xb5, 0xce, 0xa8));      // green

    m_stringFormat.setForeground(QColor(0xce, 0x91, 0x78));     // orange

    m_numberFormat.setForeground(QColor(0xdc, 0xdc, 0xaa));     // light yellow

    m_commentFormat.setForeground(QColor(0x6a, 0x99, 0x55));    // gray-green
    m_commentFormat.setFontItalic(true);

    m_pseudoStateFormat.setForeground(QColor(0x4e, 0xc9, 0xb0)); // teal — pseudo-states / sub-controls

    // Rule order matters: more specific first, then general patterns. The
    // multi-line comment pass overrides everything at the end via block-state.

    // 1) Selectors at start of a rule — match identifier(s) at start of line
    //    optionally followed by #id/.class and pseudo-states, up to the '{'.
    //    This is intentionally line-anchored: QSS selectors live before '{'.
    {
        Rule r;
        r.pattern = QRegularExpression(
            QStringLiteral("^[A-Za-z_][A-Za-z0-9_:#.\\-\\s,>*\\[\\]=\"']*(?=\\{)"),
            QRegularExpression::MultilineOption);
        r.format  = m_selectorFormat;
        m_rules.append(r);
    }
    // Selector continuation (commas) — when a selector group spans multiple
    // lines and the line ends with ',' (no '{' on this line).
    {
        Rule r;
        r.pattern = QRegularExpression(
            QStringLiteral("^[A-Za-z_][A-Za-z0-9_:#.\\-\\s,>*\\[\\]=\"']*,\\s*$"),
            QRegularExpression::MultilineOption);
        r.format  = m_selectorFormat;
        m_rules.append(r);
    }

    // 2) Pseudo-states / sub-controls (e.g. :hover, :pressed, ::chunk).
    //    Highlighted on top of selectors with a distinct color so they pop.
    {
        Rule r;
        r.pattern = QRegularExpression(
            QStringLiteral(":{1,2}[A-Za-z\\-]+(?=[\\s,{>:]|$)"));
        r.format  = m_pseudoStateFormat;
        m_rules.append(r);
    }

    // 3) Properties — identifier at start of an indented declaration line,
    //    immediately followed (after optional space) by ':'.
    {
        Rule r;
        r.pattern = QRegularExpression(
            QStringLiteral("\\b([A-Za-z\\-]+)(?=\\s*:[^:])"));
        r.format  = m_propertyFormat;
        r.captureGroup = 1;
        m_rules.append(r);
    }

    // 4) Color values
    //    a) #hex (3, 4, 6, or 8 hex digits)
    {
        Rule r;
        r.pattern = QRegularExpression(
            QStringLiteral("#[0-9A-Fa-f]{3,8}\\b"));
        r.format  = m_colorFormat;
        m_rules.append(r);
    }
    //    b) rgb(...) / rgba(...) / hsl(...) / hsla(...)
    {
        Rule r;
        r.pattern = QRegularExpression(
            QStringLiteral("\\b(?:rgb|rgba|hsl|hsla)\\s*\\([^)]*\\)"));
        r.format  = m_colorFormat;
        m_rules.append(r);
    }
    //    c) Named colors (a small but useful set — Qt accepts SVG color names).
    {
        static const QStringList kNamed = {
            QStringLiteral("black"),  QStringLiteral("white"),  QStringLiteral("red"),
            QStringLiteral("green"),  QStringLiteral("blue"),   QStringLiteral("yellow"),
            QStringLiteral("cyan"),   QStringLiteral("magenta"),QStringLiteral("gray"),
            QStringLiteral("grey"),   QStringLiteral("orange"), QStringLiteral("purple"),
            QStringLiteral("brown"),  QStringLiteral("pink"),   QStringLiteral("transparent"),
        };
        QStringList alt;
        for (const QString &n : kNamed)
            alt << QStringLiteral("\\b%1\\b").arg(n);
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("(?:%1)").arg(alt.join(QLatin1Char('|'))),
                                       QRegularExpression::CaseInsensitiveOption);
        r.format  = m_colorFormat;
        m_rules.append(r);
    }

    // 5) Numbers + units (px, em, pt, %, ex)
    {
        Rule r;
        r.pattern = QRegularExpression(
            QStringLiteral("\\b\\d+(?:\\.\\d+)?(?:px|em|pt|ex|%)?\\b"));
        r.format  = m_numberFormat;
        m_rules.append(r);
    }

    // 6) Strings (double + single quoted)
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\"([^\"\\\\]|\\\\.)*\""));
        r.format  = m_stringFormat;
        m_rules.append(r);
    }
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("'([^'\\\\]|\\\\.)*'"));
        r.format  = m_stringFormat;
        m_rules.append(r);
    }

    // Multi-line /* … */ comments handled by block-state machinery below.
    m_blockStart = QRegularExpression(QStringLiteral("/\\*"));
    m_blockEnd   = QRegularExpression(QStringLiteral("\\*/"));
}

void QssSyntaxHighlighter::highlightBlock(const QString &text)
{
    // 1) Apply rules in declared order.
    for (const Rule &r : std::as_const(m_rules)) {
        auto it = r.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            if (r.captureGroup > 0 && m.lastCapturedIndex() >= r.captureGroup)
                setFormat(m.capturedStart(r.captureGroup),
                          m.capturedLength(r.captureGroup),
                          r.format);
            else
                setFormat(m.capturedStart(), m.capturedLength(), r.format);
        }
    }

    // 2) Multi-line /* … */ via block state — applied last so it overrides
    //    any rule-coloring that happened to match inside a comment span.
    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(m_blockStart);

    while (startIndex >= 0) {
        const auto endMatch = m_blockEnd.match(text, startIndex);
        const int endIndex = endMatch.hasMatch() ? endMatch.capturedStart() : -1;
        int commentLen = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLen = text.length() - startIndex;
        } else {
            commentLen = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, commentLen, m_commentFormat);
        startIndex = text.indexOf(m_blockStart, startIndex + commentLen);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// LineNumberArea — gutter for the QSS editor
// ─────────────────────────────────────────────────────────────────────────────

namespace {

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(QssPlainTextEdit *editor)
        : QWidget(editor), m_editor(editor) {}

    QSize sizeHint() const override { return QSize(computeWidth(), 0); }

    int computeWidth() const {
        int digits = 1;
        int max = qMax(1, m_editor->blockCount());
        while (max >= 10) { max /= 10; ++digits; }
        const int padding = 12;
        return padding + m_editor->fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QssPlainTextEdit *m_editor;
};

} // namespace

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(0x1e, 0x1e, 0x1e));

    QTextBlock block = m_editor->firstVisibleBlock();
    int blockNumber = block.blockNumber();
    qreal top = m_editor->blockBoundingGeometry(block)
                    .translated(m_editor->contentOffset()).top();
    qreal bottom = top + m_editor->blockBoundingRect(block).height();

    const QColor fg(0x85, 0x85, 0x85);
    painter.setPen(fg);
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const QString number = QString::number(blockNumber + 1);
            painter.drawText(0, int(top), width() - 6,
                             m_editor->fontMetrics().height(),
                             Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + m_editor->blockBoundingRect(block).height();
        ++blockNumber;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// QssEditorWidget
// ─────────────────────────────────────────────────────────────────────────────

QssEditorWidget::QssEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    wireShortcuts();
    applyDarkPalette();
    updateStatus(tr("QSS editor ready"), 2000);
}

QssEditorWidget::~QssEditorWidget() = default;

void QssEditorWidget::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ────────────────────────────────────────────────────────────
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet(QStringLiteral(
        "QToolBar { background: #2d2d30; border: 0; spacing: 4px; padding: 4px; }"
        "QToolButton { color: #d4d4d4; background: transparent; padding: 4px 8px; border-radius: 2px; }"
        "QToolButton:hover { background: #3e3e42; }"
        "QToolButton:pressed { background: #094771; }"));

    auto *reloadAct = m_toolbar->addAction(tr("Reload Preview"));
    reloadAct->setShortcuts({QKeySequence(Qt::Key_F5),
                             QKeySequence(QStringLiteral("Ctrl+Shift+R"))});
    reloadAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    reloadAct->setToolTip(tr("Reload Preview (F5 / Ctrl+Shift+R)"));
    connect(reloadAct, &QAction::triggered, this, &QssEditorWidget::reloadPreviewNow);
    addAction(reloadAct);

    auto *toggleAct = m_toolbar->addAction(tr("Toggle Preview"));
    toggleAct->setShortcut(QKeySequence(Qt::Key_F12));
    toggleAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    toggleAct->setToolTip(tr("Toggle Preview Pane (F12)"));
    connect(toggleAct, &QAction::triggered, this, &QssEditorWidget::togglePreviewVisible);
    addAction(toggleAct);

    auto *copyAct = m_toolbar->addAction(tr("Copy QSS"));
    copyAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    copyAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    copyAct->setToolTip(tr("Copy current QSS to clipboard (Ctrl+Shift+C)"));
    connect(copyAct, &QAction::triggered, this, &QssEditorWidget::copyQssToClipboard);
    addAction(copyAct);

    auto *saveAct = m_toolbar->addAction(tr("Save"));
    saveAct->setShortcut(QKeySequence::Save); // Ctrl+S
    saveAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    saveAct->setToolTip(tr("Save (Ctrl+S)"));
    connect(saveAct, &QAction::triggered, this, [this]() {
        if (save())
            updateStatus(tr("Saved %1").arg(QFileInfo(m_filePath).fileName()));
        else
            updateStatus(tr("Save failed"), 4000, /*warning=*/true);
    });
    addAction(saveAct);

    root->addWidget(m_toolbar);

    // ── Splitter: editor | preview ─────────────────────────────────────────
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(4);
    m_splitter->setChildrenCollapsible(false);

    // Editor side: QPlainTextEdit + line-number gutter on the left.
    auto *editorContainer = new QWidget(m_splitter);
    auto *editorLayout = new QHBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    m_editor = new QssPlainTextEdit(editorContainer);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono(QStringLiteral("Consolas"), 11);
    mono.setStyleHint(QFont::Monospace);
    m_editor->setFont(mono);
    m_editor->setTabStopDistance(QFontMetricsF(mono).horizontalAdvance(QLatin1Char(' ')) * 4);

    auto *gutter = new LineNumberArea(m_editor);
    editorLayout->addWidget(gutter);
    editorLayout->addWidget(m_editor, 1);

    // Sync gutter width on edits / scroll.
    auto syncGutter = [gutter, this]() {
        gutter->setFixedWidth(gutter->computeWidth());
        gutter->update();
    };
    connect(m_editor, &QPlainTextEdit::blockCountChanged, this, syncGutter);
    connect(m_editor, &QPlainTextEdit::updateRequest,    this, syncGutter);
    connect(m_editor->document(), &QTextDocument::contentsChanged, this, syncGutter);
    syncGutter();

    m_splitter->addWidget(editorContainer);

    // Preview pane (right).
    m_preview = new QssPreviewPane(m_splitter);
    m_splitter->addWidget(m_preview);

    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({400, 400});

    root->addWidget(m_splitter, 1);

    // ── Status label (parse warnings + char count) ─────────────────────────
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #2d2d30; color: #cccccc; padding: 4px 8px; "
        "         font-family: 'Segoe UI', sans-serif; font-size: 9pt; }"));
    m_statusLabel->setText(tr("Ready"));
    root->addWidget(m_statusLabel);

    // Highlighter on the editor document.
    m_highlighter = new QssSyntaxHighlighter(m_editor->document());

    // 300 ms debounce timer for live-preview reload (per spec).
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(300);
    connect(m_debounce, &QTimer::timeout, this, &QssEditorWidget::onDebounceTimeout);
    connect(m_editor, &QPlainTextEdit::textChanged, this, &QssEditorWidget::onTextChanged);
}

void QssEditorWidget::wireShortcuts()
{
    // Toolbar QActions own all primary shortcuts (with WidgetWithChildren
    // scope) — no extra QShortcuts needed here. This keeps the keyboard
    // story in one place.
}

void QssEditorWidget::applyDarkPalette()
{
    // VS dark-modern background for the host shell. The preview pane already
    // owns its own QSS scope (#QssSampleHost) so user QSS does not bleed
    // here.
    setStyleSheet(QStringLiteral(
        "QssEditorWidget { background: #1e1e1e; }"
        "QPlainTextEdit { background: #1e1e1e; color: #d4d4d4; "
        "                 selection-background-color: #264f78; "
        "                 selection-color: #ffffff; "
        "                 border: 0; }"
        "QSplitter::handle { background: #2d2d30; }"));
}

bool QssEditorWidget::loadFromFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    m_loadingFile = true;
    const QByteArray bytes = f.readAll();
    f.close();

    m_editor->setPlainText(QString::fromUtf8(bytes));
    m_filePath = path;
    setProperty("filePath", path);
    m_loadingFile = false;
    m_modified = false;
    emit modificationChanged(false);

    // Initial preview render.
    m_preview->applyStyleSheet(m_editor->toPlainText());
    updateInfoStatus();
    return true;
}

bool QssEditorWidget::save()
{
    if (m_filePath.isEmpty()) return false;
    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    f.write(m_editor->toPlainText().toUtf8());
    f.close();
    m_modified = false;
    emit modificationChanged(false);
    updateInfoStatus();
    return true;
}

void QssEditorWidget::onTextChanged()
{
    if (m_loadingFile) return;
    if (!m_modified) {
        m_modified = true;
        emit modificationChanged(true);
    }
    m_debounce->start();
}

void QssEditorWidget::onDebounceTimeout()
{
    m_preview->applyStyleSheet(m_editor->toPlainText());
    updateInfoStatus();
}

void QssEditorWidget::reloadPreviewNow()
{
    m_preview->applyStyleSheet(m_editor->toPlainText());
    const QString warn = detectParseWarning();
    if (warn.isEmpty())
        updateStatus(tr("Applied OK — preview reloaded"));
    else
        updateStatus(warn, 4000, /*warning=*/true);
}

void QssEditorWidget::togglePreviewVisible()
{
    if (!m_preview) return;
    const bool nowVisible = !m_preview->isVisible();
    m_preview->setVisible(nowVisible);
    updateStatus(nowVisible ? tr("Preview shown") : tr("Preview hidden"));
}

void QssEditorWidget::copyQssToClipboard()
{
    const QString qss = m_editor->toPlainText();
    QApplication::clipboard()->setText(qss);
    updateStatus(tr("Copied %1 chars to clipboard").arg(qss.size()));
}

QString QssEditorWidget::detectParseWarning() const
{
    // Lightweight heuristic — Qt's QStyle stylesheet parser does not surface
    // diagnostics, so we do a shallow lexer pass to surface common mistakes.
    const QString src = m_editor->toPlainText();
    int braceDepth = 0;
    int line = 1, col = 0;
    int firstUnclosedLine = -1;
    bool inString = false;
    QChar stringQuote;
    bool inComment = false;
    int commentStartLine = -1;
    for (int i = 0; i < src.size(); ++i) {
        const QChar c = src[i];
        if (c == QLatin1Char('\n')) { ++line; col = 0; continue; }
        ++col;
        if (inComment) {
            if (c == QLatin1Char('*') && i + 1 < src.size()
                && src[i + 1] == QLatin1Char('/')) {
                inComment = false;
                ++i; ++col;
            }
            continue;
        }
        if (inString) {
            if (c == QLatin1Char('\\') && i + 1 < src.size()) { ++i; ++col; continue; }
            if (c == stringQuote) inString = false;
            continue;
        }
        if (c == QLatin1Char('/') && i + 1 < src.size() && src[i + 1] == QLatin1Char('*')) {
            inComment = true;
            commentStartLine = line;
            ++i; ++col;
            continue;
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            inString = true;
            stringQuote = c;
            continue;
        }
        if (c == QLatin1Char('{')) {
            if (braceDepth == 0) firstUnclosedLine = line;
            ++braceDepth;
        } else if (c == QLatin1Char('}')) {
            --braceDepth;
            if (braceDepth < 0)
                return tr("Parse warning at line %1: unmatched '}'").arg(line);
        }
    }
    if (inComment)
        return tr("Parse warning at line %1: unterminated /* comment").arg(commentStartLine);
    if (inString)
        return tr("Parse warning at line %1: unterminated string").arg(line);
    if (braceDepth > 0)
        return tr("Parse warning at line %1: unmatched '{'").arg(firstUnclosedLine);
    return {};
}

void QssEditorWidget::updateStatus(const QString &msg, int timeoutMs, bool warning)
{
    const QString fg = warning ? QStringLiteral("#dcdcaa")  // yellow
                               : QStringLiteral("#73c991"); // green
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #2d2d30; color: %1; padding: 4px 8px; "
        "         font-family: 'Segoe UI', sans-serif; font-size: 9pt; }").arg(fg));
    m_statusLabel->setText(msg);
    emit statusMessage(msg, timeoutMs);
    if (timeoutMs > 0) {
        QPointer<QssEditorWidget> guard(this);
        QTimer::singleShot(timeoutMs, this, [guard]() {
            if (!guard) return;
            guard->updateInfoStatus();
        });
    }
}

void QssEditorWidget::updateInfoStatus()
{
    const int chars = m_editor->toPlainText().size();
    const int lines = m_editor->blockCount();

    const QString warn = detectParseWarning();
    if (!warn.isEmpty()) {
        m_statusLabel->setStyleSheet(QStringLiteral(
            "QLabel { background: #2d2d30; color: #dcdcaa; padding: 4px 8px; "
            "         font-family: 'Segoe UI', sans-serif; font-size: 9pt; }"));
        m_statusLabel->setText(warn + tr("    |    %1 lines, %2 chars")
                                          .arg(lines).arg(chars));
        return;
    }

    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #2d2d30; color: #73c991; padding: 4px 8px; "
        "         font-family: 'Segoe UI', sans-serif; font-size: 9pt; }"));
    QString sizeStr;
    if (!m_filePath.isEmpty()) {
        const qint64 bytes = QFileInfo(m_filePath).size();
        sizeStr = tr("    |    %1 bytes on disk").arg(bytes);
    }
    m_statusLabel->setText(tr("Applied OK    |    %1 lines, %2 chars%3")
                               .arg(lines).arg(chars).arg(sizeStr));
}

} // namespace exo::forms

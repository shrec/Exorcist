#include "qmleditorwidget.h"

#include "qmlpreviewpane.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickView>
#include <QRegularExpression>
#include <QShortcut>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

namespace exo::forms {

// ─────────────────────────────────────────────────────────────────────────────
// QmlSyntaxHighlighter
// ─────────────────────────────────────────────────────────────────────────────

QmlSyntaxHighlighter::QmlSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // VS-Code-ish dark palette.
    m_keywordFormat.setForeground(QColor(0x56, 0x9c, 0xd6)); // blue
    m_keywordFormat.setFontWeight(QFont::DemiBold);

    m_qmlTypeFormat.setForeground(QColor(0x4e, 0xc9, 0xb0)); // teal

    m_stringFormat.setForeground(QColor(0xce, 0x91, 0x78)); // soft orange

    m_numberFormat.setForeground(QColor(0xb5, 0xce, 0xa8)); // pale green

    m_commentFormat.setForeground(QColor(0x6a, 0x99, 0x55)); // green
    m_commentFormat.setFontItalic(true);

    m_propertyFormat.setForeground(QColor(0x9c, 0xdc, 0xfe)); // pale blue

    m_functionFormat.setForeground(QColor(0xdc, 0xdc, 0xaa)); // yellow

    // Keywords (JS + QML reserved words).
    static const QStringList kKeywords = {
        QStringLiteral("import"),    QStringLiteral("property"), QStringLiteral("signal"),
        QStringLiteral("function"),  QStringLiteral("var"),       QStringLiteral("let"),
        QStringLiteral("const"),     QStringLiteral("true"),      QStringLiteral("false"),
        QStringLiteral("null"),      QStringLiteral("undefined"), QStringLiteral("if"),
        QStringLiteral("else"),      QStringLiteral("for"),       QStringLiteral("while"),
        QStringLiteral("do"),        QStringLiteral("return"),    QStringLiteral("break"),
        QStringLiteral("continue"),  QStringLiteral("switch"),    QStringLiteral("case"),
        QStringLiteral("default"),   QStringLiteral("new"),       QStringLiteral("this"),
        QStringLiteral("typeof"),    QStringLiteral("instanceof"),QStringLiteral("in"),
        QStringLiteral("of"),        QStringLiteral("readonly"),  QStringLiteral("required"),
        QStringLiteral("default"),   QStringLiteral("alias"),     QStringLiteral("on"),
        QStringLiteral("as"),        QStringLiteral("enum"),      QStringLiteral("pragma"),
    };
    for (const QString &kw : kKeywords) {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw));
        r.format  = m_keywordFormat;
        m_rules.append(r);
    }

    // QML built-in / well-known types.
    static const QStringList kQmlTypes = {
        QStringLiteral("QtObject"),    QStringLiteral("Item"),         QStringLiteral("Rectangle"),
        QStringLiteral("Text"),        QStringLiteral("Window"),       QStringLiteral("Image"),
        QStringLiteral("MouseArea"),   QStringLiteral("Column"),       QStringLiteral("Row"),
        QStringLiteral("Grid"),        QStringLiteral("Flow"),         QStringLiteral("Repeater"),
        QStringLiteral("ListView"),    QStringLiteral("GridView"),     QStringLiteral("PathView"),
        QStringLiteral("Loader"),      QStringLiteral("Component"),    QStringLiteral("Connections"),
        QStringLiteral("Binding"),     QStringLiteral("Timer"),        QStringLiteral("Animation"),
        QStringLiteral("PropertyAnimation"), QStringLiteral("NumberAnimation"),
        QStringLiteral("Behavior"),    QStringLiteral("State"),        QStringLiteral("Transition"),
        QStringLiteral("ApplicationWindow"), QStringLiteral("Button"), QStringLiteral("Label"),
        QStringLiteral("TextField"),   QStringLiteral("TextArea"),     QStringLiteral("ComboBox"),
        QStringLiteral("CheckBox"),    QStringLiteral("RadioButton"),  QStringLiteral("Slider"),
        QStringLiteral("ScrollView"),  QStringLiteral("StackLayout"),  QStringLiteral("RowLayout"),
        QStringLiteral("ColumnLayout"),QStringLiteral("GridLayout"),   QStringLiteral("Flickable"),
        QStringLiteral("ListModel"),   QStringLiteral("ListElement"),  QStringLiteral("Canvas"),
        QStringLiteral("Shape"),       QStringLiteral("Path"),         QStringLiteral("Qt"),
        QStringLiteral("string"),      QStringLiteral("int"),          QStringLiteral("real"),
        QStringLiteral("double"),      QStringLiteral("bool"),         QStringLiteral("color"),
        QStringLiteral("url"),         QStringLiteral("date"),         QStringLiteral("variant"),
        QStringLiteral("point"),       QStringLiteral("size"),         QStringLiteral("rect"),
        QStringLiteral("vector2d"),    QStringLiteral("vector3d"),     QStringLiteral("vector4d"),
        QStringLiteral("quaternion"),  QStringLiteral("matrix4x4"),    QStringLiteral("font"),
    };
    for (const QString &t : kQmlTypes) {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(t));
        r.format  = m_qmlTypeFormat;
        m_rules.append(r);
    }

    // Numbers: 123, 1.5, 0xff
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\\b(0x[0-9a-fA-F]+|\\d+(\\.\\d+)?)\\b"));
        r.format  = m_numberFormat;
        m_rules.append(r);
    }

    // property declaration: highlight the identifier after `property <type>`.
    {
        Rule r;
        r.pattern = QRegularExpression(
            QStringLiteral("\\bproperty\\s+\\w+\\s+([A-Za-z_]\\w*)"));
        r.format  = m_propertyFormat;
        m_rules.append(r);
    }

    // function name (simple): function foo(...)
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\\bfunction\\s+([A-Za-z_]\\w*)"));
        r.format  = m_functionFormat;
        m_rules.append(r);
    }

    // Double-quoted strings.
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\"([^\"\\\\]|\\\\.)*\""));
        r.format  = m_stringFormat;
        m_rules.append(r);
    }
    // Single-quoted strings.
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("'([^'\\\\]|\\\\.)*'"));
        r.format  = m_stringFormat;
        m_rules.append(r);
    }

    // Single-line comments must be applied LAST so they override prior formats
    // on the same line.
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
        r.format  = m_commentFormat;
        m_rules.append(r);
    }

    // Multi-line comments handled via block-state machinery below.
    m_blockStart = QRegularExpression(QStringLiteral("/\\*"));
    m_blockEnd   = QRegularExpression(QStringLiteral("\\*/"));
}

void QmlSyntaxHighlighter::highlightBlock(const QString &text)
{
    // 1) Apply rules in order.
    for (const Rule &r : std::as_const(m_rules)) {
        auto it = r.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            // For rules with a capture group, prefer that span; fall back to
            // the full match otherwise.
            if (m.lastCapturedIndex() >= 1)
                setFormat(m.capturedStart(1), m.capturedLength(1), r.format);
            else
                setFormat(m.capturedStart(), m.capturedLength(), r.format);
        }
    }

    // 2) Multi-line /* ... */ comments via block state.
    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(m_blockStart);

    while (startIndex >= 0) {
        const auto endMatch = m_blockEnd.match(text, startIndex);
        int endIndex = endMatch.hasMatch() ? endMatch.capturedStart() : -1;
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
// QmlEditorWidget
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// Tiny side panel that paints line numbers next to a QmlPlainTextEdit.
class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(QmlPlainTextEdit *editor)
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
    QmlPlainTextEdit *m_editor;
};

} // namespace

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(0x1e, 0x1e, 0x1e));

    QTextBlock block = m_editor->firstVisibleBlock();
    int blockNumber = block.blockNumber();
    qreal top = m_editor->blockBoundingGeometry(block).translated(m_editor->contentOffset()).top();
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

QmlEditorWidget::QmlEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    wireShortcuts();
    applyDarkPalette();
    updateStatus(tr("QML editor ready"), 2000);
}

QmlEditorWidget::~QmlEditorWidget() = default;

void QmlEditorWidget::buildUi()
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
    reloadAct->setShortcut(QKeySequence(Qt::Key_F5));
    reloadAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    reloadAct->setToolTip(tr("Reload Preview (F5)"));
    connect(reloadAct, &QAction::triggered, this, &QmlEditorWidget::reloadPreviewNow);
    addAction(reloadAct);

    auto *toggleAct = m_toolbar->addAction(tr("Toggle Preview"));
    toggleAct->setShortcut(QKeySequence(Qt::Key_F12));
    toggleAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    toggleAct->setToolTip(tr("Toggle Preview Pane (F12)"));
    connect(toggleAct, &QAction::triggered, this, &QmlEditorWidget::togglePreviewVisible);
    addAction(toggleAct);

    auto *runAct = m_toolbar->addAction(tr("Run in External Window"));
    runAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    runAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    runAct->setToolTip(tr("Run QML in Separate Window (Ctrl+R)"));
    connect(runAct, &QAction::triggered, this, &QmlEditorWidget::runInExternalWindow);
    addAction(runAct);

    root->addWidget(m_toolbar);

    // ── Splitter: editor | preview ────────────────────────────────────────
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(4);
    m_splitter->setChildrenCollapsible(false);

    // Editor side: QPlainTextEdit hosted in a small container with a line
    // number gutter on the left.
    auto *editorContainer = new QWidget(m_splitter);
    auto *editorLayout = new QHBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    m_editor = new QmlPlainTextEdit(editorContainer);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono(QStringLiteral("Consolas"), 11);
    mono.setStyleHint(QFont::Monospace);
    m_editor->setFont(mono);
    m_editor->setTabStopDistance(QFontMetricsF(mono).horizontalAdvance(QLatin1Char(' ')) * 4);

    auto *gutter = new LineNumberArea(m_editor);
    editorLayout->addWidget(gutter);
    editorLayout->addWidget(m_editor, 1);

    // Sync gutter with editor scroll/edits.
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
    m_preview = new QmlPreviewPane(m_splitter);
    m_splitter->addWidget(m_preview);

    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({400, 400});

    root->addWidget(m_splitter, 1);

    // ── Status label (parse errors + file size) ───────────────────────────
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #2d2d30; color: #cccccc; padding: 4px 8px; "
        "         font-family: 'Segoe UI', sans-serif; font-size: 9pt; }"));
    m_statusLabel->setText(tr("Ready"));
    root->addWidget(m_statusLabel);

    // Highlighter.
    m_highlighter = new QmlSyntaxHighlighter(m_editor->document());

    // Debounce timer (500 ms) for live preview reload.
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(500);
    connect(m_debounce, &QTimer::timeout, this, &QmlEditorWidget::onDebounceTimeout);
    connect(m_editor,   &QPlainTextEdit::textChanged, this, &QmlEditorWidget::onTextChanged);

    // Preview signal hookups.
    connect(m_preview, &QmlPreviewPane::errorMessage,    this, &QmlEditorWidget::onPreviewError);
    connect(m_preview, &QmlPreviewPane::previewReloaded, this, &QmlEditorWidget::onPreviewReloaded);
}

void QmlEditorWidget::wireShortcuts()
{
    // Ctrl+S — save. We use a widget-scoped QShortcut so it never fires when
    // focus is outside this editor (avoids stealing global save).
    auto *saveSc = new QShortcut(QKeySequence::Save, this);
    saveSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(saveSc, &QShortcut::activated, this, [this]() {
        if (save())
            updateStatus(tr("Saved %1").arg(QFileInfo(m_filePath).fileName()));
        else
            updateStatus(tr("Save failed"), 4000);
    });
}

void QmlEditorWidget::applyDarkPalette()
{
    setStyleSheet(QStringLiteral(
        "QmlEditorWidget { background: #1e1e1e; }"
        "QPlainTextEdit { background: #1e1e1e; color: #d4d4d4; "
        "                 selection-background-color: #264f78; "
        "                 selection-color: #ffffff; "
        "                 border: 0; }"
        "QSplitter::handle { background: #2d2d30; }"));
}

bool QmlEditorWidget::loadFromFile(const QString &path)
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
    m_preview->setQmlSource(m_editor->toPlainText());
    updateFileSizeStatus();
    return true;
}

bool QmlEditorWidget::save()
{
    if (m_filePath.isEmpty()) return false;
    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    f.write(m_editor->toPlainText().toUtf8());
    f.close();
    m_modified = false;
    emit modificationChanged(false);
    updateFileSizeStatus();
    return true;
}

void QmlEditorWidget::onTextChanged()
{
    if (m_loadingFile) return;
    if (!m_modified) {
        m_modified = true;
        emit modificationChanged(true);
    }
    m_debounce->start();
}

void QmlEditorWidget::onDebounceTimeout()
{
    m_preview->setQmlSource(m_editor->toPlainText());
    updateFileSizeStatus();
}

void QmlEditorWidget::reloadPreviewNow()
{
    m_preview->setQmlSource(m_editor->toPlainText());
    updateStatus(tr("Preview reloaded"));
}

void QmlEditorWidget::togglePreviewVisible()
{
    if (!m_preview) return;
    const bool nowVisible = !m_preview->isVisible();
    m_preview->setVisible(nowVisible);
    updateStatus(nowVisible ? tr("Preview shown") : tr("Preview hidden"));
}

void QmlEditorWidget::runInExternalWindow()
{
    // Spin up a top-level QQuickView pointed at the same temp file the embedded
    // preview uses. We persist the QML to a fresh temp path so the external
    // window owns its own source.
    const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString path = tmpDir + QStringLiteral("/exorcist_qml_external_%1.qml")
                             .arg(reinterpret_cast<quintptr>(this), 0, 16);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        updateStatus(tr("Failed to open temp file for external run"), 4000);
        return;
    }
    f.write(m_editor->toPlainText().toUtf8());
    f.close();

    auto *view = new QQuickView();
    // QQuickView is a QWindow, not a QWidget — auto-delete via the
    // visibilityChanged signal so the window is freed when the user closes it.
    QObject::connect(view, &QQuickView::visibilityChanged, view,
                     [view](QWindow::Visibility v) {
                         if (v == QWindow::Hidden)
                             view->deleteLater();
                     });
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->setColor(QColor(0x1e, 0x1e, 0x1e));
    view->setTitle(tr("QML Preview — %1")
                       .arg(m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName()));
    view->resize(640, 480);
    view->setSource(QUrl::fromLocalFile(path));
    view->show();

    // Surface external window errors back into our status bar.
    QPointer<QmlEditorWidget> guard(this);
    QObject::connect(view, &QQuickView::statusChanged, view,
                     [view, guard](QQuickView::Status status) {
                         if (!guard || status != QQuickView::Error) return;
                         QStringList lines;
                         for (const QQmlError &e : view->errors())
                             lines << e.toString();
                         emit guard->statusMessage(
                             QObject::tr("External preview errors: %1")
                                 .arg(lines.join(QLatin1Char('\n'))),
                             5000);
                     });

    updateStatus(tr("Launched external preview window"));
}

void QmlEditorWidget::onPreviewError(const QString &msg)
{
    // Compress to one line for the status label; full text stays on the
    // preview overlay.
    QString first = msg.section(QLatin1Char('\n'), 0, 0);
    if (first.length() > 200) first = first.left(200) + QStringLiteral("…");
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #2d2d30; color: #ff6b6b; padding: 4px 8px; "
        "         font-family: 'Segoe UI', sans-serif; font-size: 9pt; }"));
    m_statusLabel->setText(tr("QML errors: %1").arg(first));
    emit statusMessage(tr("QML preview error"), 5000);
}

void QmlEditorWidget::onPreviewReloaded()
{
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #2d2d30; color: #cccccc; padding: 4px 8px; "
        "         font-family: 'Segoe UI', sans-serif; font-size: 9pt; }"));
    updateFileSizeStatus();
}

void QmlEditorWidget::updateStatus(const QString &msg, int timeoutMs)
{
    m_statusLabel->setText(msg);
    emit statusMessage(msg, timeoutMs);
    if (timeoutMs > 0) {
        QPointer<QmlEditorWidget> guard(this);
        QTimer::singleShot(timeoutMs, this, [guard]() {
            if (!guard) return;
            guard->updateFileSizeStatus();
        });
    }
}

void QmlEditorWidget::updateFileSizeStatus()
{
    const int chars = m_editor->toPlainText().size();
    const int lines = m_editor->blockCount();
    QString sizeStr;
    if (!m_filePath.isEmpty()) {
        const qint64 bytes = QFileInfo(m_filePath).size();
        sizeStr = tr(" — %1 bytes on disk").arg(bytes);
    }
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #2d2d30; color: #cccccc; padding: 4px 8px; "
        "         font-family: 'Segoe UI', sans-serif; font-size: 9pt; }"));
    m_statusLabel->setText(tr("%1 lines, %2 chars%3")
                               .arg(lines)
                               .arg(chars)
                               .arg(sizeStr));
}

} // namespace exo::forms

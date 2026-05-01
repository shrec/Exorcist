// QmlEditorWidget — split-view QML editor with live preview.
//
// Layout (top → bottom):
//   ┌──────────────────────────────────────────────────────┐
//   │  Toolbar: Reload (F5)  Toggle Preview (F12)  Run     │
//   ├──────────────────────────────────────────────────────┤
//   │  QSplitter (horizontal)                              │
//   │  ┌──── editor (left) ────┬──── preview (right) ────┐ │
//   │  │ QPlainTextEdit + nums │  QmlPreviewPane         │ │
//   │  └───────────────────────┴─────────────────────────┘ │
//   ├──────────────────────────────────────────────────────┤
//   │  Status bar: parse errors / file size                │
//   └──────────────────────────────────────────────────────┘
//
// Edits to the editor trigger a debounced (500 ms) reload of the preview so
// we never thrash the QML engine on every keystroke. Hard reloads via F5
// bypass the debounce.
//
// Per the project UX principles (see docs/ux-principles.md):
//   • Keyboard-first: F5 reload, Ctrl+S save, Ctrl+R run-external,
//     F12 toggle preview pane.
//   • Inline > Modal: parse errors are shown in a red overlay on the preview
//     pane and in the status bar — never as message boxes.
//   • Status messages auto-dismiss after 3 s.
//   • VS dark-modern palette: #1e1e1e bg, #d4d4d4 text, #007acc accent.
#pragma once

#include <QHash>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QWidget>

class QSplitter;
class QLabel;
class QToolBar;
class QTimer;
class QPaintEvent;
class QResizeEvent;

namespace exo::forms {

class QmlPreviewPane;
class QmlLineNumberArea;

// ── Lightweight regex-based highlighter for QML.  Not a full grammar — the
//    project intentionally avoids tree-sitter for this small editor.
class QmlSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit QmlSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };
    QVector<Rule> m_rules;

    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_qmlTypeFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_propertyFormat;
    QTextCharFormat m_functionFormat;
    QRegularExpression m_blockStart;
    QRegularExpression m_blockEnd;
};

// ── Tiny QPlainTextEdit subclass that exposes the protected block-geometry
//    helpers needed by the gutter painter.  No new behaviour beyond that.
class QmlPlainTextEdit : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit QmlPlainTextEdit(QWidget *parent = nullptr) : QPlainTextEdit(parent) {}

    using QPlainTextEdit::firstVisibleBlock;
    using QPlainTextEdit::blockBoundingGeometry;
    using QPlainTextEdit::blockBoundingRect;
    using QPlainTextEdit::contentOffset;
};

// ── Top-level QML editor widget. Plain QWidget — not derived from EditorView
//    (that subsystem is out of scope for this widget). The hosting tab stores
//    the file path via setProperty("filePath", ...) so save/close flows work
//    transparently.
class QmlEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit QmlEditorWidget(QWidget *parent = nullptr);
    ~QmlEditorWidget() override;

    // Load a .qml file from disk. Returns true on success.
    bool loadFromFile(const QString &path);

    // Save the current editor contents back to filePath() (if set).
    bool save();

    // The path supplied to loadFromFile() (mirrored in property("filePath")).
    QString filePath() const { return m_filePath; }

    // Direct access for tests / external callers.
    QmlPlainTextEdit *editor() const { return m_editor; }
    QmlPreviewPane   *previewPane() const { return m_preview; }

signals:
    void modificationChanged(bool modified);
    void statusMessage(const QString &msg, int timeoutMs);

public slots:
    void reloadPreviewNow();        // F5 — force reload, ignores debounce
    void togglePreviewVisible();    // F12 — show/hide right pane
    void runInExternalWindow();     // Ctrl+R — open QQuickView top-level

private slots:
    void onTextChanged();
    void onDebounceTimeout();
    void onPreviewError(const QString &msg);
    void onPreviewReloaded();

private:
    void buildUi();
    void wireShortcuts();
    void applyDarkPalette();
    void updateStatus(const QString &msg, int timeoutMs = 3000);
    void updateFileSizeStatus();

    QString             m_filePath;
    QmlPlainTextEdit   *m_editor       = nullptr;
    QmlPreviewPane     *m_preview      = nullptr;
    QSplitter          *m_splitter     = nullptr;
    QToolBar           *m_toolbar      = nullptr;
    QLabel             *m_statusLabel  = nullptr;
    QTimer             *m_debounce     = nullptr;
    QmlSyntaxHighlighter *m_highlighter = nullptr;

    bool m_loadingFile = false;
    bool m_modified    = false;
};

} // namespace exo::forms

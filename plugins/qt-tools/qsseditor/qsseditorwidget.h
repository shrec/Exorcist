// QssEditorWidget — split-view QSS (Qt Stylesheet) editor with live preview.
//
// Layout:
//   ┌──────────────────────────────────────────────────────────────────────┐
//   │ Toolbar: Reload (F5)  Toggle Preview (F12)  Copy (Ctrl+Shift+C)  ... │
//   ├──────────────────────────────────────────────────────────────────────┤
//   │ QSplitter (horizontal, 50/50)                                        │
//   │ ┌──── editor (left) ────┬──── preview (right) ───────┐               │
//   │ │ QPlainTextEdit + nums │  QssPreviewPane            │               │
//   │ └───────────────────────┴────────────────────────────┘               │
//   ├──────────────────────────────────────────────────────────────────────┤
//   │ Status bar: parse warnings + character count                         │
//   └──────────────────────────────────────────────────────────────────────┘
//
// Edits trigger a 300 ms debounced reload of the preview so we never thrash
// QStyle on every keystroke. F5 / Ctrl+Shift+R bypass the debounce.
//
// Per docs/ux-principles.md:
//   • Keyboard-first: F5/Ctrl+Shift+R reload, Ctrl+S save, Ctrl+Shift+C copy,
//     F12 toggle preview pane.
//   • Inline > Modal: parse warnings shown inline in status bar, never modal.
//   • Status messages auto-dismiss after 3 s.
//   • VS dark-modern palette: #1e1e1e bg, #d4d4d4 text, #007acc accent.
#pragma once

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

namespace exo::forms {

class QssPreviewPane;

// ── Regex-based highlighter for QSS (Qt Stylesheet) syntax.
//    Highlights selectors, properties, color values, strings, numbers/units,
//    comments — using the VS-Code-ish dark palette.
class QssSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit QssSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat    format;
        int                captureGroup = 0; // 0 = full match
    };
    QVector<Rule> m_rules;

    QTextCharFormat m_selectorFormat;
    QTextCharFormat m_propertyFormat;
    QTextCharFormat m_colorFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_pseudoStateFormat;

    QRegularExpression m_blockStart;
    QRegularExpression m_blockEnd;
};

// ── Tiny QPlainTextEdit subclass that exposes the protected block-geometry
//    helpers needed by the gutter painter.
class QssPlainTextEdit : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit QssPlainTextEdit(QWidget *parent = nullptr) : QPlainTextEdit(parent) {}

    using QPlainTextEdit::firstVisibleBlock;
    using QPlainTextEdit::blockBoundingGeometry;
    using QPlainTextEdit::blockBoundingRect;
    using QPlainTextEdit::contentOffset;
};

// ── Top-level QSS editor widget. Plain QWidget — not derived from EditorView.
class QssEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit QssEditorWidget(QWidget *parent = nullptr);
    ~QssEditorWidget() override;

    // Load a .qss file from disk. Returns true on success.
    bool loadFromFile(const QString &path);

    // Save the current editor contents back to filePath() (if set).
    bool save();

    // The path supplied to loadFromFile().
    QString filePath() const { return m_filePath; }

    QssPlainTextEdit *editor() const { return m_editor; }
    QssPreviewPane   *previewPane() const { return m_preview; }

signals:
    void modificationChanged(bool modified);
    void statusMessage(const QString &msg, int timeoutMs);

public slots:
    void reloadPreviewNow();        // F5 — force reload, ignores debounce
    void togglePreviewVisible();    // F12 — show/hide right pane
    void copyQssToClipboard();      // Ctrl+Shift+C — copy current QSS

private slots:
    void onTextChanged();
    void onDebounceTimeout();

private:
    void buildUi();
    void wireShortcuts();
    void applyDarkPalette();
    void updateStatus(const QString &msg, int timeoutMs = 3000, bool warning = false);
    void updateInfoStatus();

    // Lightweight QSS parse-warning detection: counts of unmatched braces,
    // unterminated strings/comments. Returns first warning line or empty.
    QString detectParseWarning() const;

    QString             m_filePath;
    QssPlainTextEdit   *m_editor       = nullptr;
    QssPreviewPane     *m_preview      = nullptr;
    QSplitter          *m_splitter     = nullptr;
    QToolBar           *m_toolbar      = nullptr;
    QLabel             *m_statusLabel  = nullptr;
    QTimer             *m_debounce     = nullptr;
    QssSyntaxHighlighter *m_highlighter = nullptr;

    bool m_loadingFile = false;
    bool m_modified    = false;
};

} // namespace exo::forms

#pragma once

#include <QString>
#include <QWidget>

class QTextBrowser;
class QLabel;

/// MarkdownPreviewPanel — side dock that renders the active editor's
/// Markdown (.md / .markdown) buffer as HTML.
///
/// Design notes:
///  * Read-only QTextBrowser inside a QVBoxLayout.
///  * Implements its own minimal Markdown→HTML translator (no external lib):
///    headings, bold, italic, inline code, fenced code blocks, links, lists,
///    blockquotes, horizontal rules, plain paragraphs.
///  * VS Code dark-theme styling to match the rest of the IDE.
///  * Owners (e.g. MainWindow) feed text through setMarkdown() — typically on a
///    debounced timer driven by the active editor's contentsChange signal.
class MarkdownPreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MarkdownPreviewPanel(QWidget *parent = nullptr);
    ~MarkdownPreviewPanel() override = default;

    /// Render the supplied Markdown source into the preview area.
    /// Pass an empty string to clear the preview ("no Markdown file open").
    void setMarkdown(const QString &md);

    /// Convenience: returns true if the supplied file path looks like Markdown
    /// (case-insensitive .md / .markdown extension).
    static bool isMarkdownPath(const QString &path);

    /// Public for tests — convert the supplied Markdown source into HTML.
    static QString markdownToHtml(const QString &md);

private:
    QLabel        *m_header  = nullptr;
    QTextBrowser  *m_browser = nullptr;
};

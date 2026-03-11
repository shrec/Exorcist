#pragma once

#include <QString>
#include <QList>

// Lightweight Markdown → HTML converter for chat display.
// Handles: fenced code blocks, inline code, bold, italic, headers, links.

namespace MarkdownRenderer {

struct CodeBlock
{
    int     index = -1;
    QString language;
    QString filePath;
    QString code;
};

struct RenderResult
{
	QString html;
	QList<CodeBlock> codeBlocks;
};

QString toHtml(const QString &markdown);
RenderResult toHtmlWithActions(const QString &markdown);

} // namespace MarkdownRenderer

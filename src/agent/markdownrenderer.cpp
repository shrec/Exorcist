#include "markdownrenderer.h"

#include <QRegularExpression>
#include <QStringList>

namespace MarkdownRenderer {

static QString escapeHtml(const QString &text)
{
    QString out = text;
    out.replace(QLatin1Char('&'),  QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'),  QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'),  QStringLiteral("&gt;"));
    return out;
}

// Process inline formatting: **bold**, ~~strikethrough~~, *italic*, `code`, [text](url), ![alt](img)
static QString processInline(const QString &line)
{
    QString out = escapeHtml(line);

    // Inline code: `code`  (must be first to protect code from other rules)
    static const QRegularExpression reCode(QStringLiteral("`([^`]+)`"));
    out.replace(reCode, QStringLiteral("<code>\\1</code>"));

    // Bold+Italic: ***text***
    static const QRegularExpression reBoldItalic(QStringLiteral("\\*\\*\\*(.+?)\\*\\*\\*"));
    out.replace(reBoldItalic, QStringLiteral("<b><i>\\1</i></b>"));

    // Bold: **text** or __text__
    static const QRegularExpression reBold(QStringLiteral("\\*\\*(.+?)\\*\\*"));
    out.replace(reBold, QStringLiteral("<b>\\1</b>"));
    static const QRegularExpression reBoldU(QStringLiteral("__(.+?)__"));
    out.replace(reBoldU, QStringLiteral("<b>\\1</b>"));

    // Strikethrough: ~~text~~
    static const QRegularExpression reStrike(QStringLiteral("~~(.+?)~~"));
    out.replace(reStrike, QStringLiteral("<s>\\1</s>"));

    // Italic: *text* (but not inside **)
    static const QRegularExpression reItalic(QStringLiteral("(?<!\\*)\\*([^*]+)\\*(?!\\*)"));
    out.replace(reItalic, QStringLiteral("<i>\\1</i>"));

    // Image: ![alt](url)
    static const QRegularExpression reImg(QStringLiteral("!\\[([^\\]]*)\\]\\(([^)]+)\\)"));
    out.replace(reImg, QStringLiteral("<img src='\\2' alt='\\1' style='max-width:100%;'>"));

    // Links: [text](url)
    static const QRegularExpression reLink(QStringLiteral("\\[([^\\]]+)\\]\\(([^)]+)\\)"));
    out.replace(reLink, QStringLiteral("<a href='\\2'>\\1</a>"));

    return out;
}

static QString extractFilePathHint(const QString &code)
{
    const QString firstLine = code.section(QLatin1Char('\n'), 0, 0).trimmed();
    if (firstLine.startsWith(QLatin1String("// file:"), Qt::CaseInsensitive))
        return firstLine.mid(8).trimmed();
    if (firstLine.startsWith(QLatin1String("# file:"), Qt::CaseInsensitive))
        return firstLine.mid(7).trimmed();
    return {};
}

// Detect if a trimmed line is a table separator row: |---|---|
static bool isTableSepRow(const QString &trimmed)
{
    if (!trimmed.startsWith(QLatin1Char('|')))
        return false;
    static const QRegularExpression reSep(
        QStringLiteral("^\\|[\\s:]*-{3,}[\\s:]*(?:\\|[\\s:]*-{3,}[\\s:]*)*\\|?$"));
    return reSep.match(trimmed).hasMatch();
}

static QStringList splitTableCells(const QString &row)
{
    QString inner = row.trimmed();
    if (inner.startsWith(QLatin1Char('|'))) inner = inner.mid(1);
    if (inner.endsWith(QLatin1Char('|')))   inner.chop(1);
    QStringList cells;
    const auto parts = inner.split(QLatin1Char('|'));
    for (const QString &p : parts)
        cells << p.trimmed();
    return cells;
}

// ── Code block emitter ────────────────────────────────────────────────────────

static void emitCodeBlock(QString &html, QList<CodeBlock> &blocks,
                          const QString &lang, const QString &code, int &idx)
{
    CodeBlock block;
    block.language = lang;
    block.code     = code;
    block.filePath = extractFilePathHint(code);
    blocks.append(block);

    // Language label header (like VS Code)
    const QString langLabel = lang.isEmpty()
        ? QStringLiteral("text")
        : lang.toLower();

    html += QStringLiteral(
        "<div class='code-header'>"
        "<span class='code-lang'>%1</span>"
        "<span class='code-header-actions'>"
        "<a href='codeblock://%2/copy'>Copy</a>"
        "</span>"
        "</div>").arg(langLabel.toHtmlEscaped()).arg(idx);
    html += QStringLiteral("<pre><code>%1</code></pre>")
                .arg(escapeHtml(code));
    html += QStringLiteral(
        "<div class='code-actions'>"
        "<a href='codeblock://%1/apply'>Apply</a> · "
        "<a href='codeblock://%1/insert'>Insert</a> · "
        "<a href='codeblock://%1/copy'>Copy</a> · "
        "<a href='codeblock://%1/run'>Run</a>"
        "</div>").arg(idx);
    ++idx;
}

// ── Main renderer ─────────────────────────────────────────────────────────────

RenderResult toHtmlWithActions(const QString &markdown)
{
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    RenderResult result;
    QString &html = result.html;

    bool inCodeBlock = false;
    QString codeAccum;
    QString codeLang;
    int codeIndex = 0;

    // List tracking
    bool inUl = false;
    bool inOl = false;
    // Blockquote tracking
    bool inBlockquote = false;
    // Table tracking
    bool inTable = false;
    bool tableHeaderDone = false;
    QStringList pendingTableHeader;

    // Helpers to close open block elements
    auto closeUl = [&]() { if (inUl) { html += QStringLiteral("</ul>"); inUl = false; } };
    auto closeOl = [&]() { if (inOl) { html += QStringLiteral("</ol>"); inOl = false; } };
    auto closeBq = [&]() { if (inBlockquote) { html += QStringLiteral("</blockquote>"); inBlockquote = false; } };
    auto closeTable = [&]() {
        if (inTable) {
            html += QStringLiteral("</tbody></table>");
            inTable = false;
            tableHeaderDone = false;
            pendingTableHeader.clear();
        }
    };
    auto closeAllBlocks = [&]() { closeUl(); closeOl(); closeBq(); closeTable(); };

    static const QRegularExpression reOrderedList(QStringLiteral("^(\\s*)(\\d+)\\.\\s+(.*)$"));

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];

        // ── Fenced code block ─────────────────────────────────────────────
        if (line.trimmed().startsWith(QLatin1String("```"))) {
            if (!inCodeBlock) {
                closeAllBlocks();
                inCodeBlock = true;
                codeLang = line.trimmed().mid(3).trimmed();
                codeAccum.clear();
            } else {
                emitCodeBlock(html, result.codeBlocks, codeLang, codeAccum, codeIndex);
                inCodeBlock = false;
                codeAccum.clear();
                codeLang.clear();
            }
            continue;
        }

        if (inCodeBlock) {
            if (!codeAccum.isEmpty())
                codeAccum += QLatin1Char('\n');
            codeAccum += line;
            continue;
        }

        const QString trimmed = line.trimmed();

        // ── Horizontal rule: --- / *** / ___ (3+ chars) ───────────────────
        if (trimmed.size() >= 3) {
            bool allDash = true, allStar = true, allUnder = true;
            for (const QChar &ch : trimmed) {
                if (ch != QLatin1Char('-') && ch != QLatin1Char(' ')) allDash = false;
                if (ch != QLatin1Char('*') && ch != QLatin1Char(' ')) allStar = false;
                if (ch != QLatin1Char('_') && ch != QLatin1Char(' ')) allUnder = false;
            }
            if (allDash || allStar || allUnder) {
                // Ensure at least 3 non-space chars
                int count = 0;
                for (const QChar &ch : trimmed)
                    if (ch != QLatin1Char(' ')) ++count;
                if (count >= 3 && !trimmed.startsWith(QLatin1String("- "))) {
                    closeAllBlocks();
                    html += QStringLiteral("<hr>");
                    continue;
                }
            }
        }

        // ── Table detection ───────────────────────────────────────────────
        if (trimmed.startsWith(QLatin1Char('|')) && trimmed.contains(QLatin1String("|"))) {
            if (!inTable && !isTableSepRow(trimmed)) {
                // First row = header candidate
                pendingTableHeader = splitTableCells(trimmed);
                // Peek next line for separator
                if (i + 1 < lines.size() && isTableSepRow(lines[i + 1].trimmed())) {
                    closeAllBlocks();
                    inTable = true;
                    html += QStringLiteral("<table><thead><tr>");
                    for (const QString &cell : std::as_const(pendingTableHeader))
                        html += QStringLiteral("<th>%1</th>").arg(processInline(cell));
                    html += QStringLiteral("</tr></thead><tbody>");
                    tableHeaderDone = false;
                    continue;
                }
            }
            if (inTable) {
                if (isTableSepRow(trimmed)) {
                    tableHeaderDone = true;
                    continue;  // skip separator row
                }
                // Data row
                const QStringList cells = splitTableCells(trimmed);
                html += QStringLiteral("<tr>");
                for (const QString &cell : cells)
                    html += QStringLiteral("<td>%1</td>").arg(processInline(cell));
                html += QStringLiteral("</tr>");
                continue;
            }
        } else {
            closeTable();
        }

        // ── Blockquote: > text ────────────────────────────────────────────
        if (trimmed.startsWith(QLatin1String("> ")) || trimmed == QLatin1String(">")) {
            closeUl(); closeOl(); closeTable();
            if (!inBlockquote) {
                inBlockquote = true;
                html += QStringLiteral("<blockquote>");
            }
            const QString content = (trimmed.size() > 2) ? trimmed.mid(2) : QString();
            html += processInline(content) + QStringLiteral("<br>");
            continue;
        } else {
            closeBq();
        }

        // ── Headers ───────────────────────────────────────────────────────
        if (trimmed.startsWith(QLatin1String("#### "))) {
            closeAllBlocks();
            html += QStringLiteral("<h4>%1</h4>").arg(processInline(trimmed.mid(5)));
            continue;
        }
        if (trimmed.startsWith(QLatin1String("### "))) {
            closeAllBlocks();
            html += QStringLiteral("<h3>%1</h3>").arg(processInline(trimmed.mid(4)));
            continue;
        }
        if (trimmed.startsWith(QLatin1String("## "))) {
            closeAllBlocks();
            html += QStringLiteral("<h2>%1</h2>").arg(processInline(trimmed.mid(3)));
            continue;
        }
        if (trimmed.startsWith(QLatin1String("# "))) {
            closeAllBlocks();
            html += QStringLiteral("<h1>%1</h1>").arg(processInline(trimmed.mid(2)));
            continue;
        }

        // ── Unordered list: - item / * item ───────────────────────────────
        if (trimmed.startsWith(QLatin1String("- ")) ||
            trimmed.startsWith(QLatin1String("* "))) {
            closeOl(); closeTable(); closeBq();
            if (!inUl) {
                inUl = true;
                html += QStringLiteral("<ul>");
            }
            html += QStringLiteral("<li>%1</li>").arg(processInline(trimmed.mid(2)));
            continue;
        }

        // ── Checkbox list: - [ ] / - [x] ─────────────────────────────────
        if (trimmed.startsWith(QLatin1String("- [ ] ")) ||
            trimmed.startsWith(QLatin1String("- [x] ")) ||
            trimmed.startsWith(QLatin1String("- [X] "))) {
            closeOl(); closeTable(); closeBq();
            if (!inUl) {
                inUl = true;
                html += QStringLiteral("<ul style='list-style:none;padding-left:0;'>");
            }
            const bool checked = trimmed[3] != QLatin1Char(' ');
            const QString checkbox = checked
                ? QStringLiteral("&#9745; ") : QStringLiteral("&#9744; ");
            html += QStringLiteral("<li>%1%2</li>")
                .arg(checkbox, processInline(trimmed.mid(6)));
            continue;
        }

        // ── Ordered list: 1. item / 2. item ──────────────────────────────
        const auto olMatch = reOrderedList.match(line);
        if (olMatch.hasMatch()) {
            closeUl(); closeTable(); closeBq();
            if (!inOl) {
                inOl = true;
                html += QStringLiteral("<ol>");
            }
            html += QStringLiteral("<li>%1</li>").arg(processInline(olMatch.captured(3)));
            continue;
        }

        // If we get here, close any open list/block
        closeUl(); closeOl();

        // ── Blank line ────────────────────────────────────────────────────
        if (trimmed.isEmpty()) {
            html += QStringLiteral("<br>");
            continue;
        }

        // ── Normal paragraph text ─────────────────────────────────────────
        html += processInline(trimmed) + QStringLiteral("<br>");
    }

    // Close any open blocks
    closeAllBlocks();

    // Handle unclosed code block
    if (inCodeBlock && !codeAccum.isEmpty())
        emitCodeBlock(html, result.codeBlocks, codeLang, codeAccum, codeIndex);

    return result;
}

QString toHtml(const QString &markdown)
{
    return toHtmlWithActions(markdown).html;
}

} // namespace MarkdownRenderer

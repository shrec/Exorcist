#include "markdownpreviewpanel.h"

#include <QFileInfo>
#include <QLabel>
#include <QRegularExpression>
#include <QStringList>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

/// HTML-escape a snippet of plain text (so user content cannot inject tags).
QString escapeHtml(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (QChar c : s) {
        switch (c.unicode()) {
        case '&': out += QStringLiteral("&amp;"); break;
        case '<': out += QStringLiteral("&lt;");  break;
        case '>': out += QStringLiteral("&gt;");  break;
        case '"': out += QStringLiteral("&quot;"); break;
        default:  out += c; break;
        }
    }
    return out;
}

/// Apply inline Markdown transforms to a single line of (already
/// HTML-escaped-where-necessary) text.  Order matters:
///   1. Inline code (so its content is shielded from further conversion)
///   2. Bold (**...**)
///   3. Italic (*...* / _..._)
///   4. Links [text](url)
QString applyInlineFormatting(const QString &lineIn)
{
    QString line = lineIn;

    // 1. Inline code: escape its contents & wrap in <code>.
    static const QRegularExpression rxCode(QStringLiteral("`([^`]+)`"));
    {
        QString out;
        int last = 0;
        auto it = rxCode.globalMatch(line);
        while (it.hasNext()) {
            const auto m = it.next();
            out += escapeHtml(line.mid(last, m.capturedStart() - last));
            out += QStringLiteral("<code>") + escapeHtml(m.captured(1))
                 + QStringLiteral("</code>");
            last = m.capturedEnd();
        }
        out += escapeHtml(line.mid(last));
        line = out;
    }

    // 2. Bold: **text**.  Operates on already-escaped HTML — so the literal
    // sequence "**" is still intact.
    static const QRegularExpression rxBold(QStringLiteral("\\*\\*([^*]+)\\*\\*"));
    line.replace(rxBold, QStringLiteral("<b>\\1</b>"));

    // 3a. Italic via *...* — but not inside an already-handled <b>**...
    // Simple form: a single * not adjacent to another *.
    static const QRegularExpression rxItalicStar(
        QStringLiteral("(^|[^*])\\*([^*\\s][^*]*?)\\*(?!\\*)"));
    line.replace(rxItalicStar, QStringLiteral("\\1<i>\\2</i>"));

    // 3b. Italic via _..._ (avoid mid-word matches).
    static const QRegularExpression rxItalicUnderscore(
        QStringLiteral("(^|\\s)_([^_]+)_(?=$|\\s|[.,;:!?])"));
    line.replace(rxItalicUnderscore, QStringLiteral("\\1<i>\\2</i>"));

    // 4. Links: [text](url)
    static const QRegularExpression rxLink(
        QStringLiteral("\\[([^\\]]+)\\]\\(([^)]+)\\)"));
    line.replace(rxLink, QStringLiteral("<a href=\"\\2\">\\1</a>"));

    return line;
}

/// Output state — keeps track of whether we're currently inside a list,
/// blockquote, or paragraph so the translator can emit matching closing tags.
struct EmitState {
    QString out;
    enum Block { None, Para, Ul, Ol, Quote, Pre } block = None;

    void closeBlock()
    {
        switch (block) {
        case Para:  out += QStringLiteral("</p>\n");          break;
        case Ul:    out += QStringLiteral("</ul>\n");         break;
        case Ol:    out += QStringLiteral("</ol>\n");         break;
        case Quote: out += QStringLiteral("</blockquote>\n"); break;
        case Pre:   out += QStringLiteral("</code></pre>\n"); break;
        case None:  break;
        }
        block = None;
    }

    void enter(Block b, const QString &openTag)
    {
        if (block != b) {
            closeBlock();
            out += openTag;
            block = b;
        }
    }
};

} // namespace

// ─── MarkdownPreviewPanel ──────────────────────────────────────────────────

MarkdownPreviewPanel::MarkdownPreviewPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_header = new QLabel(tr("Markdown Preview"), this);
    m_header->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background: #252526;"
        "  color: #cccccc;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "  border-bottom: 1px solid #1e1e1e;"
        "}"));
    layout->addWidget(m_header);

    m_browser = new QTextBrowser(this);
    m_browser->setReadOnly(true);
    m_browser->setOpenExternalLinks(true);
    m_browser->setStyleSheet(QStringLiteral(
        "QTextBrowser {"
        "  background: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: none;"
        "  padding: 8px 12px;"
        "  font-size: 13px;"
        "}"
        "QScrollBar:vertical {"
        "  background: #1e1e1e;"
        "  width: 10px;"
        "  border: none;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #424242;"
        "  min-height: 20px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #686868;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"));
    layout->addWidget(m_browser, 1);

    setMarkdown(QString());
}

void MarkdownPreviewPanel::setMarkdown(const QString &md)
{
    if (!m_browser)
        return;

    if (md.isEmpty()) {
        m_browser->setHtml(QStringLiteral(
            "<div style=\"color:#858585;font-style:italic;\">"
            "No Markdown file open."
            "</div>"));
        return;
    }
    m_browser->setHtml(markdownToHtml(md));
}

bool MarkdownPreviewPanel::isMarkdownPath(const QString &path)
{
    if (path.isEmpty())
        return false;
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("md") || suffix == QLatin1String("markdown");
}

QString MarkdownPreviewPanel::markdownToHtml(const QString &md)
{
    EmitState st;
    st.out.reserve(md.size() * 2);

    // Wrap output in a styled body for the QTextBrowser document.
    st.out += QStringLiteral(
        "<style>"
        " body { color:#d4d4d4; font-family:'Segoe UI', sans-serif; line-height:1.5; }"
        " h1,h2,h3,h4,h5,h6 { color:#ffffff; margin-top:18px; margin-bottom:6px; }"
        " h1 { font-size:22px; border-bottom:1px solid #3c3c3c; padding-bottom:4px; }"
        " h2 { font-size:18px; border-bottom:1px solid #3c3c3c; padding-bottom:3px; }"
        " h3 { font-size:16px; }"
        " p  { margin:6px 0; }"
        " a  { color:#4fc1ff; text-decoration:none; }"
        " code { background:#2d2d2d; color:#ce9178; padding:1px 4px; border-radius:3px;"
        "        font-family:Consolas, 'Courier New', monospace; font-size:12px; }"
        " pre { background:#1e1e1e; border:1px solid #3c3c3c; border-radius:4px;"
        "       padding:8px 10px; overflow:auto; }"
        " pre code { background:transparent; color:#d4d4d4; padding:0; font-size:12px; }"
        " blockquote { border-left:3px solid #007acc; margin:6px 0; padding:2px 10px;"
        "              color:#a6a6a6; background:#252526; }"
        " ul, ol { margin:4px 0 8px 22px; }"
        " li { margin:2px 0; }"
        " hr { border:none; border-top:1px solid #3c3c3c; margin:12px 0; }"
        "</style>\n");

    const QStringList lines = md.split(QChar('\n'));

    static const QRegularExpression rxFence(QStringLiteral("^```(.*)$"));
    static const QRegularExpression rxHeading(QStringLiteral("^(#{1,6})\\s+(.*)$"));
    static const QRegularExpression rxUl(QStringLiteral("^[*\\-+]\\s+(.*)$"));
    static const QRegularExpression rxOl(QStringLiteral("^\\d+\\.\\s+(.*)$"));
    static const QRegularExpression rxQuote(QStringLiteral("^>\\s?(.*)$"));
    static const QRegularExpression rxHr(QStringLiteral("^(\\-{3,}|\\*{3,}|_{3,})\\s*$"));

    for (int i = 0; i < lines.size(); ++i) {
        const QString raw = lines[i];
        const QString trimmed = raw.trimmed();

        // ── Fenced code block: ```lang … ``` ──────────────────────────────
        if (st.block == EmitState::Pre) {
            if (rxFence.match(raw).hasMatch()) {
                st.closeBlock();
            } else {
                st.out += escapeHtml(raw);
                st.out += QChar('\n');
            }
            continue;
        }
        if (rxFence.match(raw).hasMatch()) {
            st.closeBlock();
            st.out += QStringLiteral("<pre><code>");
            st.block = EmitState::Pre;
            continue;
        }

        // ── Blank line: terminates current block ──────────────────────────
        if (trimmed.isEmpty()) {
            st.closeBlock();
            continue;
        }

        // ── Horizontal rule ──────────────────────────────────────────────
        if (rxHr.match(trimmed).hasMatch()) {
            st.closeBlock();
            st.out += QStringLiteral("<hr/>\n");
            continue;
        }

        // ── Heading ──────────────────────────────────────────────────────
        if (auto m = rxHeading.match(raw); m.hasMatch()) {
            st.closeBlock();
            const int level = m.captured(1).size();
            const QString body = applyInlineFormatting(m.captured(2));
            st.out += QStringLiteral("<h%1>%2</h%1>\n").arg(level).arg(body);
            continue;
        }

        // ── Unordered list ───────────────────────────────────────────────
        if (auto m = rxUl.match(trimmed); m.hasMatch()) {
            st.enter(EmitState::Ul, QStringLiteral("<ul>\n"));
            st.out += QStringLiteral("<li>")
                    + applyInlineFormatting(m.captured(1))
                    + QStringLiteral("</li>\n");
            continue;
        }

        // ── Ordered list ─────────────────────────────────────────────────
        if (auto m = rxOl.match(trimmed); m.hasMatch()) {
            st.enter(EmitState::Ol, QStringLiteral("<ol>\n"));
            st.out += QStringLiteral("<li>")
                    + applyInlineFormatting(m.captured(1))
                    + QStringLiteral("</li>\n");
            continue;
        }

        // ── Blockquote ───────────────────────────────────────────────────
        if (auto m = rxQuote.match(trimmed); m.hasMatch()) {
            st.enter(EmitState::Quote, QStringLiteral("<blockquote>"));
            st.out += applyInlineFormatting(m.captured(1));
            st.out += QStringLiteral("<br/>");
            continue;
        }

        // ── Plain paragraph (greedy: consecutive lines coalesce) ─────────
        st.enter(EmitState::Para, QStringLiteral("<p>"));
        st.out += applyInlineFormatting(raw);
        st.out += QStringLiteral(" ");
    }

    st.closeBlock();
    return st.out;
}

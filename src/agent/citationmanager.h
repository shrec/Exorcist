#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTextBrowser>
#include <QTextCursor>

// ─────────────────────────────────────────────────────────────────────────────
// CitationManager — tracks and displays source URLs in AI responses.
//
// When the agent processes web content, this manager tracks the source URLs
// and generates citation footnotes for display in the chat panel.
// ─────────────────────────────────────────────────────────────────────────────

class CitationManager : public QObject
{
    Q_OBJECT

public:
    explicit CitationManager(QObject *parent = nullptr) : QObject(parent) {}

    struct Citation {
        int    number;
        QString url;
        QString title;
        QString snippet;
    };

    // Add a citation and return its number
    int addCitation(const QString &url, const QString &title = {},
                    const QString &snippet = {})
    {
        const int num = m_citations.size() + 1;
        m_citations.append({num, url, title.isEmpty() ? url : title, snippet});
        return num;
    }

    // Get all citations for current response
    QList<Citation> citations() const { return m_citations; }

    // Generate HTML footnotes block
    QString toHtml() const
    {
        if (m_citations.isEmpty()) return {};

        QString html = QStringLiteral("<hr><div class='citations' style='font-size:11px; color:palette(mid);'>"
                                       "<b>Sources:</b><ol>");
        for (const auto &c : m_citations) {
            html += QStringLiteral("<li><a href='%1'>%2</a>")
                .arg(c.url.toHtmlEscaped(), c.title.toHtmlEscaped());
            if (!c.snippet.isEmpty())
                html += QStringLiteral(" — <i>%1</i>").arg(c.snippet.left(100).toHtmlEscaped());
            html += QStringLiteral("</li>");
        }
        html += QStringLiteral("</ol></div>");
        return html;
    }

    // Generate markdown footnotes
    QString toMarkdown() const
    {
        if (m_citations.isEmpty()) return {};

        QString md = QStringLiteral("\n---\n**Sources:**\n");
        for (const auto &c : m_citations)
            md += QStringLiteral("%1. [%2](%3)\n").arg(c.number).arg(c.title, c.url);
        return md;
    }

    // Clear citations (call at start of new response)
    void clear() { m_citations.clear(); }

    // Check if text contains citation markers like [1], [2]
    static bool hasCitationMarkers(const QString &text)
    {
        static const QRegularExpression rx(QStringLiteral(R"(\[(\d+)\])"));
        return rx.match(text).hasMatch();
    }

private:
    QList<Citation> m_citations;
};

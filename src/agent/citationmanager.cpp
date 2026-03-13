#include "citationmanager.h"

#include <QRegularExpression>

CitationManager::CitationManager(QObject *parent)
    : QObject(parent)
{
}

int CitationManager::addCitation(const QString &url, const QString &title,
                                 const QString &snippet)
{
    const int num = m_citations.size() + 1;
    m_citations.append({num, url, title.isEmpty() ? url : title, snippet});
    return num;
}

QString CitationManager::toHtml() const
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

QString CitationManager::toMarkdown() const
{
    if (m_citations.isEmpty()) return {};

    QString md = QStringLiteral("\n---\n**Sources:**\n");
    for (const auto &c : m_citations)
        md += QStringLiteral("%1. [%2](%3)\n").arg(c.number).arg(c.title, c.url);
    return md;
}

void CitationManager::clear()
{
    m_citations.clear();
}

bool CitationManager::hasCitationMarkers(const QString &text)
{
    static const QRegularExpression rx(QStringLiteral(R"(\[(\d+)\])"));
    return rx.match(text).hasMatch();
}

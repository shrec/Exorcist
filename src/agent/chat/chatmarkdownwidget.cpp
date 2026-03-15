#include "chatmarkdownwidget.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QVBoxLayout>

// ── Constructor ───────────────────────────────────────────────────────────────

ChatMarkdownWidget::ChatMarkdownWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(false);
    m_browser->setReadOnly(true);
    m_browser->setFrameShape(QFrame::NoFrame);
    m_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_browser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_browser->setStyleSheet(
        QStringLiteral("QTextBrowser { background: transparent; border: none;"
                      " color: %1; font-family: %2; font-size: %3px; }")
            .arg(ChatTheme::FgPrimary, ChatTheme::FontFamily)
            .arg(ChatTheme::FontSize));
    m_browser->document()->setDefaultStyleSheet(defaultCss());

    connect(m_browser->document(), &QTextDocument::contentsChanged, this, [this]() {
        m_browser->setFixedHeight(int(m_browser->document()->size().height()) + 4);
        updateGeometry();
    });
    connect(m_browser, &QTextBrowser::anchorClicked,
            this, &ChatMarkdownWidget::handleAnchorClick);

    layout->addWidget(m_browser);
}

// ── Public methods ────────────────────────────────────────────────────────────

void ChatMarkdownWidget::setMarkdown(const QString &md, bool streaming)
{
    m_rawMarkdown = md;
    if (streaming) {
        const auto rendered = MarkdownRenderer::toHtmlWithActions(md);
        m_lastBlocks = rendered.codeBlocks;
        QString html = rendered.html;
        html += QStringLiteral("<span style='color:%1;'>\u2587</span>")
                    .arg(ChatTheme::AccentBlue);
        m_browser->setHtml(html);
    } else {
        const auto rendered = MarkdownRenderer::toHtmlWithActions(md);
        m_lastBlocks = rendered.codeBlocks;
        m_browser->setHtml(rendered.html);
    }
}

void ChatMarkdownWidget::appendDelta(const QString &delta)
{
    m_rawMarkdown += delta;
    setMarkdown(m_rawMarkdown, true);
}

void ChatMarkdownWidget::finalize()
{
    setMarkdown(m_rawMarkdown, false);
}

// ── Private slots ─────────────────────────────────────────────────────────────

void ChatMarkdownWidget::handleAnchorClick(const QUrl &url)
{
    if (url.scheme() == QLatin1String("codeblock")) {
        bool ok = false;
        const int idx = url.host().toInt(&ok);
        if (ok && idx >= 0 && idx < m_lastBlocks.size()) {
            const QString &code = m_lastBlocks[idx].code;
            const QString action = url.path().mid(1);
            if (action == QLatin1String("copy")) {
                QGuiApplication::clipboard()->setText(code);
                return;
            }
            if (action == QLatin1String("insert")
                || action == QLatin1String("apply")) {
                emit insertCodeRequested(code);
                return;
            }
        }
    }
    emit anchorClicked(url);
}

// ── Private helpers ───────────────────────────────────────────────────────────

QString ChatMarkdownWidget::defaultCss()
{
    return QStringLiteral(
        "body { color:%1; font-family:%2; font-size:%3px; line-height:1.55; }"
        "pre { background:%4; padding:10px 12px; border-radius:0 0 2px 2px;"
        "  border-left:3px solid #0e639c;"
        "  font-family:%5; font-size:13px; margin:0 0 8px 0; white-space:pre-wrap; }"
        "code { color:%6; font-family:%5; font-size:13px;"
        "  background:%7; padding:2px 6px; border-radius:3px; }"
        ".code-header { background:%8; border-radius:2px 2px 0 0;"
        "  padding:5px 10px; margin:8px 0 0 0; font-size:11px; color:%9; }"
        ".code-lang { color:%10; font-weight:600; }"
        ".code-header-actions a { color:%10; font-size:11px; margin-left:8px; }"
        "a { color:%11; text-decoration:none; }"
        "a:hover { color:%12; text-decoration:underline; }"
        "blockquote { border-left:3px solid %12; padding:6px 14px;"
        "  margin:8px 0; color:%9; background:%4; border-radius:0 4px 4px 0; }"
        "table.data { border-collapse:collapse; margin:8px 0; width:100%%; }"
        "th { background:%8; color:%10; padding:5px 10px; text-align:left;"
        "  font-size:12px; border-bottom:1px solid %13; }"
        "td { padding:4px 10px; font-size:12px; border-bottom:1px solid %14; }"
        "ol { padding-left:24px; margin:4px 0; }"
        "ul { padding-left:20px; margin:4px 0; }"
        "li { margin:2px 0; }"
        "hr { border:none; border-top:1px solid %14; margin:8px 10px; }"
    ).arg(ChatTheme::FgPrimary, ChatTheme::FontFamily)
        .arg(ChatTheme::FontSize)
        .arg(ChatTheme::CodeBg, ChatTheme::MonoFamily, ChatTheme::FgCode,
             ChatTheme::InlineCodeBg, ChatTheme::CodeHeaderBg,
             ChatTheme::FgSecondary, ChatTheme::KeywordBlue,
             ChatTheme::LinkBlue, ChatTheme::AccentBlue,
             ChatTheme::Border, ChatTheme::SepLine);
}

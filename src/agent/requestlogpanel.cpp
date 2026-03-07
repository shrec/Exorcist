#include "requestlogpanel.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

RequestLogPanel::RequestLogPanel(QWidget *parent)
    : QWidget(parent)
    , m_browser(new QTextBrowser(this))
    , m_clearBtn(new QToolButton(this))
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    m_clearBtn->setText(tr("Clear Log"));
    m_clearBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_clearBtn, &QToolButton::clicked, this, &RequestLogPanel::clear);
    lay->addWidget(m_clearBtn);

    m_browser->setOpenLinks(false);
    m_browser->setReadOnly(true);
    m_browser->document()->setDefaultStyleSheet(QStringLiteral(
        "body { font-family: 'Consolas','Courier New',monospace; font-size: 12px; "
        "       color: #d4d4d4; background: #1e1e1e; }"
        ".ts  { color: #666; font-size: 10px; }"
        ".req { color: #569cd6; font-weight: bold; }"
        ".res { color: #4ec9b0; }"
        ".tool{ color: #dcdcaa; }"
        ".err { color: #f44747; font-weight: bold; }"
        ".ctx { color: #9cdcfe; }"
        ".ok  { color: #6a9955; }"
        ".fail{ color: #f44747; }"
        "pre  { white-space: pre-wrap; margin: 2px 0; }"
        "hr   { border: 0; border-top: 1px solid #333; }"
    ));
    lay->addWidget(m_browser);
}

void RequestLogPanel::clear()
{
    m_browser->clear();
    m_entryCount = 0;
}

void RequestLogPanel::appendLog(const QString &css, const QString &label,
                                const QString &body)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
    const QString html = QStringLiteral(
        "<div><span class='ts'>[%1]</span> <span class='%2'>%3</span>"
        "<pre>%4</pre></div><hr>")
        .arg(ts.toHtmlEscaped(), css, label.toHtmlEscaped(),
             body.toHtmlEscaped());
    m_browser->append(html);
    ++m_entryCount;
}

void RequestLogPanel::logRequest(const QString &requestId, const QString &model,
                                 int messageCount, int toolCount)
{
    const QString body = QStringLiteral(
        "ID: %1\nModel: %2\nMessages: %3\nTools: %4")
        .arg(requestId, model)
        .arg(messageCount)
        .arg(toolCount);
    appendLog(QStringLiteral("req"), tr("→ REQUEST"), body);
}

void RequestLogPanel::logResponse(const QString &requestId, const QString &text)
{
    const QString preview = text.length() > 500
        ? text.left(500) + QStringLiteral("…[truncated]")
        : text;
    appendLog(QStringLiteral("res"),
              tr("← RESPONSE (%1)").arg(requestId),
              preview);
}

void RequestLogPanel::logToolCall(const QString &toolName, const QJsonObject &args,
                                  bool ok, const QString &resultPreview)
{
    const QString argsStr = QString::fromUtf8(
        QJsonDocument(args).toJson(QJsonDocument::Compact));
    const QString body = QStringLiteral(
        "Tool: %1\nArgs: %2\nResult: %3\n%4")
        .arg(toolName,
             argsStr.length() > 300 ? argsStr.left(300) + QStringLiteral("…") : argsStr,
             ok ? tr("OK") : tr("FAILED"),
             resultPreview.length() > 200
                 ? resultPreview.left(200) + QStringLiteral("…")
                 : resultPreview);
    appendLog(QStringLiteral("tool"), tr("⚙ TOOL CALL"), body);
}

void RequestLogPanel::logError(const QString &requestId, const QString &error)
{
    appendLog(QStringLiteral("err"),
              tr("✗ ERROR (%1)").arg(requestId),
              error);
}

void RequestLogPanel::logContext(const QString &section, const QString &content)
{
    const QString preview = content.length() > 1000
        ? content.left(1000) + QStringLiteral("…[truncated]")
        : content;
    appendLog(QStringLiteral("ctx"),
              tr("📋 CONTEXT: %1").arg(section),
              preview);
}

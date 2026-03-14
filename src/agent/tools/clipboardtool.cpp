#include "clipboardtool.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>

ToolSpec ClipboardTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("clipboard");
    s.description = QStringLiteral(
        "Access the system clipboard. Operations:\n"
        "  'read' — read current clipboard text content\n"
        "  'write' — write text to clipboard\n"
        "  'has_text' — check if clipboard contains text");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("operation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("One of: read, write, has_text")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("read"), QStringLiteral("write"),
                    QStringLiteral("has_text")}}
            }},
            {QStringLiteral("text"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Text to write to clipboard (required for 'write').")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
    };
    return s;
}

ToolExecResult ClipboardTool::invoke(const QJsonObject &args)
{
    const QString op = args[QLatin1String("operation")].toString();
    QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard)
        return {false, {}, {}, QStringLiteral("Clipboard not available.")};

    if (op == QLatin1String("read")) {
        const QString text = clipboard->text();
        if (text.isEmpty())
            return {true, {}, QStringLiteral("(clipboard is empty)"), {}};
        // Limit to 64KB to prevent context overflow
        if (text.size() > 65536)
            return {true, {},
                text.left(65536) + QStringLiteral("\n... (truncated, total %1 chars)")
                    .arg(text.size()), {}};
        return {true, {}, text, {}};
    }

    if (op == QLatin1String("write")) {
        const QString text = args[QLatin1String("text")].toString();
        if (text.isEmpty())
            return {false, {}, {},
                QStringLiteral("'text' parameter is required for 'write'.")};
        clipboard->setText(text);
        return {true, {},
            QStringLiteral("Wrote %1 characters to clipboard.").arg(text.size()), {}};
    }

    if (op == QLatin1String("has_text")) {
        const bool has = clipboard->mimeData() && clipboard->mimeData()->hasText();
        return {true, {},
            has ? QStringLiteral("true") : QStringLiteral("false"), {}};
    }

    return {false, {}, {},
        QStringLiteral("Unknown operation: %1. Use: read, write, has_text").arg(op)};
}

#pragma once

#include "../itool.h"
#include "../../core/ifilesystem.h"

#include <QJsonArray>

// ── ApplyPatchTool ────────────────────────────────────────────────────────────
// Applies a text replacement (patch) to a file.
// The agent specifies the exact old text and new text.
// This is safer than full file writes because it verifies the old text exists.

class ApplyPatchTool : public ITool
{
public:
    explicit ApplyPatchTool(IFileSystem *fs) : m_fs(fs) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("apply_patch");
        s.description = QStringLiteral(
            "Apply a targeted text replacement in a file. Specify the exact "
            "old text to find and the new text to replace it with. The old text "
            "must match exactly (including whitespace). This is safer than "
            "write_file because it verifies the context before making changes.");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("path"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("File path to patch.")}
                }},
                {QStringLiteral("oldText"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("The exact text to find and replace. "
                                    "Must appear exactly once in the file.")}
                }},
                {QStringLiteral("newText"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("The replacement text.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("path"),
                QStringLiteral("oldText"),
                QStringLiteral("newText")
            }}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString path    = args[QLatin1String("path")].toString();
        const QString oldText = args[QLatin1String("oldText")].toString();
        const QString newText = args[QLatin1String("newText")].toString();

        if (path.isEmpty())
            return {false, {}, {}, QStringLiteral("Missing required parameter: path")};
        if (oldText.isEmpty())
            return {false, {}, {}, QStringLiteral("Missing required parameter: oldText")};

        if (!m_fs->exists(path))
            return {false, {}, {}, QStringLiteral("File not found: %1").arg(path)};

        // Read current content
        QString error;
        QString content = m_fs->readTextFile(path, &error);
        if (!error.isEmpty())
            return {false, {}, {}, error};

        // Verify old text exists and is unique
        const int count = content.count(oldText);
        if (count == 0) {
            return {false, {}, {},
                    QStringLiteral("The specified old text was not found in %1. "
                                   "Make sure the text matches exactly, "
                                   "including whitespace and indentation.").arg(path)};
        }
        if (count > 1) {
            return {false, {}, {},
                    QStringLiteral("The specified old text appears %1 times in %2. "
                                   "It must appear exactly once. Include more "
                                   "surrounding context to make it unique.").arg(count).arg(path)};
        }

        // Apply the replacement
        content.replace(oldText, newText);

        if (!m_fs->writeTextFile(path, content, &error))
            return {false, {}, {}, error};

        return {true, {},
                QStringLiteral("Successfully patched %1: replaced %2 chars with %3 chars.")
                    .arg(path).arg(oldText.size()).arg(newText.size()),
                {}};
    }

private:
    IFileSystem *m_fs;
};

#pragma once

#include "../itool.h"

#include <functional>

// ── FormatCodeTool ──────────────────────────────────────────────────────────
// Abstract code formatting interface. The agent calls this single tool;
// the backend dispatches to the correct formatter based on language/file
// extension (clang-format, prettier, black, rustfmt, gofmt, etc.).
//
// The host provides a CodeFormatter callback that:
//   1. Detects language from file extension (or explicit language param)
//   2. Finds the appropriate formatter (clang-format, prettier, etc.)
//   3. Respects project config (.clang-format, .prettierrc, .editorconfig)
//   4. Returns formatted text + what changed
//
// The agent never needs to know which formatter runs underneath.

class FormatCodeTool : public ITool
{
public:
    struct FormatResult {
        bool    ok;
        QString formatted;    // formatted text (empty if in-place file format)
        QString diff;         // unified diff of changes (empty if no changes)
        QString error;
        QString formatterUsed; // e.g. "clang-format 17", "prettier 3.2"
    };

    // formatter(filePath, content, language, options) → FormatResult
    //   filePath: used to detect language & find config; may be empty for raw text
    //   content:  text to format; if empty, reads from filePath
    //   language: override language detection ("cpp", "js", "py", etc.); empty = auto
    //   rangeStart/rangeEnd: 0 = format entire file, otherwise 1-based line range
    using CodeFormatter = std::function<FormatResult(
        const QString &filePath,
        const QString &content,
        const QString &language,
        int rangeStart,
        int rangeEnd)>;

    explicit FormatCodeTool(CodeFormatter formatter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    CodeFormatter m_formatter;
};

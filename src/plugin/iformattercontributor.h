#pragma once

#include <QList>
#include <QString>

// ── IFormatterContributor ─────────────────────────────────────────────────────
//
// Plugins implement this to provide code formatting for specific languages.
// The IDE calls format methods when the user triggers Format Document / Format
// Selection, or on format-on-save / format-on-type events.

struct TextEdit
{
    int     startLine;
    int     startColumn;
    int     endLine;
    int     endColumn;
    QString newText;
};

class IFormatterContributor
{
public:
    virtual ~IFormatterContributor() = default;

    /// Format the entire document.
    virtual QList<TextEdit> formatDocument(const QString &filePath,
                                          const QString &languageId,
                                          const QString &content,
                                          int tabSize,
                                          bool insertSpaces) = 0;

    /// Format a selection range.
    virtual QList<TextEdit> formatRange(const QString &filePath,
                                       const QString &languageId,
                                       const QString &content,
                                       int startLine, int endLine,
                                       int tabSize, bool insertSpaces)
    {
        Q_UNUSED(filePath);  Q_UNUSED(languageId); Q_UNUSED(content);
        Q_UNUSED(startLine); Q_UNUSED(endLine);
        Q_UNUSED(tabSize);   Q_UNUSED(insertSpaces);
        return {};
    }

    /// Format after a character is typed (for format-on-type).
    virtual QList<TextEdit> formatOnType(const QString &filePath,
                                        const QString &languageId,
                                        const QString &content,
                                        int line, int column,
                                        const QString &ch,
                                        int tabSize, bool insertSpaces)
    {
        Q_UNUSED(filePath);  Q_UNUSED(languageId); Q_UNUSED(content);
        Q_UNUSED(line);      Q_UNUSED(column);     Q_UNUSED(ch);
        Q_UNUSED(tabSize);   Q_UNUSED(insertSpaces);
        return {};
    }

    /// Which languages does this formatter support?
    virtual QStringList supportedLanguages() const = 0;

    /// Characters that trigger format-on-type (e.g., "}", ";", "\n").
    virtual QStringList triggerCharacters() const { return {}; }
};

#define EXORCIST_FORMATTER_CONTRIBUTOR_IID "org.exorcist.IFormatterContributor/1.0"
Q_DECLARE_INTERFACE(IFormatterContributor, EXORCIST_FORMATTER_CONTRIBUTOR_IID)

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QTextCharFormat>

// ── IEditorContributor ────────────────────────────────────────────────────────
//
// Plugins implement this to add editor decorations, code lenses,
// inlay hints, and other visual overlays on the text editor.
//
// The IDE calls these methods when an editor is opened or scrolled
// to the visible range. Implementations should be fast — the IDE
// calls them on the main thread during paint.

// ── Decoration types ──────────────────────────────────────────────────────────

struct EditorDecoration
{
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;

    enum Kind {
        Highlight,       // background color range
        InlineText,      // injected text (inlay hint)
        GutterIcon,      // icon in the gutter
        LineAnnotation,  // text after end of line (like git blame)
        Underline,       // wavy/solid underline
    };
    Kind kind = Highlight;

    QString text;            // for InlineText / LineAnnotation
    QString icon;            // for GutterIcon
    QString hoverMessage;    // tooltip on hover
    QTextCharFormat format;  // styling (foreground, background, font weight)
    QString cssClass;        // for non-C++ layers (maps to predefined styles)
};

struct CodeLens
{
    int     line;
    QString title;       // "3 references" / "Run Test"
    QString commandId;   // command to execute on click
    QJsonObject args;    // command arguments
};

class IEditorContributor
{
public:
    virtual ~IEditorContributor() = default;

    /// Return decorations for the given file within the visible range.
    /// Called when editor content changes or scrolls.
    virtual QList<EditorDecoration> provideDecorations(
        const QString &filePath,
        const QString &languageId,
        int firstVisibleLine,
        int lastVisibleLine) = 0;

    /// Return code lenses for the given file.
    /// Called when editor content changes.
    virtual QList<CodeLens> provideCodeLenses(
        const QString &filePath,
        const QString &languageId)
    {
        Q_UNUSED(filePath);
        Q_UNUSED(languageId);
        return {};
    }

    /// Languages this contributor is interested in.
    /// Empty = all languages.
    virtual QStringList supportedLanguages() const { return {}; }
};

#define EXORCIST_EDITOR_CONTRIBUTOR_IID "org.exorcist.IEditorContributor/1.0"
Q_DECLARE_INTERFACE(IEditorContributor, EXORCIST_EDITOR_CONTRIBUTOR_IID)

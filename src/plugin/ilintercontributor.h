#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// ── ILinterContributor ────────────────────────────────────────────────────────
//
// Plugins implement this to provide code diagnostics (linting) for
// specific languages. Results are merged with LSP diagnostics and
// displayed in the editor (wavy underlines + gutter markers).

struct LintDiagnostic
{
    int     startLine;
    int     startColumn;
    int     endLine;
    int     endColumn;
    QString message;
    QString source;      // "eslint", "clippy", "pylint"
    QString code;        // rule identifier

    enum Severity { Error, Warning, Info, Hint };
    Severity severity = Warning;

    // Optional: quick fix
    QString fixTitle;    // "Import missing module"
    QString fixEdit;     // replacement text (empty = no fix)
};

class ILinterContributor
{
public:
    virtual ~ILinterContributor() = default;

    /// Lint the given file content and return diagnostics.
    /// Called on a worker thread — safe to block.
    virtual QList<LintDiagnostic> lint(const QString &filePath,
                                      const QString &languageId,
                                      const QString &content) = 0;

    /// Languages this linter supports.
    virtual QStringList supportedLanguages() const = 0;

    /// Whether this linter runs on save only (true) or on every change (false).
    virtual bool runOnSaveOnly() const { return false; }
};

#define EXORCIST_LINTER_CONTRIBUTOR_IID "org.exorcist.ILinterContributor/1.0"
Q_DECLARE_INTERFACE(ILinterContributor, EXORCIST_LINTER_CONTRIBUTOR_IID)

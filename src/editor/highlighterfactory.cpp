#include "highlighterfactory.h"
#include "syntaxhighlighter.h"
#include "treesitterhighlighter.h"

#include <QFileInfo>

#ifdef EXORCIST_HAS_TREESITTER
#include <tree_sitter/api.h>

// Extern declarations for tree-sitter language functions
extern "C" {
const TSLanguage *tree_sitter_c();
const TSLanguage *tree_sitter_cpp();
const TSLanguage *tree_sitter_python();
const TSLanguage *tree_sitter_javascript();
const TSLanguage *tree_sitter_typescript();
const TSLanguage *tree_sitter_rust();
const TSLanguage *tree_sitter_json();
}

/// Maps file extension to tree-sitter language.
/// Returns nullptr if no grammar is available.
static const TSLanguage *languageForExtension(const QString &ext)
{
    // C
    if (ext == QLatin1String("c"))
        return tree_sitter_c();

    // C++
    if (ext == QLatin1String("cpp") || ext == QLatin1String("cxx") ||
        ext == QLatin1String("cc")  || ext == QLatin1String("h")   ||
        ext == QLatin1String("hpp") || ext == QLatin1String("hxx") ||
        ext == QLatin1String("inl") || ext == QLatin1String("mm"))
        return tree_sitter_cpp();

    // Python
    if (ext == QLatin1String("py") || ext == QLatin1String("pyw"))
        return tree_sitter_python();

    // JavaScript
    if (ext == QLatin1String("js")  || ext == QLatin1String("mjs") ||
        ext == QLatin1String("cjs") || ext == QLatin1String("jsx"))
        return tree_sitter_javascript();

    // TypeScript
    if (ext == QLatin1String("ts") || ext == QLatin1String("tsx"))
        return tree_sitter_typescript();

    // Rust
    if (ext == QLatin1String("rs"))
        return tree_sitter_rust();

    // JSON
    if (ext == QLatin1String("json") || ext == QLatin1String("jsonc"))
        return tree_sitter_json();

    return nullptr;
}
#endif

QSyntaxHighlighter *HighlighterFactory::create(const QString &filePath, QTextDocument *doc)
{
    if (filePath.isEmpty() || !doc)
        return nullptr;

#ifdef EXORCIST_HAS_TREESITTER
    const QString ext = QFileInfo(filePath).suffix().toLower();
    const TSLanguage *lang = languageForExtension(ext);
    if (lang) {
        auto *hl = new TreeSitterHighlighter(doc);
        hl->setLanguage(lang);
        return hl;
    }
#endif

    // Fall back to regex highlighter for unsupported languages
    return SyntaxHighlighter::create(filePath, doc);
}

bool HighlighterFactory::hasTreeSitter()
{
#ifdef EXORCIST_HAS_TREESITTER
    return true;
#else
    return false;
#endif
}

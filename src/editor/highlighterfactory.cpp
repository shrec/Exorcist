#include "highlighterfactory.h"
#include "syntaxhighlighter.h"
#include "treesitterhighlighter.h"

#include <QFileInfo>

HighlighterFactory::LanguageLookupFn HighlighterFactory::s_languageLookup;

void HighlighterFactory::setLanguageLookup(LanguageLookupFn fn)
{
    s_languageLookup = std::move(fn);
}

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
const TSLanguage *tree_sitter_go();
const TSLanguage *tree_sitter_yaml();
const TSLanguage *tree_sitter_toml();
}

/// Maps a language ID (from ContributionRegistry) to its tree-sitter grammar.
/// Returns nullptr if no grammar is compiled in for this language ID.
static const TSLanguage *tsLanguageForId(const QString &id)
{
    if (id == QLatin1String("c"))          return tree_sitter_c();
    if (id == QLatin1String("cpp"))        return tree_sitter_cpp();
    if (id == QLatin1String("python"))     return tree_sitter_python();
    if (id == QLatin1String("javascript")) return tree_sitter_javascript();
    if (id == QLatin1String("typescript")) return tree_sitter_typescript();
    if (id == QLatin1String("rust"))       return tree_sitter_rust();
    if (id == QLatin1String("json"))       return tree_sitter_json();
    if (id == QLatin1String("go"))         return tree_sitter_go();
    if (id == QLatin1String("yaml"))       return tree_sitter_yaml();
    if (id == QLatin1String("toml"))       return tree_sitter_toml();
    return nullptr;
}

/// Maps file extension to tree-sitter language (hard-coded fallback).
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

    // Go
    if (ext == QLatin1String("go"))
        return tree_sitter_go();

    // YAML
    if (ext == QLatin1String("yaml") || ext == QLatin1String("yml"))
        return tree_sitter_yaml();

    // TOML
    if (ext == QLatin1String("toml"))
        return tree_sitter_toml();

    return nullptr;
}
#endif

QSyntaxHighlighter *HighlighterFactory::create(const QString &filePath, QTextDocument *doc)
{
    if (filePath.isEmpty() || !doc)
        return nullptr;

#ifdef EXORCIST_HAS_TREESITTER
    const QString ext = QFileInfo(filePath).suffix().toLower();

    const TSLanguage *lang = nullptr;

    // 1. Plugin-registered languages take precedence
    if (s_languageLookup) {
        const QString langId = s_languageLookup(ext);
        if (!langId.isEmpty())
            lang = tsLanguageForId(langId);
    }

    // 2. Hard-coded fallback (graceful degradation when no plugins loaded)
    if (!lang)
        lang = languageForExtension(ext);

    if (lang) {
        auto *hl = new TreeSitterHighlighter(doc);
        hl->setLanguage(lang);
        return hl;
    }
#endif

    // 3. Fall back to regex highlighter for unsupported languages
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

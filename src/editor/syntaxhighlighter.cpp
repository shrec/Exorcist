#include "syntaxhighlighter.h"

#include <QFileInfo>
#include <memory>

// ── Colours (VS Code dark theme palette) ────────────────────────────────────

namespace clr
{
static QTextCharFormat fmt(const QColor &fg, bool bold = false)
{
    QTextCharFormat f;
    f.setForeground(fg);
    if (bold)
        f.setFontWeight(QFont::Bold);
    return f;
}

static const QTextCharFormat keyword   = fmt(QColor(0x56, 0x9C, 0xD6), true);  // blue
static const QTextCharFormat type_     = fmt(QColor(0x4E, 0xC9, 0xB0));         // teal
static const QTextCharFormat string_   = fmt(QColor(0xCE, 0x91, 0x78));         // orange
static const QTextCharFormat comment_  = fmt(QColor(0x6A, 0x99, 0x55));         // green
static const QTextCharFormat number_   = fmt(QColor(0xB5, 0xCE, 0xA8));         // light green
static const QTextCharFormat preproc_  = fmt(QColor(0xC5, 0x86, 0xC0));         // purple
static const QTextCharFormat function_ = fmt(QColor(0xDC, 0xDC, 0xAA));         // yellow
static const QTextCharFormat special_  = fmt(QColor(0x9C, 0xDC, 0xFE));         // light blue
} // namespace clr

// ── Helpers ──────────────────────────────────────────────────────────────────

static void addKeywords(QVector<SyntaxHighlighter::Rule> &rules,
                        const QStringList &words,
                        const QTextCharFormat &fmt)
{
    for (const QString &w : words) {
        SyntaxHighlighter::Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\\b") + w + QStringLiteral("\\b"));
        r.format  = fmt;
        rules.append(r);
    }
}

static void addRule(QVector<SyntaxHighlighter::Rule> &rules,
                    const QString &pattern,
                    const QTextCharFormat &fmt)
{
    SyntaxHighlighter::Rule r;
    r.pattern = QRegularExpression(pattern);
    r.format  = fmt;
    rules.append(r);
}

// ── Language builders ────────────────────────────────────────────────────────

static void buildCpp(SyntaxHighlighter *h,
                     QVector<SyntaxHighlighter::Rule> &rules,
                     QRegularExpression &blockStart,
                     QRegularExpression &blockEnd,
                     QTextCharFormat &blockFmt)
{
    // Preprocessor
    addRule(rules, R"(^\s*#\s*\w+.*)", clr::preproc_);

    // Keywords
    addKeywords(rules,
                {"alignas", "alignof", "and", "and_eq", "asm", "auto",
                 "bitand", "bitor", "bool", "break", "case", "catch",
                 "char", "char8_t", "char16_t", "char32_t", "class",
                 "compl", "concept", "const", "consteval", "constexpr",
                 "constinit", "const_cast", "continue", "co_await",
                 "co_return", "co_yield", "decltype", "default", "delete",
                 "do", "double", "dynamic_cast", "else", "enum", "explicit",
                 "export", "extern", "false", "float", "for", "friend",
                 "goto", "if", "inline", "int", "long", "mutable",
                 "namespace", "new", "noexcept", "not", "not_eq", "nullptr",
                 "operator", "or", "or_eq", "override", "private",
                 "protected", "public", "register", "reinterpret_cast",
                 "requires", "return", "short", "signed", "sizeof",
                 "static", "static_assert", "static_cast", "struct",
                 "switch", "template", "this", "thread_local", "throw",
                 "true", "try", "typedef", "typeid", "typename", "union",
                 "unsigned", "using", "virtual", "void", "volatile",
                 "wchar_t", "while", "xor", "xor_eq"},
                clr::keyword);

    // Common types / standard names
    addKeywords(rules,
                {"QString", "QStringList", "QObject", "QWidget", "QList",
                 "QVector", "QMap", "QHash", "QSet", "QSharedPointer",
                 "std", "size_t", "uint8_t", "uint16_t", "uint32_t",
                 "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t",
                 "ptrdiff_t", "nullptr_t", "string", "vector", "map",
                 "unordered_map", "set", "unordered_set", "pair", "tuple",
                 "unique_ptr", "shared_ptr", "weak_ptr", "optional",
                 "variant", "any", "function", "array", "deque", "list"},
                clr::type_);

    // Function calls:  identifier(
    addRule(rules, R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*(?=\())", clr::function_);

    // Strings & chars
    addRule(rules, R"("(?:[^"\\]|\\.)*")", clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')", clr::string_);

    // Numbers: hex, binary, float, int
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+[uUlL]*\b)", clr::number_);
    addRule(rules, R"(\b0[bB][01]+[uUlL]*\b)",         clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?[fFlL]?\b)", clr::number_);

    // Single-line comments (applied last so they override everything)
    addRule(rules, R"(//[^\n]*)", clr::comment_);

    // Block comments
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;

    Q_UNUSED(h)
}

static void buildPython(QVector<SyntaxHighlighter::Rule> &rules,
                        QRegularExpression &blockStart,
                        QRegularExpression &blockEnd,
                        QTextCharFormat &blockFmt)
{
    // Decorators
    addRule(rules, R"(@[A-Za-z_][A-Za-z0-9_.]*)", clr::preproc_);

    // Keywords
    addKeywords(rules,
                {"False", "None", "True", "and", "as", "assert", "async",
                 "await", "break", "class", "continue", "def", "del",
                 "elif", "else", "except", "finally", "for", "from",
                 "global", "if", "import", "in", "is", "lambda",
                 "nonlocal", "not", "or", "pass", "raise", "return",
                 "try", "while", "with", "yield"},
                clr::keyword);

    // Built-in types
    addKeywords(rules,
                {"bool", "bytearray", "bytes", "complex", "dict", "float",
                 "frozenset", "int", "list", "memoryview", "object",
                 "range", "set", "str", "tuple", "type"},
                clr::type_);

    // Built-in functions
    addKeywords(rules,
                {"abs", "all", "any", "bin", "callable", "chr", "dir",
                 "divmod", "enumerate", "eval", "exec", "filter", "format",
                 "getattr", "globals", "hasattr", "hash", "help", "hex",
                 "id", "input", "isinstance", "issubclass", "iter", "len",
                 "locals", "map", "max", "min", "next", "oct", "open",
                 "ord", "pow", "print", "property", "repr", "reversed",
                 "round", "setattr", "slice", "sorted", "staticmethod",
                 "sum", "super", "vars", "zip"},
                clr::function_);

    // Triple-quoted strings (mark as block-comment-like, state 1 = triple-double, 2 = triple-single)
    addRule(rules, R"(""".*?""")", clr::string_);
    addRule(rules, R"('''.*?''')", clr::string_);

    // Regular strings
    addRule(rules, R"(f?"(?:[^"\\]|\\.)*")", clr::string_);
    addRule(rules, R"(f?'(?:[^'\\]|\\.)*')", clr::string_);
    addRule(rules, R"(r"[^"]*")",             clr::string_);
    addRule(rules, R"(r'[^']*')",             clr::string_);

    // Numbers
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+\b)", clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?[jJ]?\b)", clr::number_);

    // Line comment (last, so it wins)
    addRule(rules, R"(#[^\n]*)", clr::comment_);

    // Python uses # for comments, no C-style block comments.
    // Use triple-quote as block comment delimiter via states:
    blockStart = QRegularExpression(R"(""")");
    blockEnd   = QRegularExpression(R"(""")");
    blockFmt   = clr::string_;
}

static void buildJson(QVector<SyntaxHighlighter::Rule> &rules)
{
    // Keys:  "someKey" :
    addRule(rules, R"("(?:[^"\\]|\\.)*"\s*:)", clr::special_);
    // String values
    addRule(rules, R"("(?:[^"\\]|\\.)*")",     clr::string_);
    // Keywords
    addKeywords(rules, {"true", "false", "null"}, clr::keyword);
    // Numbers
    addRule(rules, R"(-?[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)", clr::number_);
}

static void buildCmake(QVector<SyntaxHighlighter::Rule> &rules)
{
    // Commands
    addKeywords(rules,
                {"add_executable", "add_library", "add_subdirectory",
                 "cmake_minimum_required", "find_package", "if", "elseif",
                 "else", "endif", "foreach", "endforeach", "function",
                 "endfunction", "macro", "endmacro", "include", "message",
                 "option", "project", "set", "string", "target_compile_definitions",
                 "target_compile_options", "target_include_directories",
                 "target_link_libraries", "target_sources", "while",
                 "endwhile", "return", "list", "file", "get_filename_component",
                 "get_target_property", "add_custom_command",
                 "add_custom_target", "install", "configure_file",
                 "find_path", "find_library", "find_program"},
                clr::keyword);

    // Variables ${...}
    addRule(rules, R"(\$\{[^}]*\})", clr::special_);

    // Strings
    addRule(rules, R"("(?:[^"\\]|\\.)*")", clr::string_);

    // Comment
    addRule(rules, R"(#[^\n]*)", clr::comment_);
}

static void buildMarkdown(QVector<SyntaxHighlighter::Rule> &rules)
{
    // Headings
    addRule(rules, R"(^#{1,6}\s.*$)", clr::keyword);

    // Bold **text** or __text__
    addRule(rules, R"(\*\*[^*]+\*\*|__[^_]+__)", clr::type_);

    // Italic *text* or _text_
    addRule(rules, R"(\*[^*]+\*|_[^_]+_)", clr::function_);

    // Inline code `code`
    addRule(rules, R"(`[^`]+`)", clr::string_);

    // Links [text](url)
    addRule(rules, R"(\[[^\]]*\]\([^)]*\))", clr::special_);

    // Blockquotes
    addRule(rules, R"(^>\s.*$)", clr::comment_);

    // Horizontal rules
    addRule(rules, R"(^[-*_]{3,}\s*$)", clr::preproc_);

    // List markers
    addRule(rules, R"(^\s*[-*+]\s)", clr::preproc_);
    addRule(rules, R"(^\s*[0-9]+\.\s)", clr::preproc_);
}

static void buildCsharp(QVector<SyntaxHighlighter::Rule> &rules,
                        QRegularExpression &blockStart,
                        QRegularExpression &blockEnd,
                        QTextCharFormat &blockFmt)
{
    // Preprocessor directives
    addRule(rules, R"(^\s*#\s*(if|else|elif|endif|define|undef|region|endregion|pragma|warning|error|nullable)\b.*)", clr::preproc_);

    addKeywords(rules,
                {"abstract", "as", "base", "bool", "break", "byte", "case",
                 "catch", "char", "checked", "class", "const", "continue",
                 "decimal", "default", "delegate", "do", "double", "else",
                 "enum", "event", "explicit", "extern", "false", "finally",
                 "fixed", "float", "for", "foreach", "goto", "if", "implicit",
                 "in", "int", "interface", "internal", "is", "lock", "long",
                 "namespace", "new", "null", "object", "operator", "out",
                 "override", "params", "private", "protected", "public",
                 "readonly", "ref", "return", "sbyte", "sealed", "short",
                 "sizeof", "stackalloc", "static", "string", "struct",
                 "switch", "this", "throw", "true", "try", "typeof", "uint",
                 "ulong", "unchecked", "unsafe", "ushort", "using", "virtual",
                 "void", "volatile", "while", "async", "await", "var",
                 "dynamic", "yield", "get", "set", "init", "value", "add",
                 "remove", "partial", "record", "with", "required", "file",
                 "scoped", "nint", "nuint"},
                clr::keyword);

    // Attributes [Foo]
    addRule(rules, R"(\[[A-Za-z_][A-Za-z0-9_.,\s]*\])", clr::preproc_);

    addRule(rules, R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*(?=\())", clr::function_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                   clr::string_);
    addRule(rules, R"(@"[^"]*")",                             clr::string_);  // verbatim
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                   clr::string_);
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+[uUlL]*\b)",       clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?[mMfFdDuUlL]*\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)",                             clr::comment_);

    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildJava(QVector<SyntaxHighlighter::Rule> &rules,
                      QRegularExpression &blockStart,
                      QRegularExpression &blockEnd,
                      QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"abstract", "assert", "boolean", "break", "byte", "case",
                 "catch", "char", "class", "const", "continue", "default",
                 "do", "double", "else", "enum", "extends", "final",
                 "finally", "float", "for", "goto", "if", "implements",
                 "import", "instanceof", "int", "interface", "long",
                 "native", "new", "null", "package", "private", "protected",
                 "public", "return", "short", "static", "strictfp", "super",
                 "switch", "synchronized", "this", "throw", "throws",
                 "transient", "try", "var", "void", "volatile", "while",
                 "false", "true", "record", "sealed", "permits", "yield"},
                clr::keyword);

    // Annotations
    addRule(rules, R"(@[A-Za-z_][A-Za-z0-9_.]*)", clr::preproc_);

    addRule(rules, R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*(?=\())", clr::function_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                   clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                   clr::string_);
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+[lL]?\b)",         clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?[fFdDlL]?\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)",                             clr::comment_);

    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildDart(QVector<SyntaxHighlighter::Rule> &rules,
                      QRegularExpression &blockStart,
                      QRegularExpression &blockEnd,
                      QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"abstract", "as", "assert", "async", "await", "base",
                 "break", "case", "catch", "class", "const", "continue",
                 "covariant", "default", "deferred", "do", "dynamic",
                 "else", "enum", "export", "extends", "extension",
                 "external", "factory", "false", "final", "finally", "for",
                 "Function", "get", "hide", "if", "implements", "import",
                 "in", "interface", "is", "late", "library", "mixin", "new",
                 "null", "of", "on", "operator", "part", "required",
                 "rethrow", "return", "sealed", "set", "show", "static",
                 "super", "switch", "sync", "this", "throw", "true", "try",
                 "typedef", "var", "void", "when", "while", "with", "yield"},
                clr::keyword);

    addKeywords(rules,
                {"bool", "double", "int", "num", "String", "List", "Map",
                 "Set", "Iterable", "Future", "Stream", "Object", "Never",
                 "Null", "Symbol", "Type", "Record"},
                clr::type_);

    // String interpolation $expr or ${expr}
    addRule(rules, R"(\$\{[^}]*\}|\$[A-Za-z_][A-Za-z0-9_]*)", clr::special_);

    addRule(rules, R"(r?"(?:[^"\\]|\\.)*")",  clr::string_);
    addRule(rules, R"(r?'(?:[^'\\]|\\.)*')",  clr::string_);
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+\b)", clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)",              clr::comment_);

    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildXml(QVector<SyntaxHighlighter::Rule> &rules,
                     QRegularExpression &blockStart,
                     QRegularExpression &blockEnd,
                     QTextCharFormat &blockFmt)
{
    // CDATA
    addRule(rules, R"(<!\[CDATA\[[\s\S]*?\]\]>)", clr::string_);
    // Tag names
    addRule(rules, R"(</?[A-Za-z_][A-Za-z0-9_:.-]*)", clr::keyword);
    // Closing >
    addRule(rules, R"(/?>)", clr::keyword);
    // Attribute names
    addRule(rules, R"([A-Za-z_][A-Za-z0-9_:.-]*\s*=)", clr::special_);
    // Attribute values
    addRule(rules, R"("[^"]*"|'[^']*')", clr::string_);
    // Entities
    addRule(rules, R"(&[A-Za-z0-9#]+;)", clr::preproc_);
    // DOCTYPE / processing instructions
    addRule(rules, R"(<[?!][^>]*>)", clr::preproc_);

    // XML comments <!-- ... -->
    blockStart = QRegularExpression(R"(<!--)");
    blockEnd   = QRegularExpression(R"(-->)");
    blockFmt   = clr::comment_;
}

static void buildSql(QVector<SyntaxHighlighter::Rule> &rules)
{
    addKeywords(rules,
                {"ADD", "ALL", "ALTER", "AND", "AS", "ASC", "BETWEEN",
                 "BY", "CASE", "CHECK", "COLUMN", "CONSTRAINT", "CREATE",
                 "CROSS", "DATABASE", "DEFAULT", "DELETE", "DESC",
                 "DISTINCT", "DROP", "ELSE", "END", "EXCEPT", "EXISTS",
                 "FOREIGN", "FROM", "FULL", "GROUP", "HAVING", "IN",
                 "INDEX", "INNER", "INSERT", "INTERSECT", "INTO", "IS",
                 "JOIN", "KEY", "LEFT", "LIKE", "LIMIT", "NOT", "NULL",
                 "ON", "OR", "ORDER", "OUTER", "PRIMARY", "REFERENCES",
                 "RIGHT", "ROWNUM", "SELECT", "SET", "TABLE", "THEN",
                 "TOP", "TRUNCATE", "UNION", "UNIQUE", "UPDATE", "VALUES",
                 "VIEW", "WHEN", "WHERE", "WITH",
                 // lowercase variants
                 "add", "all", "alter", "and", "as", "asc", "between",
                 "by", "case", "check", "column", "constraint", "create",
                 "cross", "database", "default", "delete", "desc",
                 "distinct", "drop", "else", "end", "except", "exists",
                 "foreign", "from", "full", "group", "having", "in",
                 "index", "inner", "insert", "intersect", "into", "is",
                 "join", "key", "left", "like", "limit", "not", "null",
                 "on", "or", "order", "outer", "primary", "references",
                 "right", "rownum", "select", "set", "table", "then",
                 "top", "truncate", "union", "unique", "update", "values",
                 "view", "when", "where", "with"},
                clr::keyword);

    // Types
    addKeywords(rules,
                {"INT", "INTEGER", "BIGINT", "SMALLINT", "TINYINT",
                 "FLOAT", "DOUBLE", "DECIMAL", "NUMERIC", "REAL",
                 "CHAR", "VARCHAR", "TEXT", "NCHAR", "NVARCHAR",
                 "DATE", "DATETIME", "TIMESTAMP", "TIME", "BOOLEAN",
                 "BLOB", "CLOB", "BINARY", "VARBINARY", "UUID",
                 "int", "integer", "bigint", "smallint", "tinyint",
                 "float", "double", "decimal", "numeric", "real",
                 "char", "varchar", "text", "nchar", "nvarchar",
                 "date", "datetime", "timestamp", "time", "boolean",
                 "blob", "clob", "binary", "varbinary", "uuid"},
                clr::type_);

    // Functions
    addKeywords(rules,
                {"COUNT", "SUM", "AVG", "MIN", "MAX", "COALESCE",
                 "NULLIF", "CAST", "CONVERT", "IIF", "ISNULL",
                 "NOW", "GETDATE", "CURRENT_TIMESTAMP", "YEAR", "MONTH",
                 "DAY", "CONCAT", "LENGTH", "LEN", "SUBSTR", "SUBSTRING",
                 "TRIM", "LTRIM", "RTRIM", "UPPER", "LOWER", "REPLACE",
                 "ROUND", "FLOOR", "CEILING", "ABS",
                 "count", "sum", "avg", "min", "max", "coalesce",
                 "nullif", "cast", "convert", "iif", "isnull",
                 "now", "getdate", "current_timestamp", "year", "month",
                 "day", "concat", "length", "len", "substr", "substring",
                 "trim", "ltrim", "rtrim", "upper", "lower", "replace",
                 "round", "floor", "ceiling", "abs"},
                clr::function_);

    addRule(rules, R"('(?:[^'\\]|\\.)*')",                    clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)",              clr::number_);
    // Single-line comments: -- ...
    addRule(rules, R"(--[^\n]*)",                              clr::comment_);
}

static void buildPhp(QVector<SyntaxHighlighter::Rule> &rules,
                     QRegularExpression &blockStart,
                     QRegularExpression &blockEnd,
                     QTextCharFormat &blockFmt)
{
    // PHP open/close tags
    addRule(rules, R"(<\?(php|=)?|\?>)", clr::preproc_);

    addKeywords(rules,
                {"abstract", "and", "array", "as", "break", "callable",
                 "case", "catch", "class", "clone", "const", "continue",
                 "declare", "default", "die", "do", "echo", "else",
                 "elseif", "empty", "enddeclare", "endfor", "endforeach",
                 "endif", "endswitch", "endwhile", "eval", "exit",
                 "extends", "final", "finally", "fn", "for", "foreach",
                 "function", "global", "goto", "if", "implements",
                 "include", "include_once", "instanceof", "insteadof",
                 "interface", "isset", "list", "match", "namespace",
                 "new", "or", "print", "private", "protected", "public",
                 "readonly", "require", "require_once", "return", "static",
                 "switch", "throw", "trait", "try", "unset", "use",
                 "var", "while", "xor", "yield", "false", "true", "null",
                 "self", "parent", "this"},
                clr::keyword);

    // Variables: $var
    addRule(rules, R"(\$[A-Za-z_][A-Za-z0-9_]*)", clr::special_);

    addRule(rules, R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*(?=\())", clr::function_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                    clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                    clr::string_);
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+\b)",               clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)",              clr::number_);
    addRule(rules, R"(//[^\n]*|#[^\n]*)",                      clr::comment_);

    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildJavaScript(QVector<SyntaxHighlighter::Rule> &rules,
                            QRegularExpression &blockStart,
                            QRegularExpression &blockEnd,
                            QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"async", "await", "break", "case", "catch", "class",
                 "const", "continue", "debugger", "default", "delete",
                 "do", "else", "export", "extends", "false", "finally",
                 "for", "from", "function", "get", "if", "import", "in",
                 "instanceof", "let", "new", "null", "of", "return",
                 "set", "static", "super", "switch", "this", "throw",
                 "true", "try", "typeof", "undefined", "var", "void",
                 "while", "with", "yield"},
                clr::keyword);

    addKeywords(rules,
                {"Array", "Boolean", "Date", "Error", "Function", "JSON",
                 "Math", "Map", "Number", "Object", "Promise", "RegExp",
                 "Set", "String", "Symbol", "WeakMap", "WeakSet",
                 "console", "document", "window", "globalThis",
                 "parseInt", "parseFloat", "isNaN", "isFinite",
                 "setTimeout", "setInterval", "clearTimeout", "clearInterval",
                 "fetch", "require", "module", "exports"},
                clr::type_);

    addRule(rules, R"(`(?:[^`\\]|\\.)*`)",                    clr::string_);  // template literals
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                    clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                    clr::string_);
    addRule(rules, R"(\b([A-Za-z_$][A-Za-z0-9_$]*)\s*(?=\())", clr::function_);
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+n?\b)",              clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?n?\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)",                              clr::comment_);

    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildRust(QVector<SyntaxHighlighter::Rule> &rules,
                      QRegularExpression &blockStart,
                      QRegularExpression &blockEnd,
                      QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"as", "async", "await", "break", "const", "continue",
                 "crate", "dyn", "else", "enum", "extern", "false", "fn",
                 "for", "if", "impl", "in", "let", "loop", "match",
                 "mod", "move", "mut", "pub", "ref", "return", "self",
                 "Self", "static", "struct", "super", "trait", "true",
                 "type", "union", "unsafe", "use", "where", "while",
                 "abstract", "become", "box", "do", "final", "macro",
                 "override", "priv", "typeof", "unsized", "virtual",
                 "yield"},
                clr::keyword);

    addKeywords(rules,
                {"bool", "char", "f32", "f64", "i8", "i16", "i32",
                 "i64", "i128", "isize", "str", "u8", "u16", "u32",
                 "u64", "u128", "usize", "String", "Vec", "Option",
                 "Result", "Box", "Rc", "Arc", "Cell", "RefCell",
                 "HashMap", "HashSet", "BTreeMap", "BTreeSet"},
                clr::type_);

    // Macro invocations
    addRule(rules, R"(\b[A-Za-z_][A-Za-z0-9_]*!)", clr::preproc_);
    // Lifetime annotations
    addRule(rules, R"('[a-z_][a-z0-9_]*\b)", clr::special_);

    addRule(rules, R"(r#?"(?:[^"\\]|\\.)*"#?)",               clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)')",                      clr::string_);
    addRule(rules, R"(\b0[xX][0-9A-Fa-f_]+(?:_?[uif][0-9]+)?\b)", clr::number_);
    addRule(rules, R"(\b0[bB][01_]+(?:_?[uif][0-9]+)?\b)",    clr::number_);
    addRule(rules, R"(\b[0-9][0-9_]*(?:\.[0-9_]+)?(?:[eE][+-]?[0-9_]+)?(?:_?[uif][0-9]+)?\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)",                              clr::comment_);

    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildGo(QVector<SyntaxHighlighter::Rule> &rules,
                    QRegularExpression &blockStart,
                    QRegularExpression &blockEnd,
                    QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"break", "case", "chan", "const", "continue", "default",
                 "defer", "else", "fallthrough", "for", "func", "go",
                 "goto", "if", "import", "interface", "map", "package",
                 "range", "return", "select", "struct", "switch", "type",
                 "var"},
                clr::keyword);
    addKeywords(rules,
                {"bool", "byte", "complex64", "complex128", "error",
                 "float32", "float64", "int", "int8", "int16", "int32",
                 "int64", "rune", "string", "uint", "uint8", "uint16",
                 "uint32", "uint64", "uintptr", "any", "comparable"},
                clr::type_);
    addKeywords(rules,
                {"append", "cap", "clear", "close", "copy", "delete",
                 "len", "make", "max", "min", "new", "panic", "print",
                 "println", "recover", "true", "false", "nil", "iota"},
                clr::function_);
    addRule(rules, R"(`[^`]*`)",                           clr::string_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                 clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                 clr::string_);
    addRule(rules, R"(\b0[xX][0-9A-Fa-f_]+\b)",           clr::number_);
    addRule(rules, R"(\b[0-9][0-9_]*(?:\.[0-9_]+)?(?:[eE][+-]?[0-9_]+)?[iI]?\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)",                          clr::comment_);
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildKotlin(QVector<SyntaxHighlighter::Rule> &rules,
                        QRegularExpression &blockStart,
                        QRegularExpression &blockEnd,
                        QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"abstract", "actual", "annotation", "as", "break", "by",
                 "catch", "class", "companion", "const", "constructor",
                 "continue", "crossinline", "data", "delegate", "do",
                 "dynamic", "else", "enum", "expect", "external", "false",
                 "field", "file", "final", "finally", "for", "fun", "get",
                 "if", "import", "in", "infix", "init", "inline",
                 "inner", "interface", "internal", "is", "it", "lateinit",
                 "noinline", "null", "object", "open", "operator",
                 "out", "override", "package", "param", "private",
                 "property", "protected", "public", "receiver", "reified",
                 "return", "sealed", "set", "setparam", "super",
                 "suspend", "tailrec", "this", "throw", "true", "try",
                 "typealias", "typeof", "val", "value", "var", "vararg",
                 "when", "where", "while"},
                clr::keyword);
    addKeywords(rules,
                {"Any", "Boolean", "Byte", "Char", "Double", "Float",
                 "Int", "Long", "Nothing", "Number", "Short", "String",
                 "Unit", "Array", "List", "Map", "Set", "MutableList",
                 "MutableMap", "MutableSet", "Pair", "Triple"},
                clr::type_);
    addRule(rules, R"(@[A-Za-z_][A-Za-z0-9_]*)",          clr::preproc_);
    addRule(rules, R"(\$\{[^}]*\}|\$[A-Za-z_][A-Za-z0-9_]*)", clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                 clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                 clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?[fFdDlL]?\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)",                          clr::comment_);
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildSwift(QVector<SyntaxHighlighter::Rule> &rules,
                       QRegularExpression &blockStart,
                       QRegularExpression &blockEnd,
                       QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"actor", "any", "as", "associatedtype", "associativity",
                 "async", "await", "break", "case", "catch", "class",
                 "continue", "convenience", "default", "defer", "deinit",
                 "didSet", "do", "dynamic", "else", "enum", "extension",
                 "fallthrough", "false", "fileprivate", "final", "for",
                 "func", "get", "guard", "if", "import", "in", "indirect",
                 "infix", "init", "inout", "internal", "is", "isolated",
                 "lazy", "left", "let", "mutating", "nil", "none",
                 "nonmutating", "open", "operator", "optional", "override",
                 "postfix", "precedence", "prefix", "private", "protocol",
                 "public", "repeat", "required", "rethrows", "return",
                 "right", "self", "Self", "set", "some", "static",
                 "struct", "subscript", "super", "switch", "throws",
                 "true", "try", "Type", "typealias", "unowned", "var",
                 "weak", "where", "while", "willSet"},
                clr::keyword);
    addKeywords(rules,
                {"Bool", "Character", "Double", "Float", "Float16",
                 "Float80", "Int", "Int8", "Int16", "Int32", "Int64",
                 "String", "UInt", "UInt8", "UInt16", "UInt32", "UInt64",
                 "Array", "Dictionary", "Optional", "Result", "Set"},
                clr::type_);
    addRule(rules, R"(#[A-Za-z]+\b)",                      clr::preproc_);  // #if #else #available
    addRule(rules, R"(@[A-Za-z_][A-Za-z0-9_]*)",           clr::preproc_);
    addRule(rules, R"(\\\([^)]*\))",                        clr::special_);  // string interpolation
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                  clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?[fF]?\b)",      clr::number_);
    addRule(rules, R"(//[^\n]*)",                           clr::comment_);
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildShell(QVector<SyntaxHighlighter::Rule> &rules)
{
    addKeywords(rules,
                {"case", "do", "done", "elif", "else", "esac", "fi",
                 "for", "function", "if", "in", "local", "return",
                 "select", "then", "time", "until", "while"},
                clr::keyword);
    addKeywords(rules,
                {"echo", "exit", "export", "read", "source", "shift",
                 "set", "unset", "exec", "eval", "test", "printf",
                 "cd", "pwd", "ls", "cp", "mv", "rm", "mkdir", "touch",
                 "cat", "grep", "sed", "awk", "find", "sort", "curl",
                 "wget", "chmod", "chown", "kill", "ps", "env", "true",
                 "false"},
                clr::function_);
    // Variables $VAR ${VAR}
    addRule(rules, R"(\$\{?[A-Za-z_][A-Za-z0-9_]*\}?)", clr::special_);
    // Special $? $# $@ $$ $0
    addRule(rules, R"(\$[@#?$!0-9*\-])",                  clr::special_);
    // Strings
    addRule(rules, R"("(?:[^"\\$]|\\.|\$[^{])*")",        clr::string_);
    addRule(rules, R"('[^']*')",                           clr::string_);
    // Back-tick command substitution
    addRule(rules, R"(`[^`]*`)",                           clr::preproc_);
    // Shebang
    addRule(rules, R"(^#![^\n]*)",                         clr::preproc_);
    addRule(rules, R"(#[^\n]*)",                           clr::comment_);
}

static void buildPowerShell(QVector<SyntaxHighlighter::Rule> &rules,
                            QRegularExpression &blockStart,
                            QRegularExpression &blockEnd,
                            QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"Begin", "Break", "Catch", "Class", "Continue", "Data",
                 "Define", "Do", "DynamicParam", "Else", "ElseIf", "End",
                 "Enum", "Exit", "Filter", "Finally", "For", "ForEach",
                 "From", "Function", "Hidden", "If", "In", "InlineScript",
                 "Parallel", "Param", "Process", "Return", "Sequence",
                 "Static", "Switch", "Throw", "Trap", "Try", "Until",
                 "Using", "Var", "While", "Workflow",
                 "begin", "break", "catch", "class", "continue", "data",
                 "do", "else", "elseif", "end", "enum", "exit", "filter",
                 "finally", "for", "foreach", "from", "function", "hidden",
                 "if", "in", "param", "process", "return", "static",
                 "switch", "throw", "trap", "try", "until", "using",
                 "var", "while"},
                clr::keyword);
    // Cmdlets pattern  Verb-Noun
    addRule(rules, R"(\b[A-Z][a-z]+-[A-Z][A-Za-z]+\b)",  clr::function_);
    // Variables $Name
    addRule(rules, R"(\$[A-Za-z_?][A-Za-z0-9_]*)",        clr::special_);
    // Strings
    addRule(rules, R"("(?:[^"\\`]|\\.)*")",                clr::string_);
    addRule(rules, R"('[^']*')",                           clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)",           clr::number_);
    // Here-string and attributes
    addRule(rules, R"(\[[\w.,\s\[\]]+\])",                 clr::preproc_);
    addRule(rules, R"(#[^\n]*)",                           clr::comment_);
    blockStart = QRegularExpression(R"(<#)");
    blockEnd   = QRegularExpression(R"(#>)");
    blockFmt   = clr::comment_;
}

static void buildYaml(QVector<SyntaxHighlighter::Rule> &rules)
{
    // Document markers
    addRule(rules, R"(^---\s*$|^\.\.\.\s*$)", clr::preproc_);
    // Keys:  key:  or  "key":
    addRule(rules, R"(^(\s*-?\s*)([A-Za-z_][A-Za-z0-9_\-. ]*)\s*:)", clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*"\s*:)",             clr::special_);
    // Anchors & aliases
    addRule(rules, R"(&[A-Za-z_][A-Za-z0-9_]*|*[A-Za-z_][A-Za-z0-9_]*)", clr::preproc_);
    // Tags
    addRule(rules, R"(![A-Za-z!][A-Za-z0-9!/]*)",          clr::type_);
    // Strings
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                 clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                 clr::string_);
    // Scalar values (booleans, null)
    addKeywords(rules, {"true", "false", "null", "yes", "no", "on", "off",
                        "True", "False", "Null", "Yes", "No", "On", "Off",
                        "TRUE", "FALSE", "NULL"}, clr::keyword);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\b)", clr::number_);
    addRule(rules, R"(#[^\n]*)",                           clr::comment_);
}

static void buildToml(QVector<SyntaxHighlighter::Rule> &rules)
{
    // Section headers  [section]  [[array]]
    addRule(rules, R"(^\s*\[\[?[^\]]*\]\]?)",              clr::keyword);
    // Keys
    addRule(rules, R"(^(\s*[A-Za-z_][A-Za-z0-9_.\-]*)\s*=)", clr::special_);
    // Strings
    addRule(rules, R"(""".*?"""|'''.*?''')",               clr::string_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                 clr::string_);
    addRule(rules, R"('[^']*')",                           clr::string_);
    // Dates
    addRule(rules, R"(\b\d{4}-\d{2}-\d{2}(?:T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})?)?\b)", clr::number_);
    addKeywords(rules, {"true", "false", "inf", "nan"}, clr::keyword);
    addRule(rules, R"(\b[0-9][0-9_]*(?:\.[0-9_]+)?(?:[eE][+-]?[0-9_]+)?\b)", clr::number_);
    addRule(rules, R"(#[^\n]*)",                           clr::comment_);
}

static void buildIni(QVector<SyntaxHighlighter::Rule> &rules)
{
    // Section headers  [section]
    addRule(rules, R"(^\s*\[[^\]]*\])",                    clr::keyword);
    // Keys
    addRule(rules, R"(^\s*[A-Za-z_][A-Za-z0-9_.\-]*\s*(?==))", clr::special_);
    // Values
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                 clr::string_);
    addRule(rules, R"('[^']*')",                           clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)",           clr::number_);
    addKeywords(rules, {"true", "false", "yes", "no", "on", "off",
                        "True", "False", "Yes", "No"}, clr::keyword);
    // Comments: ; or #
    addRule(rules, R"([;#][^\n]*)",                        clr::comment_);
}

static void buildLua(QVector<SyntaxHighlighter::Rule> &rules,
                     QRegularExpression &blockStart,
                     QRegularExpression &blockEnd,
                     QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"and", "break", "do", "else", "elseif", "end", "false",
                 "for", "function", "goto", "if", "in", "local", "nil",
                 "not", "or", "repeat", "return", "then", "true", "until",
                 "while"},
                clr::keyword);
    addKeywords(rules,
                {"assert", "collectgarbage", "dofile", "error", "ipairs",
                 "load", "loadfile", "next", "pairs", "pcall", "print",
                 "rawequal", "rawget", "rawlen", "rawset", "require",
                 "select", "setmetatable", "tonumber", "tostring", "type",
                 "warn", "xpcall",
                 "coroutine", "debug", "io", "math", "os", "package",
                 "string", "table", "utf8"},
                clr::function_);
    addRule(rules, R"(\[\[[\s\S]*?\]\])",                  clr::string_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                 clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                 clr::string_);
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+\b)",            clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?\b)", clr::number_);
    addRule(rules, R"(--[^\n]*)",                          clr::comment_);
    blockStart = QRegularExpression(R"(--\[\[)");
    blockEnd   = QRegularExpression(R"(\]\])");
    blockFmt   = clr::comment_;
}

static void buildRuby(QVector<SyntaxHighlighter::Rule> &rules,
                      QRegularExpression &blockStart,
                      QRegularExpression &blockEnd,
                      QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"__ENCODING__", "__LINE__", "__FILE__", "BEGIN", "END",
                 "alias", "and", "begin", "break", "case", "class",
                 "def", "defined?", "do", "else", "elsif", "end",
                 "ensure", "false", "for", "if", "in", "module",
                 "next", "nil", "not", "or", "redo", "rescue",
                 "retry", "return", "self", "super", "then", "true",
                 "undef", "unless", "until", "when", "while", "yield"},
                clr::keyword);
    // Symbols :foo
    addRule(rules, R"(:[A-Za-z_][A-Za-z0-9_?!]*)",        clr::special_);
    // Instance/class variables
    addRule(rules, R"(@@?[A-Za-z_][A-Za-z0-9_]*)",        clr::special_);
    // String interpolation #{...}
    addRule(rules, R"(#\{[^}]*\})",                        clr::preproc_);
    addRule(rules, R"("(?:[^"\\#]|\\.)*")",                clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                 clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?[rRiI]?\b)",   clr::number_);
    addRule(rules, R"(#[^\n]*)",                           clr::comment_);
    blockStart = QRegularExpression(R"(^=begin\b)");
    blockEnd   = QRegularExpression(R"(^=end\b)");
    blockFmt   = clr::comment_;
}

static void buildMakefile(QVector<SyntaxHighlighter::Rule> &rules)
{
    // Targets:  target:
    addRule(rules, R"(^[A-Za-z_.][A-Za-z0-9_.\-/ ]*\s*:(?!=))", clr::keyword);
    // Variables $(VAR)  ${VAR}
    addRule(rules, R"(\$[\(\{][A-Za-z_][A-Za-z0-9_]*[\)\}])",  clr::special_);
    addRule(rules, R"(\$[@<^%?*+|])",                           clr::special_);
    // Directives
    addKeywords(rules,
                {"include", "define", "endef", "ifdef", "ifndef",
                 "ifeq", "ifneq", "else", "endif", "export", "unexport",
                 "override", "private", "vpath", "undefine"},
                clr::preproc_);
    // Functions $(call, $(subst ...
    addRule(rules, R"(\$\((?:call|subst|patsubst|strip|findstring|filter|sort|word|wordlist|firstword|lastword|dir|notdir|suffix|basename|addsuffix|addprefix|join|wildcard|realpath|abspath|foreach|if|or|and|origin|flavor|value|eval|file|info|warning|error)\b)",
            clr::function_);
    addRule(rules, R"(#[^\n]*)", clr::comment_);
}

static void buildDockerfile(QVector<SyntaxHighlighter::Rule> &rules)
{
    addKeywords(rules,
                {"FROM", "RUN", "CMD", "LABEL", "EXPOSE", "ENV", "ADD",
                 "COPY", "ENTRYPOINT", "VOLUME", "USER", "WORKDIR",
                 "ARG", "ONBUILD", "STOPSIGNAL", "HEALTHCHECK", "SHELL"},
                clr::keyword);
    // Variable expansion
    addRule(rules, R"(\$\{?[A-Za-z_][A-Za-z0-9_]*\}?)", clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",              clr::string_);
    addRule(rules, R"('[^']*')",                        clr::string_);
    addRule(rules, R"(#[^\n]*)",                        clr::comment_);
}

static void buildEnv(QVector<SyntaxHighlighter::Rule> &rules)
{
    // KEY=value
    addRule(rules, R"(^[A-Za-z_][A-Za-z0-9_]*(?==))",  clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",              clr::string_);
    addRule(rules, R"('[^']*')",                        clr::string_);
    // ${VAR} and $VAR in values
    addRule(rules, R"(\$\{?[A-Za-z_][A-Za-z0-9_]*\}?)", clr::preproc_);
    addRule(rules, R"(#[^\n]*)",                        clr::comment_);
}

static void buildGlsl(QVector<SyntaxHighlighter::Rule> &rules,
                      QRegularExpression &blockStart,
                      QRegularExpression &blockEnd,
                      QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"attribute", "break", "bvec2", "bvec3", "bvec4", "case",
                 "centroid", "coherent", "const", "continue", "default",
                 "discard", "do", "double", "dvec2", "dvec3", "dvec4",
                 "else", "flat", "float", "for", "highp", "if", "in",
                 "inout", "int", "invariant", "ivec2", "ivec3", "ivec4",
                 "layout", "lowp", "mat2", "mat3", "mat4", "mat2x2",
                 "mat2x3", "mat2x4", "mat3x2", "mat3x3", "mat3x4",
                 "mat4x2", "mat4x3", "mat4x4", "mediump", "noperspective",
                 "out", "patch", "precise", "precision", "readonly",
                 "restrict", "return", "sample", "sampler2D", "sampler3D",
                 "samplerCube", "samplerCubeShadow", "sampler2DShadow",
                 "smooth", "struct", "subroutine", "switch", "true",
                 "false", "uint", "uniform", "uvec2", "uvec3", "uvec4",
                 "varying", "vec2", "vec3", "vec4", "void", "volatile",
                 "while", "writeonly"},
                clr::keyword);
    addKeywords(rules,
                {"gl_Position", "gl_FragCoord", "gl_FragDepth",
                 "gl_VertexID", "gl_InstanceID", "gl_PointSize",
                 "gl_ClipDistance", "gl_FrontFacing", "gl_PointCoord",
                 "gl_Layer", "gl_ViewportIndex"},
                clr::special_);
    addRule(rules, R"(^\s*#\s*\w+.*)", clr::preproc_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",              clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?[uUfF]?\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)",                       clr::comment_);
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildScala(QVector<SyntaxHighlighter::Rule> &rules,
                       QRegularExpression &blockStart,
                       QRegularExpression &blockEnd,
                       QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"abstract", "case", "catch", "class", "def", "do",
                 "else", "extends", "false", "final", "finally", "for",
                 "forSome", "if", "implicit", "import", "lazy", "match",
                 "new", "null", "object", "override", "package", "private",
                 "protected", "return", "sealed", "super", "this", "throw",
                 "trait", "true", "try", "type", "val", "var", "while",
                 "with", "yield",
                 "given", "using", "enum", "export", "then", "derives",
                 "end", "extension", "opaque", "open", "transparent"},
                clr::keyword);
    addKeywords(rules,
                {"Any", "AnyRef", "AnyVal", "Boolean", "Byte", "Char",
                 "Double", "Float", "Int", "Long", "Nothing", "Null",
                 "Short", "String", "Unit"},
                clr::type_);
    addRule(rules, R"(@[A-Za-z_][A-Za-z0-9_]*)",       clr::preproc_);
    addRule(rules, R"(s"(?:[^"\\]|\\.)*")",             clr::string_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",              clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",              clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?[lLfFdD]?\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)",                       clr::comment_);
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildBatch(QVector<SyntaxHighlighter::Rule> &rules)
{
    addKeywords(rules,
                {"call", "cd", "cls", "copy", "del", "dir", "echo",
                 "else", "endlocal", "exit", "for", "goto", "if",
                 "md", "mkdir", "move", "not", "pause", "rd", "rem",
                 "ren", "rmdir", "set", "setlocal", "shift", "start",
                 "title", "type", "xcopy",
                 "CALL", "CD", "CLS", "COPY", "DEL", "DIR", "ECHO",
                 "ELSE", "ENDLOCAL", "EXIT", "FOR", "GOTO", "IF",
                 "MD", "MKDIR", "MOVE", "NOT", "PAUSE", "RD", "REM",
                 "REN", "RMDIR", "SET", "SETLOCAL", "SHIFT", "START",
                 "TITLE", "TYPE", "XCOPY"},
                clr::keyword);
    // Labels :label
    addRule(rules, R"(^:[A-Za-z_][A-Za-z0-9_]*)",     clr::special_);
    // Variables %VAR%  %1
    addRule(rules, R"(%[A-Za-z_0-9~]*%|%[0-9*])",     clr::preproc_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",             clr::string_);
    // REM comments  and ::
    addRule(rules, R"((?i)^(\s*rem\b|::)[^\n]*)",      clr::comment_);
}

static void buildGraphql(QVector<SyntaxHighlighter::Rule> &rules,
                         QRegularExpression &blockStart,
                         QRegularExpression &blockEnd,
                         QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"directive", "enum", "extend", "fragment", "implements",
                 "input", "interface", "mutation", "on", "query",
                 "repeatable", "scalar", "schema", "subscription",
                 "type", "union"},
                clr::keyword);
    addKeywords(rules,
                {"Boolean", "Float", "ID", "Int", "String",
                 "true", "false", "null"},
                clr::type_);
    // Directives @name
    addRule(rules, R"(@[A-Za-z_][A-Za-z0-9_]*)",      clr::preproc_);
    // Arguments $var
    addRule(rules, R"(\$[A-Za-z_][A-Za-z0-9_]*)",     clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",             clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)",       clr::number_);
    addRule(rules, R"(#[^\n]*)",                       clr::comment_);
    blockStart = QRegularExpression(R"(""")");
    blockEnd   = QRegularExpression(R"(""")");
    blockFmt   = clr::string_;
}

static void buildTerraform(QVector<SyntaxHighlighter::Rule> &rules,
                           QRegularExpression &blockStart,
                           QRegularExpression &blockEnd,
                           QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"resource", "data", "provider", "variable", "output",
                 "module", "terraform", "locals", "dynamic", "for_each",
                 "count", "depends_on", "lifecycle", "provisioner",
                 "connection", "backend", "required_providers",
                 "required_version"},
                clr::keyword);
    addKeywords(rules, {"true", "false", "null"}, clr::type_);
    // Interpolation ${...}  and function calls
    addRule(rules, R"(\$\{[^}]*\})",              clr::special_);
    addRule(rules, R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*\()", clr::function_);
    addRule(rules, R"(<<-?\s*[A-Z_]+)",          clr::preproc_);  // heredoc
    addRule(rules, R"("(?:[^"\\]|\\.)*")",        clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)",  clr::number_);
    addRule(rules, R"(#[^\n]*)",                  clr::comment_);
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

static void buildSln(QVector<SyntaxHighlighter::Rule> &rules)
{
    // Section keywords
    addKeywords(rules,
                {"Project", "EndProject", "Global", "EndGlobal",
                 "GlobalSection", "EndGlobalSection",
                 "ProjectSection", "EndProjectSection"},
                clr::keyword);

    // GUIDs  {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
    addRule(rules, R"(\{[0-9A-Fa-f\-]{36}\})", clr::special_);

    // Section type labels in parentheses
    addRule(rules, R"(\([A-Za-z][A-Za-z0-9]*\))", clr::type_);

    // String values after = or between quotes
    addRule(rules, R"("(?:[^"\\]|\\.)*")", clr::string_);

    // Version header lines  (#  Visual Studio Version...)
    addRule(rules, R"(^#[^\n]*)", clr::comment_);

    // Key = value pairs (right-hand side)
    addRule(rules, R"(\b\d+\.\d+(?:\.\d+)*\b)", clr::number_);
}

static void buildQmake(QVector<SyntaxHighlighter::Rule> &rules)
{
    // Built-in variables
    addKeywords(rules,
                {"SOURCES", "HEADERS", "FORMS", "RESOURCES",
                 "QT", "CONFIG", "LIBS", "INCLUDEPATH", "DEFINES",
                 "TARGET", "TEMPLATE", "DESTDIR", "OBJECTS_DIR",
                 "MOC_DIR", "UI_DIR", "RC_ICONS", "RC_FILE",
                 "VERSION", "QMAKE_CXXFLAGS", "QMAKE_LFLAGS",
                 "TRANSLATIONS", "QMAKE_INFO_PLIST",
                 "SUBDIRS", "DEPENDPATH", "VPATH",
                 "QMAKE_TARGET_BUNDLE_PREFIX", "QMAKE_APPLE_DEVICE_ARCHS"},
                clr::keyword);

    // Platform / condition scopes:  win32:  unix:  macx:
    addRule(rules, R"(\b(win32|unix|macx|linux|android|ios|wasm|msvc|gcc|clang|debug|release)\b(?=\s*[:{(]))",
            clr::type_);

    // Variable expansion  $$VAR  $$(ENV)  $$[prop]  $${}
    addRule(rules, R"(\$\$[\w\[\]{}()]+|\$\$\([^)]*\))", clr::special_);

    // Strings
    addRule(rules, R"("(?:[^"\\]|\\.)*")", clr::string_);

    // Operators and continuation
    addRule(rules, R"([+\-*]?=)", clr::preproc_);
    addRule(rules, R"(\\$)",      clr::preproc_);   // line continuation

    // Numbers
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)", clr::number_);

    // Comments
    addRule(rules, R"(#[^\n]*)", clr::comment_);
}

// ── CSS / SCSS / LESS ────────────────────────────────────────────────────────

static void buildCss(QVector<SyntaxHighlighter::Rule> &rules,
                     QRegularExpression &blockStart,
                     QRegularExpression &blockEnd,
                     QTextCharFormat &blockFmt)
{
    // At-rules
    addRule(rules, R"(@[a-zA-Z-]+)", clr::keyword);

    // Selectors – class, ID, pseudo
    addRule(rules, R"(\.[a-zA-Z_][\w-]*)", clr::type_);
    addRule(rules, R"(#[a-zA-Z_][\w-]*)", clr::function_);
    addRule(rules, R"(:{1,2}[a-zA-Z-]+)", clr::special_);

    // Properties
    addRule(rules, R"([a-zA-Z-]+\s*(?=:))", clr::special_);

    // Strings
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])*')", clr::string_);

    // Numbers with units
    addRule(rules, R"(\b\d+(\.\d+)?(px|em|rem|%|vh|vw|pt|cm|mm|in|s|ms|deg|rad|fr)?\b)", clr::number_);

    // Hex colors
    addRule(rules, R"(#[0-9a-fA-F]{3,8}\b)", clr::number_);

    // !important
    addRule(rules, R"(!important\b)", clr::keyword);

    // SCSS variables and Sass/LESS variables
    addRule(rules, R"(\$[a-zA-Z_][\w-]*)", clr::special_);
    addRule(rules, R"(@[a-zA-Z_][\w-]*)", clr::special_);

    // Functions
    addRule(rules, R"(\b[a-zA-Z-]+\s*(?=\())", clr::function_);

    // Comments
    addRule(rules, R"(//[^\n]*)", clr::comment_);

    // Block comments
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

// ── Perl ─────────────────────────────────────────────────────────────────────

static void buildPerl(QVector<SyntaxHighlighter::Rule> &rules)
{
    addKeywords(rules,
                {"my", "our", "local", "use", "require", "package", "sub",
                 "if", "elsif", "else", "unless", "while", "until", "for",
                 "foreach", "do", "last", "next", "redo", "return",
                 "die", "warn", "print", "say", "chomp", "chop",
                 "push", "pop", "shift", "unshift", "splice",
                 "grep", "map", "sort", "reverse", "join", "split",
                 "open", "close", "read", "write", "seek", "tell",
                 "defined", "undef", "ref", "bless", "tie",
                 "eval", "BEGIN", "END", "AUTOLOAD", "DESTROY",
                 "eq", "ne", "lt", "gt", "le", "ge", "cmp", "not", "and", "or"},
                clr::keyword);

    // Variables
    addRule(rules, R"([\$@%]\w+)", clr::special_);

    // Strings
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])*')", clr::string_);

    // Regex
    addRule(rules, R"(/(?:\\/|[^/\n])*/[gimsxpe]*)", clr::preproc_);

    // Numbers
    addRule(rules, R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F]+\b)", clr::number_);

    // Functions
    addRule(rules, R"(\b[a-zA-Z_]\w*\s*(?=\())", clr::function_);

    // Comments
    addRule(rules, R"(#[^\n]*)", clr::comment_);
}

// ── R ────────────────────────────────────────────────────────────────────────

static void buildR(QVector<SyntaxHighlighter::Rule> &rules)
{
    addKeywords(rules,
                {"if", "else", "repeat", "while", "for", "in", "next",
                 "break", "return", "function", "library", "require",
                 "source", "TRUE", "FALSE", "NULL", "NA", "NA_integer_",
                 "NA_real_", "NA_complex_", "NA_character_", "Inf", "NaN",
                 "switch", "tryCatch", "stop", "warning", "message"},
                clr::keyword);

    // Assignment operators
    addRule(rules, R"(<-|->|<<-|->>)", clr::keyword);

    // Strings
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])*')", clr::string_);

    // Numbers
    addRule(rules, R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?L?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F]+L?\b)", clr::number_);

    // Functions
    addRule(rules, R"(\b[a-zA-Z_.]\w*\s*(?=\())", clr::function_);

    // Comments
    addRule(rules, R"(#[^\n]*)", clr::comment_);
}

// ── Haskell ──────────────────────────────────────────────────────────────────

static void buildHaskell(QVector<SyntaxHighlighter::Rule> &rules,
                         QRegularExpression &blockStart,
                         QRegularExpression &blockEnd,
                         QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"module", "import", "qualified", "as", "hiding", "where",
                 "data", "type", "newtype", "class", "instance", "deriving",
                 "if", "then", "else", "case", "of", "let", "in", "do",
                 "return", "forall", "infixl", "infixr", "infix",
                 "True", "False", "Nothing", "Just", "Left", "Right",
                 "otherwise"},
                clr::keyword);

    // Type names (start with uppercase)
    addRule(rules, R"(\b[A-Z][a-zA-Z0-9_']*)", clr::type_);

    // Strings / Chars
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])')", clr::string_);

    // Numbers
    addRule(rules, R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F]+\b)", clr::number_);
    addRule(rules, R"(\b0[oO][0-7]+\b)", clr::number_);

    // Operators
    addRule(rules, R"(->|<-|=>|::|\.\.|\\)", clr::keyword);

    // Comments
    addRule(rules, R"(--[^\n]*)", clr::comment_);

    blockStart = QRegularExpression(R"(\{-)");
    blockEnd   = QRegularExpression(R"(-\})");
    blockFmt   = clr::comment_;
}

// ── Elixir ───────────────────────────────────────────────────────────────────

static void buildElixir(QVector<SyntaxHighlighter::Rule> &rules)
{
    addKeywords(rules,
                {"def", "defp", "defmodule", "defprotocol", "defimpl",
                 "defmacro", "defmacrop", "defguard", "defguardp",
                 "defstruct", "defexception", "defdelegate",
                 "do", "end", "if", "else", "unless", "cond", "case",
                 "when", "with", "for", "receive", "after", "try",
                 "catch", "rescue", "raise", "throw", "import", "use",
                 "alias", "require", "fn", "in", "not", "and", "or",
                 "true", "false", "nil", "quote", "unquote", "super"},
                clr::keyword);

    // Module names
    addRule(rules, R"(\b[A-Z][a-zA-Z0-9_.]*)", clr::type_);

    // Atoms
    addRule(rules, R"(:[a-zA-Z_]\w*)", clr::special_);

    // Strings
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])*')", clr::string_);

    // Sigils
    addRule(rules, R"(~[rRswWcC]\{[^}]*\})", clr::preproc_);
    addRule(rules, R"(~[rRswWcC]\([^)]*\))", clr::preproc_);
    addRule(rules, R"(~[rRswWcC]\"[^"]*\")", clr::preproc_);

    // Numbers
    addRule(rules, R"(\b\d+(?:_\d+)*(?:\.\d+(?:_\d+)*)?(?:[eE][+-]?\d+)?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F_]+\b)", clr::number_);
    addRule(rules, R"(\b0[bB][01_]+\b)", clr::number_);
    addRule(rules, R"(\b0[oO][0-7_]+\b)", clr::number_);

    // Module attributes / decorators
    addRule(rules, R"(@[a-zA-Z_]\w*)", clr::preproc_);

    // Functions
    addRule(rules, R"(\b[a-z_]\w*[!?]?\s*(?=\())", clr::function_);

    // Pipe operator
    addRule(rules, R"(\|>)", clr::keyword);

    // Comments
    addRule(rules, R"(#[^\n]*)", clr::comment_);
}

// ── Zig ──────────────────────────────────────────────────────────────────────

static void buildZig(QVector<SyntaxHighlighter::Rule> &rules,
                     QRegularExpression &blockStart,
                     QRegularExpression &blockEnd,
                     QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"align", "allowzero", "and", "anytype", "asm", "async",
                 "await", "break", "catch", "comptime", "const", "continue",
                 "defer", "else", "enum", "errdefer", "error", "export",
                 "extern", "fn", "for", "if", "inline", "noalias",
                 "nosuspend", "opaque", "or", "orelse", "packed",
                 "pub", "resume", "return", "struct", "suspend", "switch",
                 "test", "threadlocal", "try", "union", "unreachable",
                 "usingnamespace", "var", "volatile", "while",
                 "true", "false", "null", "undefined"},
                clr::keyword);

    // Types
    addKeywords(rules,
                {"i8", "i16", "i32", "i64", "i128", "isize",
                 "u8", "u16", "u32", "u64", "u128", "usize",
                 "f16", "f32", "f64", "f128", "bool", "void",
                 "anyerror", "anyframe", "comptime_int", "comptime_float",
                 "type", "noreturn"},
                clr::type_);

    // Built-in functions
    addRule(rules, R"(@[a-zA-Z_]\w*)", clr::preproc_);

    // Strings
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);

    // Numbers
    addRule(rules, R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F_]+\b)", clr::number_);
    addRule(rules, R"(\b0[bB][01_]+\b)", clr::number_);
    addRule(rules, R"(\b0[oO][0-7_]+\b)", clr::number_);

    // Functions
    addRule(rules, R"(\b[a-zA-Z_]\w*\s*(?=\())", clr::function_);

    // Comments
    addRule(rules, R"(//[^\n]*)", clr::comment_);

    // Zig has no block comments, but keep the signature for consistency
    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

// ── Protobuf ─────────────────────────────────────────────────────────────────

static void buildProtobuf(QVector<SyntaxHighlighter::Rule> &rules,
                          QRegularExpression &blockStart,
                          QRegularExpression &blockEnd,
                          QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"syntax", "package", "import", "option", "message", "enum",
                 "service", "rpc", "returns", "stream", "oneof", "map",
                 "reserved", "extensions", "extend", "optional", "required",
                 "repeated", "public", "weak", "true", "false", "to", "max"},
                clr::keyword);

    // Types
    addKeywords(rules,
                {"double", "float", "int32", "int64", "uint32", "uint64",
                 "sint32", "sint64", "fixed32", "fixed64",
                 "sfixed32", "sfixed64", "bool", "string", "bytes",
                 "google", "protobuf", "Any", "Timestamp", "Duration"},
                clr::type_);

    // Message/Enum names (PascalCase)
    addRule(rules, R"(\b[A-Z][a-zA-Z0-9_]*)", clr::type_);

    // Strings
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])*')", clr::string_);

    // Numbers
    addRule(rules, R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F]+\b)", clr::number_);

    // Field numbers
    addRule(rules, R"(=\s*\d+)", clr::number_);

    // Comments
    addRule(rules, R"(//[^\n]*)", clr::comment_);

    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

// ── SyntaxHighlighter ────────────────────────────────────────────────────────

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *doc)
    : QSyntaxHighlighter(doc)
{
}

SyntaxHighlighter *SyntaxHighlighter::create(const QString &filePath, QTextDocument *doc)
{
    const QString ext = QFileInfo(filePath).suffix().toLower();
    const QString name = QFileInfo(filePath).fileName().toLower();

    auto *h = new SyntaxHighlighter(doc);

    if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "c" ||
        ext == "h"   || ext == "hpp" || ext == "hxx" || ext == "inl" ||
        ext == "mm")
    {
        buildCpp(h, h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "py" || ext == "pyw")
    {
        buildPython(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "json" || ext == "jsonc")
    {
        buildJson(h->m_rules);
    }
    else if (name == "cmakelists.txt" || ext == "cmake")
    {
        buildCmake(h->m_rules);
    }
    else if (ext == "md" || ext == "markdown")
    {
        buildMarkdown(h->m_rules);
    }
    else if (ext == "cs")
    {
        buildCsharp(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "java")
    {
        buildJava(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "dart")
    {
        buildDart(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "xml"   || ext == "html"   || ext == "htm"  ||
             ext == "svg"   || ext == "xsl"    || ext == "xslt" ||
             ext == "xaml"  || ext == "plist"  || ext == "ui"   ||
             ext == "resx"  || ext == "csproj" || ext == "vbproj" ||
             ext == "fsproj"|| ext == "props"  || ext == "targets" ||
             ext == "config"|| ext == "manifest")
    {
        buildXml(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "sln")
    {
        buildSln(h->m_rules);
    }
    else if (ext == "pro" || ext == "pri" || ext == "prf")
    {
        buildQmake(h->m_rules);
    }
    else if (ext == "sql" || ext == "ddl" || ext == "dml")
    {
        buildSql(h->m_rules);
    }
    else if (ext == "php" || ext == "php3" || ext == "php4" || ext == "phtml")
    {
        buildPhp(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "js"  || ext == "mjs" || ext == "cjs" ||
             ext == "ts"  || ext == "tsx" || ext == "jsx")
    {
        buildJavaScript(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "rs")
    {
        buildRust(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "go")
    {
        buildGo(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "kt" || ext == "kts")
    {
        buildKotlin(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "swift")
    {
        buildSwift(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "sh" || ext == "bash" || ext == "zsh" ||
             ext == "ksh" || ext == "fish" || name == ".bashrc" ||
             name == ".zshrc" || name == ".profile" || name == ".bash_profile")
    {
        buildShell(h->m_rules);
    }
    else if (ext == "ps1" || ext == "psm1" || ext == "psd1")
    {
        buildPowerShell(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "yaml" || ext == "yml")
    {
        buildYaml(h->m_rules);
    }
    else if (ext == "toml")
    {
        buildToml(h->m_rules);
    }
    else if (ext == "ini"   || ext == "conf"  || ext == "cfg"  ||
             ext == "properties" || ext == "env" || name == ".env" ||
             name == ".gitconfig" || name == ".editorconfig")
    {
        buildIni(h->m_rules);
    }
    else if (name == ".env" || ext == "env")
    {
        buildEnv(h->m_rules);
    }
    else if (ext == "lua")
    {
        buildLua(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "rb" || ext == "rake" || ext == "gemspec" ||
             name == "Gemfile" || name == "Rakefile" || name == "Guardfile")
    {
        buildRuby(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (name == "makefile" || name == "Makefile" || name == "GNUmakefile" || ext == "mk")
    {
        buildMakefile(h->m_rules);
    }
    else if (name == "Dockerfile" || name.startsWith("Dockerfile.") || ext == "dockerfile")
    {
        buildDockerfile(h->m_rules);
    }
    else if (ext == "glsl" || ext == "vert" || ext == "frag" ||
             ext == "geom" || ext == "comp" || ext == "tesc" ||
             ext == "tese" || ext == "hlsl" || ext == "fx")
    {
        buildGlsl(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "scala" || ext == "sc")
    {
        buildScala(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "bat" || ext == "cmd")
    {
        buildBatch(h->m_rules);
    }
    else if (ext == "graphql" || ext == "gql")
    {
        buildGraphql(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "tf" || ext == "tfvars" || ext == "hcl")
    {
        buildTerraform(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "css" || ext == "scss" || ext == "sass" || ext == "less")
    {
        buildCss(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "pl" || ext == "pm" || ext == "t" || ext == "pod")
    {
        buildPerl(h->m_rules);
    }
    else if (ext == "r" || ext == "rmd")
    {
        buildR(h->m_rules);
    }
    else if (ext == "hs" || ext == "lhs")
    {
        buildHaskell(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else if (ext == "ex" || ext == "exs" || ext == "heex" || ext == "leex")
    {
        buildElixir(h->m_rules);
    }
    else if (ext == "zig" || ext == "zon")
    {
        buildZig(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
    }
    else if (ext == "proto" || ext == "proto3")
    {
        buildProtobuf(h->m_rules, h->m_blockCommentStart, h->m_blockCommentEnd, h->m_blockCommentFmt);
        h->m_hasBlockComments = true;
    }
    else
    {
        // No rules — delete the highlighter, return nullptr
        delete h;
        return nullptr;
    }

    return h;
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    // ── Single-line rules ────────────────────────────────────────────────
    for (const Rule &rule : std::as_const(m_rules)) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }

    // ── Multi-line block comments (state 0 = normal, 1 = inside block) ──
    if (!m_hasBlockComments)
        return;

    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = static_cast<int>(
            m_blockCommentStart.match(text).capturedStart());

    while (startIndex >= 0) {
        const QRegularExpressionMatch endMatch =
            m_blockCommentEnd.match(text, startIndex + 2);

        int endIndex = static_cast<int>(endMatch.capturedStart());
        int commentLen;
        if (endIndex < 0) {
            setCurrentBlockState(1);
            commentLen = text.length() - startIndex;
        } else {
            commentLen = endIndex - startIndex + static_cast<int>(endMatch.capturedLength());
        }
        setFormat(startIndex, commentLen, m_blockCommentFmt);

        const QRegularExpressionMatch nextStart =
            m_blockCommentStart.match(text, startIndex + commentLen);
        startIndex = static_cast<int>(nextStart.capturedStart());
    }
}

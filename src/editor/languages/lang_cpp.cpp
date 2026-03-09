#include "langcommon.h"

void buildCpp(QVector<SyntaxHighlighter::Rule> &rules,
              QRegularExpression &blockStart,
              QRegularExpression &blockEnd,
              QTextCharFormat &blockFmt)
{
    addRule(rules, R"(^\s*#\s*\w+.*)", clr::preproc_);

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

    addRule(rules, R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*(?=\())", clr::function_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")", clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')", clr::string_);
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+[uUlL]*\b)", clr::number_);
    addRule(rules, R"(\b0[bB][01]+[uUlL]*\b)",         clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?[fFlL]?\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)", clr::comment_);

    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

void buildCsharp(QVector<SyntaxHighlighter::Rule> &rules,
                 QRegularExpression &blockStart,
                 QRegularExpression &blockEnd,
                 QTextCharFormat &blockFmt)
{
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

    addRule(rules, R"(\[[A-Za-z_][A-Za-z0-9_.,\s]*\])", clr::preproc_);
    addRule(rules, R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*(?=\())", clr::function_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                   clr::string_);
    addRule(rules, R"(@"[^"]*")",                             clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                   clr::string_);
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+[uUlL]*\b)",       clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?[mMfFdDuUlL]*\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)",                             clr::comment_);

    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

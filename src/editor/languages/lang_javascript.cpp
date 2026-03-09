#include "langcommon.h"

void buildJavaScript(QVector<SyntaxHighlighter::Rule> &rules,
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

    addRule(rules, R"(`(?:[^`\\]|\\.)*`)",                    clr::string_);
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

void buildTypeScript(QVector<SyntaxHighlighter::Rule> &rules,
                     QRegularExpression &blockStart,
                     QRegularExpression &blockEnd,
                     QTextCharFormat &blockFmt)
{
    // Decorators (TypeScript metadata decorators)
    addRule(rules, R"(@[A-Za-z_][A-Za-z0-9_.]*)", clr::preproc_);

    // All JavaScript keywords + TypeScript-specific keywords
    addKeywords(rules,
                {"abstract", "as", "async", "await", "break", "case",
                 "catch", "class", "const", "constructor", "continue",
                 "debugger", "declare", "default", "delete", "do", "else",
                 "enum", "export", "extends", "false", "finally", "for",
                 "from", "function", "get", "if", "implements", "import",
                 "in", "infer", "instanceof", "interface", "is", "keyof",
                 "let", "module", "namespace", "new", "null", "of",
                 "override", "package", "private", "protected", "public",
                 "readonly", "return", "satisfies", "set", "static",
                 "super", "switch", "this", "throw", "true", "try",
                 "type", "typeof", "undefined", "unique", "var", "void",
                 "while", "with", "yield"},
                clr::keyword);

    // TypeScript built-in types + JS standard types
    addKeywords(rules,
                {"any", "bigint", "boolean", "never", "number", "object",
                 "string", "symbol", "unknown", "void",
                 "Array", "Boolean", "Date", "Error", "Function", "JSON",
                 "Map", "Math", "Number", "Object", "Partial", "Pick",
                 "Promise", "Readonly", "Record", "RegExp", "Required",
                 "ReturnType", "Set", "String", "Symbol", "WeakMap",
                 "WeakSet", "Exclude", "Extract", "Omit", "NonNullable",
                 "Parameters", "ConstructorParameters", "InstanceType",
                 "Awaited", "Uppercase", "Lowercase", "Capitalize",
                 "Uncapitalize",
                 "console", "document", "window", "globalThis",
                 "parseInt", "parseFloat", "isNaN", "isFinite",
                 "setTimeout", "setInterval", "clearTimeout", "clearInterval",
                 "fetch", "require", "module", "exports"},
                clr::type_);

    // Angle-bracket type assertions  <Type>
    addRule(rules, R"(<[A-Z][A-Za-z0-9_,<>\s]*>)", clr::type_);

    // Template literal strings
    addRule(rules, R"(`(?:[^`\\]|\\.)*`)",                    clr::string_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                    clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                    clr::string_);

    // Function calls
    addRule(rules, R"(\b([A-Za-z_$][A-Za-z0-9_$]*)\s*(?=\())", clr::function_);

    // Numbers (with BigInt n suffix)
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+n?\b)",              clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?n?\b)", clr::number_);

    // Single-line comments
    addRule(rules, R"(//[^\n]*)",                              clr::comment_);

    // Block comments
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

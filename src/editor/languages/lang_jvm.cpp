#include "langcommon.h"

void buildJava(QVector<SyntaxHighlighter::Rule> &rules,
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

void buildKotlin(QVector<SyntaxHighlighter::Rule> &rules,
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

void buildScala(QVector<SyntaxHighlighter::Rule> &rules,
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

void buildDart(QVector<SyntaxHighlighter::Rule> &rules,
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

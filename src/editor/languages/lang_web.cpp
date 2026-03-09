#include "langcommon.h"

void buildXml(QVector<SyntaxHighlighter::Rule> &rules,
              QRegularExpression &blockStart,
              QRegularExpression &blockEnd,
              QTextCharFormat &blockFmt)
{
    addRule(rules, R"(<!\[CDATA\[[\s\S]*?\]\]>)", clr::string_);
    addRule(rules, R"(</?[A-Za-z_][A-Za-z0-9_:.-]*)", clr::keyword);
    addRule(rules, R"(/?>)", clr::keyword);
    addRule(rules, R"([A-Za-z_][A-Za-z0-9_:.-]*\s*=)", clr::special_);
    addRule(rules, R"("[^"]*"|'[^']*')", clr::string_);
    addRule(rules, R"(&[A-Za-z0-9#]+;)", clr::preproc_);
    addRule(rules, R"(<[?!][^>]*>)", clr::preproc_);

    blockStart = QRegularExpression(R"(<!--)");
    blockEnd   = QRegularExpression(R"(-->)");
    blockFmt   = clr::comment_;
}

void buildCss(QVector<SyntaxHighlighter::Rule> &rules,
              QRegularExpression &blockStart,
              QRegularExpression &blockEnd,
              QTextCharFormat &blockFmt)
{
    addRule(rules, R"(@[a-zA-Z-]+)", clr::keyword);
    addRule(rules, R"(\.[a-zA-Z_][\w-]*)", clr::type_);
    addRule(rules, R"(#[a-zA-Z_][\w-]*)", clr::function_);
    addRule(rules, R"(:{1,2}[a-zA-Z-]+)", clr::special_);
    addRule(rules, R"([a-zA-Z-]+\s*(?=:))", clr::special_);
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])*')", clr::string_);
    addRule(rules, R"(\b\d+(\.\d+)?(px|em|rem|%|vh|vw|pt|cm|mm|in|s|ms|deg|rad|fr)?\b)", clr::number_);
    addRule(rules, R"(#[0-9a-fA-F]{3,8}\b)", clr::number_);
    addRule(rules, R"(!important\b)", clr::keyword);
    addRule(rules, R"(\$[a-zA-Z_][\w-]*)", clr::special_);
    addRule(rules, R"(@[a-zA-Z_][\w-]*)", clr::special_);
    addRule(rules, R"(\b[a-zA-Z-]+\s*(?=\())", clr::function_);
    addRule(rules, R"(//[^\n]*)", clr::comment_);

    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

void buildPhp(QVector<SyntaxHighlighter::Rule> &rules,
              QRegularExpression &blockStart,
              QRegularExpression &blockEnd,
              QTextCharFormat &blockFmt)
{
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

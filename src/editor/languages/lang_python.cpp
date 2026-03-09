#include "langcommon.h"

void buildPython(QVector<SyntaxHighlighter::Rule> &rules,
                 QRegularExpression &blockStart,
                 QRegularExpression &blockEnd,
                 QTextCharFormat &blockFmt)
{
    addRule(rules, R"(@[A-Za-z_][A-Za-z0-9_.]*)", clr::preproc_);

    addKeywords(rules,
                {"False", "None", "True", "and", "as", "assert", "async",
                 "await", "break", "class", "continue", "def", "del",
                 "elif", "else", "except", "finally", "for", "from",
                 "global", "if", "import", "in", "is", "lambda",
                 "nonlocal", "not", "or", "pass", "raise", "return",
                 "try", "while", "with", "yield"},
                clr::keyword);

    addKeywords(rules,
                {"bool", "bytearray", "bytes", "complex", "dict", "float",
                 "frozenset", "int", "list", "memoryview", "object",
                 "range", "set", "str", "tuple", "type"},
                clr::type_);

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

    addRule(rules, R"(""".*?""")", clr::string_);
    addRule(rules, R"('''.*?''')", clr::string_);
    addRule(rules, R"(f?"(?:[^"\\]|\\.)*")", clr::string_);
    addRule(rules, R"(f?'(?:[^'\\]|\\.)*')", clr::string_);
    addRule(rules, R"(r"[^"]*")",             clr::string_);
    addRule(rules, R"(r'[^']*')",             clr::string_);
    addRule(rules, R"(\b0[xX][0-9A-Fa-f]+\b)", clr::number_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?[jJ]?\b)", clr::number_);
    addRule(rules, R"(#[^\n]*)", clr::comment_);

    blockStart = QRegularExpression(R"(""")");
    blockEnd   = QRegularExpression(R"(""")");
    blockFmt   = clr::string_;
}

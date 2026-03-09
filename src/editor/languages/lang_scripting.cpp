#include "langcommon.h"

void buildLua(QVector<SyntaxHighlighter::Rule> &rules,
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

void buildRuby(QVector<SyntaxHighlighter::Rule> &rules,
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
    addRule(rules, R"(:[A-Za-z_][A-Za-z0-9_?!]*)",        clr::special_);
    addRule(rules, R"(@@?[A-Za-z_][A-Za-z0-9_]*)",        clr::special_);
    addRule(rules, R"(#\{[^}]*\})",                        clr::preproc_);
    addRule(rules, R"("(?:[^"\\#]|\\.)*")",                clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                 clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?[rRiI]?\b)",   clr::number_);
    addRule(rules, R"(#[^\n]*)",                           clr::comment_);
    blockStart = QRegularExpression(R"(^=begin\b)");
    blockEnd   = QRegularExpression(R"(^=end\b)");
    blockFmt   = clr::comment_;
}

void buildPerl(QVector<SyntaxHighlighter::Rule> &rules,
               QRegularExpression &blockStart,
               QRegularExpression &blockEnd,
               QTextCharFormat &blockFmt)
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

    addRule(rules, R"([\$@%]\w+)", clr::special_);
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])*')", clr::string_);
    addRule(rules, R"(/(?:\\/|[^/\n])*/[gimsxpe]*)", clr::preproc_);
    addRule(rules, R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F]+\b)", clr::number_);
    addRule(rules, R"(\b[a-zA-Z_]\w*\s*(?=\())", clr::function_);
    addRule(rules, R"(#[^\n]*)", clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildR(QVector<SyntaxHighlighter::Rule> &rules,
            QRegularExpression &blockStart,
            QRegularExpression &blockEnd,
            QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"if", "else", "repeat", "while", "for", "in", "next",
                 "break", "return", "function", "library", "require",
                 "source", "TRUE", "FALSE", "NULL", "NA", "NA_integer_",
                 "NA_real_", "NA_complex_", "NA_character_", "Inf", "NaN",
                 "switch", "tryCatch", "stop", "warning", "message"},
                clr::keyword);

    addRule(rules, R"(<-|->|<<-|->>)", clr::keyword);
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])*')", clr::string_);
    addRule(rules, R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?L?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F]+L?\b)", clr::number_);
    addRule(rules, R"(\b[a-zA-Z_.]\w*\s*(?=\())", clr::function_);
    addRule(rules, R"(#[^\n]*)", clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildElixir(QVector<SyntaxHighlighter::Rule> &rules,
                 QRegularExpression &blockStart,
                 QRegularExpression &blockEnd,
                 QTextCharFormat &blockFmt)
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

    addRule(rules, R"(\b[A-Z][a-zA-Z0-9_.]*)", clr::type_);
    addRule(rules, R"(:[a-zA-Z_]\w*)", clr::special_);
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])*')", clr::string_);
    addRule(rules, R"(~[rRswWcC]\{[^}]*\})", clr::preproc_);
    addRule(rules, R"(~[rRswWcC]\([^)]*\))", clr::preproc_);
    addRule(rules, R"(~[rRswWcC]\"[^"]*\")", clr::preproc_);
    addRule(rules, R"(\b\d+(?:_\d+)*(?:\.\d+(?:_\d+)*)?(?:[eE][+-]?\d+)?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F_]+\b)", clr::number_);
    addRule(rules, R"(\b0[bB][01_]+\b)", clr::number_);
    addRule(rules, R"(\b0[oO][0-7_]+\b)", clr::number_);
    addRule(rules, R"(@[a-zA-Z_]\w*)", clr::preproc_);
    addRule(rules, R"(\b[a-z_]\w*[!?]?\s*(?=\())", clr::function_);
    addRule(rules, R"(\|>)", clr::keyword);
    addRule(rules, R"(#[^\n]*)", clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildHaskell(QVector<SyntaxHighlighter::Rule> &rules,
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

    addRule(rules, R"(\b[A-Z][a-zA-Z0-9_']*)", clr::type_);
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])')", clr::string_);
    addRule(rules, R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F]+\b)", clr::number_);
    addRule(rules, R"(\b0[oO][0-7]+\b)", clr::number_);
    addRule(rules, R"(->|<-|=>|::|\.\.|\\)", clr::keyword);
    addRule(rules, R"(--[^\n]*)", clr::comment_);

    blockStart = QRegularExpression(R"(\{-)");
    blockEnd   = QRegularExpression(R"(-\})");
    blockFmt   = clr::comment_;
}

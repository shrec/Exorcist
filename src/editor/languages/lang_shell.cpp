#include "langcommon.h"

void buildShell(QVector<SyntaxHighlighter::Rule> &rules,
                QRegularExpression &blockStart,
                QRegularExpression &blockEnd,
                QTextCharFormat &blockFmt)
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
    addRule(rules, R"(\$\{?[A-Za-z_][A-Za-z0-9_]*\}?)", clr::special_);
    addRule(rules, R"(\$[@#?$!0-9*\-])",                  clr::special_);
    addRule(rules, R"("(?:[^"\\$]|\\.|\$[^{])*")",        clr::string_);
    addRule(rules, R"('[^']*')",                           clr::string_);
    addRule(rules, R"(`[^`]*`)",                           clr::preproc_);
    addRule(rules, R"(^#![^\n]*)",                         clr::preproc_);
    addRule(rules, R"(#[^\n]*)",                           clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildPowerShell(QVector<SyntaxHighlighter::Rule> &rules,
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
    addRule(rules, R"(\b[A-Z][a-z]+-[A-Z][A-Za-z]+\b)",  clr::function_);
    addRule(rules, R"(\$[A-Za-z_?][A-Za-z0-9_]*)",        clr::special_);
    addRule(rules, R"("(?:[^"\\`]|\\.)*")",                clr::string_);
    addRule(rules, R"('[^']*')",                           clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)",           clr::number_);
    addRule(rules, R"(\[[\w.,\s\[\]]+\])",                 clr::preproc_);
    addRule(rules, R"(#[^\n]*)",                           clr::comment_);
    blockStart = QRegularExpression(R"(<#)");
    blockEnd   = QRegularExpression(R"(#>)");
    blockFmt   = clr::comment_;
}

void buildBatch(QVector<SyntaxHighlighter::Rule> &rules,
                QRegularExpression &blockStart,
                QRegularExpression &blockEnd,
                QTextCharFormat &blockFmt)
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
    addRule(rules, R"(^:[A-Za-z_][A-Za-z0-9_]*)",     clr::special_);
    addRule(rules, R"(%[A-Za-z_0-9~]*%|%[0-9*])",     clr::preproc_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",             clr::string_);
    addRule(rules, R"((?i)^(\s*rem\b|::)[^\n]*)",      clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

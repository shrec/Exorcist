#include "langcommon.h"

void buildJson(QVector<SyntaxHighlighter::Rule> &rules,
               QRegularExpression &blockStart,
               QRegularExpression &blockEnd,
               QTextCharFormat &blockFmt)
{
    addRule(rules, R"("(?:[^"\\]|\\.)*"\s*:)", clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",     clr::string_);
    addKeywords(rules, {"true", "false", "null"}, clr::keyword);
    addRule(rules, R"(-?[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)", clr::number_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildYaml(QVector<SyntaxHighlighter::Rule> &rules,
               QRegularExpression &blockStart,
               QRegularExpression &blockEnd,
               QTextCharFormat &blockFmt)
{
    addRule(rules, R"(^---\s*$|^\.\.\.\s*$)", clr::preproc_);
    addRule(rules, R"(^(\s*-?\s*)([A-Za-z_][A-Za-z0-9_\-. ]*)\s*:)", clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*"\s*:)",             clr::special_);
    addRule(rules, R"(&[A-Za-z_][A-Za-z0-9_]*|*[A-Za-z_][A-Za-z0-9_]*)", clr::preproc_);
    addRule(rules, R"(![A-Za-z!][A-Za-z0-9!/]*)",          clr::type_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                 clr::string_);
    addRule(rules, R"('(?:[^'\\]|\\.)*')",                 clr::string_);
    addKeywords(rules, {"true", "false", "null", "yes", "no", "on", "off",
                        "True", "False", "Null", "Yes", "No", "On", "Off",
                        "TRUE", "FALSE", "NULL"}, clr::keyword);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\b)", clr::number_);
    addRule(rules, R"(#[^\n]*)",                           clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildToml(QVector<SyntaxHighlighter::Rule> &rules,
               QRegularExpression &blockStart,
               QRegularExpression &blockEnd,
               QTextCharFormat &blockFmt)
{
    addRule(rules, R"(^\s*\[\[?[^\]]*\]\]?)",              clr::keyword);
    addRule(rules, R"(^(\s*[A-Za-z_][A-Za-z0-9_.\-]*)\s*=)", clr::special_);
    addRule(rules, R"(""".*?"""|'''.*?''')",               clr::string_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                 clr::string_);
    addRule(rules, R"('[^']*')",                           clr::string_);
    addRule(rules, R"(\b\d{4}-\d{2}-\d{2}(?:T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})?)?\b)", clr::number_);
    addKeywords(rules, {"true", "false", "inf", "nan"}, clr::keyword);
    addRule(rules, R"(\b[0-9][0-9_]*(?:\.[0-9_]+)?(?:[eE][+-]?[0-9_]+)?\b)", clr::number_);
    addRule(rules, R"(#[^\n]*)",                           clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildIni(QVector<SyntaxHighlighter::Rule> &rules,
              QRegularExpression &blockStart,
              QRegularExpression &blockEnd,
              QTextCharFormat &blockFmt)
{
    addRule(rules, R"(^\s*\[[^\]]*\])",                    clr::keyword);
    addRule(rules, R"(^\s*[A-Za-z_][A-Za-z0-9_.\-]*\s*(?==))", clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                 clr::string_);
    addRule(rules, R"('[^']*')",                           clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)",           clr::number_);
    addKeywords(rules, {"true", "false", "yes", "no", "on", "off",
                        "True", "False", "Yes", "No"}, clr::keyword);
    addRule(rules, R"([;#][^\n]*)",                        clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildEnv(QVector<SyntaxHighlighter::Rule> &rules,
              QRegularExpression &blockStart,
              QRegularExpression &blockEnd,
              QTextCharFormat &blockFmt)
{
    addRule(rules, R"(^[A-Za-z_][A-Za-z0-9_]*(?==))",  clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",              clr::string_);
    addRule(rules, R"('[^']*')",                        clr::string_);
    addRule(rules, R"(\$\{?[A-Za-z_][A-Za-z0-9_]*\}?)", clr::preproc_);
    addRule(rules, R"(#[^\n]*)",                        clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildSql(QVector<SyntaxHighlighter::Rule> &rules,
              QRegularExpression &blockStart,
              QRegularExpression &blockEnd,
              QTextCharFormat &blockFmt)
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
    addRule(rules, R"(--[^\n]*)",                              clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

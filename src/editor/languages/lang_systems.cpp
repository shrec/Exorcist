#include "langcommon.h"

void buildRust(QVector<SyntaxHighlighter::Rule> &rules,
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

    addRule(rules, R"(\b[A-Za-z_][A-Za-z0-9_]*!)", clr::preproc_);
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

void buildGo(QVector<SyntaxHighlighter::Rule> &rules,
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

void buildZig(QVector<SyntaxHighlighter::Rule> &rules,
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

    addKeywords(rules,
                {"i8", "i16", "i32", "i64", "i128", "isize",
                 "u8", "u16", "u32", "u64", "u128", "usize",
                 "f16", "f32", "f64", "f128", "bool", "void",
                 "anyerror", "anyframe", "comptime_int", "comptime_float",
                 "type", "noreturn"},
                clr::type_);

    addRule(rules, R"(@[a-zA-Z_]\w*)", clr::preproc_);
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F_]+\b)", clr::number_);
    addRule(rules, R"(\b0[bB][01_]+\b)", clr::number_);
    addRule(rules, R"(\b0[oO][0-7_]+\b)", clr::number_);
    addRule(rules, R"(\b[a-zA-Z_]\w*\s*(?=\())", clr::function_);
    addRule(rules, R"(//[^\n]*)", clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildSwift(QVector<SyntaxHighlighter::Rule> &rules,
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
    addRule(rules, R"(#[A-Za-z]+\b)",                      clr::preproc_);
    addRule(rules, R"(@[A-Za-z_][A-Za-z0-9_]*)",           clr::preproc_);
    addRule(rules, R"(\\\([^)]*\))",                        clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",                  clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?[fF]?\b)",      clr::number_);
    addRule(rules, R"(//[^\n]*)",                           clr::comment_);
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

#include "doxygengenerator.h"

#include <QChar>
#include <QRegularExpression>
#include <QStringList>

namespace {

// Strip C++ qualifiers and trailing specifiers that are noise for Doxygen
// purposes ("const", "override", "final", "noexcept", "= default", "= delete",
// "= 0"). We only touch the *trailing* portion; the return type's "const" is
// preserved.
QString stripTrailingSpecifiers(QString s)
{
    static const QRegularExpression rx(
        R"(\s*(?:const|override|final|noexcept(?:\([^)]*\))?|throw\s*\([^)]*\)|=\s*(?:default|delete|0))(?:\s*(?:const|override|final|noexcept(?:\([^)]*\))?|throw\s*\([^)]*\)|=\s*(?:default|delete|0)))*\s*$)");
    s.remove(rx);
    return s.trimmed();
}

// Strip a leading template<...> prefix if present.
QString stripTemplatePrefix(QString s)
{
    s = s.trimmed();
    if (!s.startsWith(QLatin1String("template")))
        return s;
    int i = QString(QLatin1String("template")).size();
    while (i < s.size() && s.at(i).isSpace()) ++i;
    if (i >= s.size() || s.at(i) != QLatin1Char('<'))
        return s;
    int depth = 0;
    for (; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c == QLatin1Char('<')) ++depth;
        else if (c == QLatin1Char('>')) {
            --depth;
            if (depth == 0) { ++i; break; }
        }
    }
    return s.mid(i).trimmed();
}

// Split a parameter list (the contents of the outer parens) on top-level
// commas, respecting <>, () and {} nesting.
QStringList splitTopLevelCommas(const QString &paramList)
{
    QStringList out;
    QString cur;
    int angle = 0, paren = 0, brace = 0;
    for (const QChar c : paramList) {
        if (c == QLatin1Char(',') && angle == 0 && paren == 0 && brace == 0) {
            out << cur.trimmed();
            cur.clear();
            continue;
        }
        if (c == QLatin1Char('<')) ++angle;
        else if (c == QLatin1Char('>') && angle > 0) --angle;
        else if (c == QLatin1Char('(')) ++paren;
        else if (c == QLatin1Char(')') && paren > 0) --paren;
        else if (c == QLatin1Char('{')) ++brace;
        else if (c == QLatin1Char('}') && brace > 0) --brace;
        cur.append(c);
    }
    if (!cur.trimmed().isEmpty())
        out << cur.trimmed();
    return out;
}

// Locate matching ')' for the '(' at openIdx. Returns -1 if unbalanced.
int matchingClose(const QString &s, int openIdx)
{
    int depth = 0;
    for (int i = openIdx; i < s.size(); ++i) {
        if (s.at(i) == QLatin1Char('(')) ++depth;
        else if (s.at(i) == QLatin1Char(')')) {
            if (--depth == 0) return i;
        }
    }
    return -1;
}

// From a parameter "type + name" blob, split off the trailing identifier as
// `name` and the rest as `type`. Default values (`= ...`) are stripped first.
DoxygenGenerator::Param splitParamBlob(const QString &blobIn)
{
    DoxygenGenerator::Param p;
    QString blob = blobIn.trimmed();
    if (blob.isEmpty()) return p;

    // Pull off default value
    const int eq = [&]() {
        int depth = 0;
        for (int i = 0; i < blob.size(); ++i) {
            const QChar c = blob.at(i);
            if (c == QLatin1Char('<') || c == QLatin1Char('(')) ++depth;
            else if (c == QLatin1Char('>') || c == QLatin1Char(')')) {
                if (depth > 0) --depth;
            } else if (c == QLatin1Char('=') && depth == 0) {
                // Skip ==, <=, >=
                const QChar prev = i > 0 ? blob.at(i - 1) : QChar();
                const QChar next = i + 1 < blob.size() ? blob.at(i + 1) : QChar();
                if (prev == QLatin1Char('=') || prev == QLatin1Char('<')
                    || prev == QLatin1Char('>') || prev == QLatin1Char('!')
                    || next == QLatin1Char('=')) continue;
                return i;
            }
        }
        return -1;
    }();
    if (eq >= 0) {
        p.defaultValue = blob.mid(eq + 1).trimmed();
        blob = blob.left(eq).trimmed();
    }

    // Special-case: parameter pack / variadic — "..." with no name.
    if (blob == QLatin1String("...")) {
        p.type = QStringLiteral("...");
        return p;
    }

    // Walk from the right to find the last identifier (the param name).
    // Identifier = [A-Za-z_][A-Za-z0-9_]*. Skip trailing `[]` arrays first.
    int end = blob.size();
    while (end > 0 && (blob.at(end - 1) == QLatin1Char(']')
                       || blob.at(end - 1) == QLatin1Char('[')
                       || blob.at(end - 1).isDigit()
                       || blob.at(end - 1).isSpace())) --end;
    int idEnd = end;
    while (idEnd > 0) {
        const QChar c = blob.at(idEnd - 1);
        if (c.isLetterOrNumber() || c == QLatin1Char('_')) --idEnd;
        else break;
    }
    if (idEnd == end) {
        // No identifier → unnamed param. Treat the whole blob as type.
        p.type = blob;
        return p;
    }
    // Guard: identifier must start with letter/_ and not be a keyword that
    // is actually part of the type ("const", "volatile", etc).
    QString candidate = blob.mid(idEnd, end - idEnd);
    static const QStringList keywords = {
        QStringLiteral("const"),    QStringLiteral("volatile"),
        QStringLiteral("unsigned"), QStringLiteral("signed"),
        QStringLiteral("short"),    QStringLiteral("long"),
        QStringLiteral("int"),      QStringLiteral("char"),
        QStringLiteral("bool"),     QStringLiteral("float"),
        QStringLiteral("double"),   QStringLiteral("void"),
        QStringLiteral("auto"),
    };
    if (keywords.contains(candidate)) {
        // The blob is a bare type with no parameter name (e.g. "int").
        p.type = blob.trimmed();
        return p;
    }
    p.name = candidate;
    p.type = blob.left(idEnd).trimmed();
    // If user put array suffix in the type half, append it back so docs make sense.
    if (end < blob.size())
        p.type += blob.mid(end);
    return p;
}

bool isVoidReturn(const QString &returnType)
{
    const QString r = returnType.trimmed();
    if (r.isEmpty()) return false;
    // Strip leading qualifiers and exactly compare to "void".
    QString stripped = r;
    static const QRegularExpression head(
        R"(^(?:inline|static|virtual|explicit|constexpr|consteval|extern|friend|\[\[[^\]]*\]\])\s+)");
    while (true) {
        const QRegularExpressionMatch m = head.match(stripped);
        if (!m.hasMatch()) break;
        stripped = stripped.mid(m.capturedLength()).trimmed();
    }
    return stripped == QLatin1String("void");
}

} // namespace

// ── Public API ───────────────────────────────────────────────────────────────

DoxygenGenerator::ParsedSignature
DoxygenGenerator::parseSignature(const QString &lineIn)
{
    ParsedSignature sig;
    QString line = lineIn.trimmed();
    if (line.isEmpty()) return sig;

    // Strip comments at end of line (best-effort, does not handle all cases).
    int slashSlash = line.indexOf(QLatin1String("//"));
    if (slashSlash >= 0) line = line.left(slashSlash).trimmed();

    // Strip leading template<...>
    line = stripTemplatePrefix(line);
    if (line.isEmpty()) return sig;

    // Find the function's parameter list: the first '(' that is not inside
    // template angle brackets, and whose matching ')' lies before the body.
    int openParen = -1;
    {
        int depth = 0;
        for (int i = 0; i < line.size(); ++i) {
            const QChar c = line.at(i);
            if (c == QLatin1Char('<')) ++depth;
            else if (c == QLatin1Char('>') && depth > 0) --depth;
            else if (c == QLatin1Char('(') && depth == 0) {
                openParen = i;
                break;
            }
        }
    }
    if (openParen < 0) return sig;

    const int closeParen = matchingClose(line, openParen);
    if (closeParen < 0) return sig;

    // Head: everything before '(' = return-type + name (or just name for ctor/dtor).
    QString head = line.left(openParen).trimmed();
    // Tail: everything after ')'
    QString tail = line.mid(closeParen + 1).trimmed();
    // Strip trailing body markers
    if (tail.endsWith(QLatin1Char(';'))) tail.chop(1);
    if (tail.endsWith(QLatin1Char('{'))) tail.chop(1);
    tail = stripTrailingSpecifiers(tail.trimmed());

    if (head.isEmpty()) return sig;

    // Drop attribute prefixes [[nodiscard]] etc. from the head when extracting
    // the function name. We keep them in the return type for context though.
    // Find the function name = the rightmost identifier in `head`,
    // including a possible "Class::" qualifier.
    // Walk from the right, skipping spaces and pointer/ref tokens that may
    // sit between return-type and name (rare but legal for ctor name lookup).
    int end = head.size();
    while (end > 0 && head.at(end - 1).isSpace()) --end;

    // Identifier (name) — possibly preceded by "Class::" segments and
    // possibly preceded by "operator..." / "~" for dtors.
    int nameEnd = end;
    int nameStart = nameEnd;
    auto isIdChar = [](QChar c) {
        return c.isLetterOrNumber() || c == QLatin1Char('_');
    };
    while (nameStart > 0 && isIdChar(head.at(nameStart - 1))) --nameStart;
    // Allow "operator..." names: if the identifier we just collected is "operator"
    // OR if the preceding chars form an operator symbol, capture everything from
    // the previous "operator" token to nameEnd.
    {
        const int kw = head.lastIndexOf(QLatin1String("operator"), nameStart);
        if (kw >= 0) {
            // ensure it's a standalone word (preceded by space/start/::)
            const bool startOk = (kw == 0)
                || head.at(kw - 1).isSpace()
                || head.at(kw - 1) == QLatin1Char(':')
                || head.at(kw - 1) == QLatin1Char('&')
                || head.at(kw - 1) == QLatin1Char('*');
            if (startOk) nameStart = kw;
        }
    }
    // Allow "::" qualifier(s) before the name.
    while (nameStart >= 2
           && head.at(nameStart - 1) == QLatin1Char(':')
           && head.at(nameStart - 2) == QLatin1Char(':')) {
        int qEnd = nameStart - 2;
        int qStart = qEnd;
        while (qStart > 0 && isIdChar(head.at(qStart - 1))) --qStart;
        // Allow tilde for ~Foo::~Foo (rare); just keep walking
        if (qStart == qEnd) break;
        nameStart = qStart;
    }
    // Allow leading '~' for destructors.
    if (nameStart > 0 && head.at(nameStart - 1) == QLatin1Char('~'))
        --nameStart;

    if (nameStart >= nameEnd) return sig;
    sig.name = head.mid(nameStart, nameEnd - nameStart).trimmed();
    sig.returnType = head.left(nameStart).trimmed();

    // Detect ctor/dtor:
    //  - "~Foo" / "~Class::Foo" → destructor
    //  - empty returnType and name in form "Foo" or "Foo::Foo" → constructor
    if (sig.name.contains(QLatin1Char('~'))) {
        sig.isDestructor = true;
        sig.returnType.clear();
    } else if (sig.returnType.isEmpty()) {
        // Could be a ctor (Foo or Class::Foo with name segments equal).
        const int dc = sig.name.lastIndexOf(QLatin1String("::"));
        if (dc >= 0) {
            const QString cls = sig.name.left(dc);
            const QString fn  = sig.name.mid(dc + 2);
            if (!cls.isEmpty() && cls == fn)
                sig.isConstructor = true;
            else
                sig.isConstructor = true; // best-effort: empty return type
        } else {
            // No "::" — treat as ctor only if first letter is uppercase
            // (heuristic that mirrors C++ class-name convention).
            if (!sig.name.isEmpty() && sig.name.at(0).isUpper())
                sig.isConstructor = true;
        }
    }

    // Void detection
    sig.isVoid = isVoidReturn(sig.returnType);

    // Param parsing
    const QString paramBlob = line.mid(openParen + 1,
                                       closeParen - openParen - 1).trimmed();
    if (!paramBlob.isEmpty() && paramBlob != QLatin1String("void")) {
        const QStringList parts = splitTopLevelCommas(paramBlob);
        for (const QString &part : parts) {
            const Param p = splitParamBlob(part);
            if (!p.type.isEmpty() || !p.name.isEmpty())
                sig.params.append(p);
        }
    }

    sig.valid = true;
    return sig;
}

DoxygenGenerator::ParsedSignature
DoxygenGenerator::parseLine(const QString &rawLine)
{
    return parseSignature(rawLine);
}

QString DoxygenGenerator::buildComment(const ParsedSignature &sig,
                                       int indentSpaces)
{
    const QString indent(indentSpaces, QLatin1Char(' '));
    QString out;

    auto line = [&](const QString &content) {
        out += indent;
        out += QLatin1String("///");
        if (!content.isEmpty()) {
            out += QLatin1Char(' ');
            out += content;
        }
        out += QLatin1Char('\n');
    };

    if (!sig.valid) {
        // Fall back to a single bare comment marker — matches the requested UX
        // for "non-function lines": insert one `///` so the user can write
        // their own comment.
        line(QString());
        return out;
    }

    line(QStringLiteral("@brief "));
    if (!sig.params.isEmpty()) {
        line(QString());  // blank `///`
        for (const Param &p : sig.params) {
            const QString pname = p.name.isEmpty()
                ? QStringLiteral("param")
                : p.name;
            line(QStringLiteral("@param %1 ").arg(pname));
        }
    }
    if (!sig.isVoid && !sig.isConstructor && !sig.isDestructor
        && !sig.returnType.isEmpty()) {
        if (sig.params.isEmpty())
            line(QString());  // blank separator
        line(QStringLiteral("@return "));
    }
    return out;
}

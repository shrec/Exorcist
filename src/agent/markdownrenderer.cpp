#include "markdownrenderer.h"

#include <QRegularExpression>
#include <QStringList>

namespace MarkdownRenderer {

static QString escapeHtml(const QString &text)
{
    QString out = text;
    out.replace(QLatin1Char('&'),  QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'),  QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'),  QStringLiteral("&gt;"));
    return out;
}

// Process inline formatting: **bold**, ~~strikethrough~~, *italic*, `code`, [text](url), ![alt](img)
static QString processInline(const QString &line)
{
    QString out = escapeHtml(line);

    // Inline code: `code`  (must be first to protect code from other rules)
    static const QRegularExpression reCode(QStringLiteral("`([^`]+)`"));
    out.replace(reCode, QStringLiteral("<code>\\1</code>"));

    // Bold+Italic: ***text***
    static const QRegularExpression reBoldItalic(QStringLiteral("\\*\\*\\*(.+?)\\*\\*\\*"));
    out.replace(reBoldItalic, QStringLiteral("<b><i>\\1</i></b>"));

    // Bold: **text** or __text__
    static const QRegularExpression reBold(QStringLiteral("\\*\\*(.+?)\\*\\*"));
    out.replace(reBold, QStringLiteral("<b>\\1</b>"));
    static const QRegularExpression reBoldU(QStringLiteral("__(.+?)__"));
    out.replace(reBoldU, QStringLiteral("<b>\\1</b>"));

    // Strikethrough: ~~text~~
    static const QRegularExpression reStrike(QStringLiteral("~~(.+?)~~"));
    out.replace(reStrike, QStringLiteral("<s>\\1</s>"));

    // Italic: *text* (but not inside **)
    static const QRegularExpression reItalic(QStringLiteral("(?<!\\*)\\*([^*]+)\\*(?!\\*)"));
    out.replace(reItalic, QStringLiteral("<i>\\1</i>"));

    // Image: ![alt](url)
    static const QRegularExpression reImg(QStringLiteral("!\\[([^\\]]*)\\]\\(([^)]+)\\)"));
    out.replace(reImg, QStringLiteral("<img src='\\2' alt='\\1' style='max-width:100%;'>"));

    // Links: [text](url)
    static const QRegularExpression reLink(QStringLiteral("\\[([^\\]]+)\\]\\(([^)]+)\\)"));
    out.replace(reLink, QStringLiteral("<a href='\\2'>\\1</a>"));

    return out;
}

static QString extractFilePathHint(const QString &code, QString *stripped)
{
    const QString firstLine = code.section(QLatin1Char('\n'), 0, 0).trimmed();
    static const QStringList exts = {
        ".cpp", ".cxx", ".cc", ".c", ".h", ".hpp", ".hh",
        ".py", ".js", ".ts", ".tsx", ".jsx", ".java", ".cs",
        ".go", ".rs", ".rb", ".php", ".swift", ".kt", ".m", ".mm",
        ".html", ".css", ".scss", ".json", ".yml", ".yaml", ".toml",
        ".md", ".txt", ".sh", ".bash", ".zsh", ".ps1", ".bat", ".cmd"
    };

    for (const QString &ext : exts) {
        if (firstLine.endsWith(ext, Qt::CaseInsensitive)) {
            if (stripped) {
                const int nl = code.indexOf(QLatin1Char('\n'));
                *stripped = (nl >= 0) ? code.mid(nl + 1) : QString();
            }
            return firstLine;
        }
    }

    if (stripped)
        *stripped = code;
    return {};
}

// Detect if a trimmed line is a table separator row: |---|---|
static bool isTableSepRow(const QString &trimmed)
{
    if (!trimmed.startsWith(QLatin1Char('|')))
        return false;
    static const QRegularExpression reSep(
        QStringLiteral("^\\|[\\s:]*-{3,}[\\s:]*(?:\\|[\\s:]*-{3,}[\\s:]*)*\\|?$"));
    return reSep.match(trimmed).hasMatch();
}

static QStringList splitTableCells(const QString &row)
{
    QString inner = row.trimmed();
    if (inner.startsWith(QLatin1Char('|'))) inner = inner.mid(1);
    if (inner.endsWith(QLatin1Char('|')))   inner.chop(1);
    QStringList cells;
    const auto parts = inner.split(QLatin1Char('|'));
    for (const QString &p : parts)
        cells << p.trimmed();
    return cells;
}

// ── Syntax highlighter ────────────────────────────────────────────────────────

static QString highlightCode(const QString &lang, const QString &code)
{
    const QString lo = lang.toLower();
    if (lo.isEmpty() || lo == QLatin1String("text") || lo == QLatin1String("plaintext")
        || lo == QLatin1String("output"))
        return escapeHtml(code);

    // ── Language feature flags ────────────────────────────────────────────
    bool lineComSl = false, lineComH = false, blkCom = false;
    bool preProc = false, tripleQ = false;

    if (lo == QLatin1String("python") || lo == QLatin1String("py")) {
        lineComH = true; tripleQ = true;
    } else if (lo == QLatin1String("bash") || lo == QLatin1String("sh")
            || lo == QLatin1String("shell") || lo == QLatin1String("zsh")
            || lo == QLatin1String("powershell") || lo == QLatin1String("ps1")
            || lo == QLatin1String("cmake") || lo == QLatin1String("yaml")
            || lo == QLatin1String("yml") || lo == QLatin1String("ruby")
            || lo == QLatin1String("rb") || lo == QLatin1String("makefile")
            || lo == QLatin1String("make") || lo == QLatin1String("dockerfile")
            || lo == QLatin1String("toml") || lo == QLatin1String("r")) {
        lineComH = true;
    } else if (lo == QLatin1String("json")) {
        /* no comments */
    } else if (lo == QLatin1String("jsonc")) {
        lineComSl = true; blkCom = true;
    } else {
        lineComSl = true; blkCom = true;          // C-family default
    }
    if (lo == QLatin1String("cpp") || lo == QLatin1String("c")
        || lo == QLatin1String("c++") || lo == QLatin1String("cxx")
        || lo == QLatin1String("h") || lo == QLatin1String("hpp"))
        preProc = true;

    // ── Keyword sets ─────────────────────────────────────────────────────
    static const QSet<QString> sCtrl = {
        QStringLiteral("if"),       QStringLiteral("else"),     QStringLiteral("elif"),
        QStringLiteral("for"),      QStringLiteral("while"),    QStringLiteral("do"),
        QStringLiteral("switch"),   QStringLiteral("case"),     QStringLiteral("default"),
        QStringLiteral("break"),    QStringLiteral("continue"), QStringLiteral("return"),
        QStringLiteral("throw"),    QStringLiteral("try"),      QStringLiteral("catch"),
        QStringLiteral("finally"),  QStringLiteral("except"),   QStringLiteral("raise"),
        QStringLiteral("yield"),    QStringLiteral("match"),    QStringLiteral("with"),
        QStringLiteral("async"),    QStringLiteral("await"),    QStringLiteral("goto"),
        QStringLiteral("loop"),
    };

    static const QSet<QString> sCKw = {
        QStringLiteral("auto"),     QStringLiteral("class"),    QStringLiteral("const"),
        QStringLiteral("constexpr"),QStringLiteral("delete"),   QStringLiteral("enum"),
        QStringLiteral("explicit"), QStringLiteral("extern"),   QStringLiteral("false"),
        QStringLiteral("final"),    QStringLiteral("friend"),   QStringLiteral("inline"),
        QStringLiteral("mutable"),  QStringLiteral("namespace"),QStringLiteral("new"),
        QStringLiteral("noexcept"), QStringLiteral("nullptr"),  QStringLiteral("null"),
        QStringLiteral("operator"), QStringLiteral("override"), QStringLiteral("private"),
        QStringLiteral("protected"),QStringLiteral("public"),   QStringLiteral("register"),
        QStringLiteral("sizeof"),   QStringLiteral("static"),   QStringLiteral("static_assert"),
        QStringLiteral("struct"),   QStringLiteral("template"), QStringLiteral("this"),
        QStringLiteral("true"),     QStringLiteral("typedef"),  QStringLiteral("typeid"),
        QStringLiteral("typename"), QStringLiteral("union"),    QStringLiteral("using"),
        QStringLiteral("virtual"),  QStringLiteral("void"),     QStringLiteral("volatile"),
        QStringLiteral("concept"),  QStringLiteral("requires"), QStringLiteral("module"),
        QStringLiteral("import"),   QStringLiteral("export"),   QStringLiteral("co_await"),
        QStringLiteral("co_return"),QStringLiteral("co_yield"),
        QStringLiteral("var"),      QStringLiteral("let"),      QStringLiteral("function"),
        QStringLiteral("from"),     QStringLiteral("of"),       QStringLiteral("in"),
        QStringLiteral("instanceof"),QStringLiteral("typeof"),  QStringLiteral("extends"),
        QStringLiteral("implements"),QStringLiteral("interface"),QStringLiteral("abstract"),
        QStringLiteral("declare"),  QStringLiteral("type"),     QStringLiteral("readonly"),
        QStringLiteral("fn"),       QStringLiteral("impl"),     QStringLiteral("trait"),
        QStringLiteral("pub"),      QStringLiteral("mod"),      QStringLiteral("use"),
        QStringLiteral("crate"),    QStringLiteral("self"),     QStringLiteral("super"),
        QStringLiteral("mut"),      QStringLiteral("ref"),      QStringLiteral("move"),
        QStringLiteral("unsafe"),   QStringLiteral("where"),    QStringLiteral("as"),
        QStringLiteral("func"),     QStringLiteral("package"),  QStringLiteral("defer"),
        QStringLiteral("go"),       QStringLiteral("select"),   QStringLiteral("chan"),
        QStringLiteral("range"),    QStringLiteral("fallthrough"),
        QStringLiteral("const_cast"),QStringLiteral("dynamic_cast"),
        QStringLiteral("reinterpret_cast"),QStringLiteral("static_cast"),
    };

    static const QSet<QString> sCTy = {
        QStringLiteral("int"),      QStringLiteral("long"),     QStringLiteral("short"),
        QStringLiteral("char"),     QStringLiteral("float"),    QStringLiteral("double"),
        QStringLiteral("bool"),     QStringLiteral("unsigned"), QStringLiteral("signed"),
        QStringLiteral("size_t"),   QStringLiteral("wchar_t"),  QStringLiteral("string"),
        QStringLiteral("vector"),   QStringLiteral("map"),      QStringLiteral("set"),
        QStringLiteral("list"),     QStringLiteral("array"),    QStringLiteral("pair"),
        QStringLiteral("tuple"),    QStringLiteral("optional"), QStringLiteral("variant"),
        QStringLiteral("any"),      QStringLiteral("shared_ptr"),QStringLiteral("unique_ptr"),
        QStringLiteral("weak_ptr"), QStringLiteral("QString"),  QStringLiteral("QList"),
        QStringLiteral("QVector"),  QStringLiteral("QMap"),     QStringLiteral("QSet"),
        QStringLiteral("QHash"),    QStringLiteral("QVariant"), QStringLiteral("QObject"),
        QStringLiteral("QWidget"),  QStringLiteral("QStringList"),QStringLiteral("QByteArray"),
        QStringLiteral("number"),   QStringLiteral("boolean"),  QStringLiteral("undefined"),
        QStringLiteral("Symbol"),   QStringLiteral("BigInt"),   QStringLiteral("Array"),
        QStringLiteral("Object"),   QStringLiteral("String"),   QStringLiteral("Number"),
        QStringLiteral("Boolean"),  QStringLiteral("Date"),     QStringLiteral("Promise"),
        QStringLiteral("Map"),      QStringLiteral("Set"),      QStringLiteral("Error"),
        QStringLiteral("RegExp"),   QStringLiteral("Function"), QStringLiteral("i32"),
        QStringLiteral("i64"),      QStringLiteral("u32"),      QStringLiteral("u64"),
        QStringLiteral("f32"),      QStringLiteral("f64"),      QStringLiteral("str"),
        QStringLiteral("Vec"),      QStringLiteral("Box"),      QStringLiteral("Rc"),
        QStringLiteral("Arc"),      QStringLiteral("Option"),   QStringLiteral("Result"),
        QStringLiteral("Some"),     QStringLiteral("None"),     QStringLiteral("Ok"),
        QStringLiteral("Err"),
    };

    static const QSet<QString> sPyKw = {
        QStringLiteral("and"),      QStringLiteral("as"),       QStringLiteral("assert"),
        QStringLiteral("class"),    QStringLiteral("def"),      QStringLiteral("del"),
        QStringLiteral("False"),    QStringLiteral("from"),     QStringLiteral("global"),
        QStringLiteral("import"),   QStringLiteral("in"),       QStringLiteral("is"),
        QStringLiteral("lambda"),   QStringLiteral("None"),     QStringLiteral("nonlocal"),
        QStringLiteral("not"),      QStringLiteral("or"),       QStringLiteral("pass"),
        QStringLiteral("True"),
    };

    static const QSet<QString> sPyTy = {
        QStringLiteral("int"),      QStringLiteral("float"),    QStringLiteral("str"),
        QStringLiteral("bool"),     QStringLiteral("list"),     QStringLiteral("dict"),
        QStringLiteral("set"),      QStringLiteral("tuple"),    QStringLiteral("bytes"),
        QStringLiteral("type"),     QStringLiteral("object"),   QStringLiteral("range"),
        QStringLiteral("property"), QStringLiteral("super"),    QStringLiteral("Exception"),
        QStringLiteral("ValueError"),QStringLiteral("TypeError"),QStringLiteral("KeyError"),
        QStringLiteral("print"),    QStringLiteral("len"),      QStringLiteral("enumerate"),
        QStringLiteral("zip"),      QStringLiteral("map"),      QStringLiteral("filter"),
        QStringLiteral("sorted"),   QStringLiteral("reversed"), QStringLiteral("any"),
        QStringLiteral("all"),      QStringLiteral("isinstance"),QStringLiteral("open"),
        QStringLiteral("input"),    QStringLiteral("abs"),      QStringLiteral("round"),
        QStringLiteral("min"),      QStringLiteral("max"),      QStringLiteral("sum"),
        QStringLiteral("iter"),     QStringLiteral("next"),     QStringLiteral("getattr"),
        QStringLiteral("setattr"),  QStringLiteral("hasattr"),  QStringLiteral("callable"),
        QStringLiteral("repr"),     QStringLiteral("hash"),     QStringLiteral("id"),
    };

    const bool isPy = (lo == QLatin1String("python") || lo == QLatin1String("py"));
    const QSet<QString> &keywords = isPy ? sPyKw : sCKw;
    const QSet<QString> &types    = isPy ? sPyTy : sCTy;

    // ── Colors (VS Code Dark Modern) ─────────────────────────────────────
    static constexpr const char *cKw   = "#569cd6";
    static constexpr const char *cCtrl = "#c586c0";
    static constexpr const char *cTy   = "#4ec9b0";
    static constexpr const char *cStr  = "#ce9178";
    static constexpr const char *cNum  = "#b5cea8";
    static constexpr const char *cCom  = "#6a9955";
    static constexpr const char *cPre  = "#c586c0";
    static constexpr const char *cFn   = "#dcdcaa";

    // ── Scanner ──────────────────────────────────────────────────────────
    const int len = code.size();
    QString out;
    out.reserve(len * 2);
    int i = 0;
    bool lineStart = true;

    auto peek = [&](int off) -> QChar {
        const int p = i + off;
        return (p >= 0 && p < len) ? code[p] : QChar();
    };
    auto emitCh = [&](QChar c) {
        if (c == QLatin1Char('<'))      out += QLatin1String("&lt;");
        else if (c == QLatin1Char('>')) out += QLatin1String("&gt;");
        else if (c == QLatin1Char('&')) out += QLatin1String("&amp;");
        else out += c;
    };
    auto span = [&](const char *col, const QString &t) {
        out += QLatin1String("<span style='color:");
        out += QLatin1String(col);
        out += QLatin1String("'>");
        out += escapeHtml(t);
        out += QLatin1String("</span>");
    };

    while (i < len) {
        const QChar ch = code[i];

        // ── Newline ──────────────────────────────────────────────────
        if (ch == QLatin1Char('\n')) { out += QLatin1Char('\n'); ++i; lineStart = true; continue; }

        // ── Preprocessor (#include etc.) at line start ───────────────
        if (preProc && lineStart && ch == QLatin1Char('#')) {
            int s = i;
            while (i < len && code[i] != QLatin1Char('\n')) ++i;
            span(cPre, code.mid(s, i - s));
            continue;
        }
        if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t'))
            lineStart = false;

        // ── Block comment /* ... */ ──────────────────────────────────
        if (blkCom && ch == QLatin1Char('/') && peek(1) == QLatin1Char('*')) {
            int s = i; i += 2;
            while (i < len) {
                if (code[i] == QLatin1Char('*') && peek(1) == QLatin1Char('/')) { i += 2; break; }
                ++i;
            }
            span(cCom, code.mid(s, i - s));
            continue;
        }

        // ── Line comment // or # ────────────────────────────────────
        if ((lineComSl && ch == QLatin1Char('/') && peek(1) == QLatin1Char('/'))
            || (lineComH && ch == QLatin1Char('#'))) {
            int s = i;
            while (i < len && code[i] != QLatin1Char('\n')) ++i;
            span(cCom, code.mid(s, i - s));
            continue;
        }

        // ── Triple-quoted strings (Python) ──────────────────────────
        if (tripleQ && (ch == QLatin1Char('"') || ch == QLatin1Char('\''))
            && peek(1) == ch && peek(2) == ch) {
            QChar q = ch; int s = i; i += 3;
            while (i < len) {
                if (code[i] == QLatin1Char('\\')) { i += 2; continue; }
                if (code[i] == q && peek(1) == q && peek(2) == q) { i += 3; break; }
                ++i;
            }
            span(cStr, code.mid(s, i - s));
            continue;
        }

        // ── String / char literal ───────────────────────────────────
        if (ch == QLatin1Char('"') || ch == QLatin1Char('\'') || ch == QLatin1Char('`')) {
            QChar q = ch; int s = i; ++i;
            while (i < len && code[i] != QLatin1Char('\n')) {
                if (code[i] == QLatin1Char('\\')) { i += 2; continue; }
                if (code[i] == q) { ++i; break; }
                ++i;
            }
            span(cStr, code.mid(s, i - s));
            continue;
        }

        // ── Number ──────────────────────────────────────────────────
        if (ch.isDigit() || (ch == QLatin1Char('.') && peek(1).isDigit())) {
            int s = i;
            if (ch == QLatin1Char('0') && (peek(1) == QLatin1Char('x') || peek(1) == QLatin1Char('X'))) {
                i += 2;
                while (i < len && (code[i].isDigit()
                    || (code[i] >= QLatin1Char('a') && code[i] <= QLatin1Char('f'))
                    || (code[i] >= QLatin1Char('A') && code[i] <= QLatin1Char('F')))) ++i;
            } else {
                while (i < len && (code[i].isDigit() || code[i] == QLatin1Char('.'))) ++i;
                if (i < len && (code[i] == QLatin1Char('e') || code[i] == QLatin1Char('E'))) {
                    ++i;
                    if (i < len && (code[i] == QLatin1Char('+') || code[i] == QLatin1Char('-'))) ++i;
                    while (i < len && code[i].isDigit()) ++i;
                }
            }
            while (i < len && QString(QLatin1String("fFuUlL")).contains(code[i])) ++i;
            span(cNum, code.mid(s, i - s));
            continue;
        }

        // ── Identifier / keyword ────────────────────────────────────
        if (ch.isLetter() || ch == QLatin1Char('_')) {
            int s = i;
            while (i < len && (code[i].isLetterOrNumber() || code[i] == QLatin1Char('_'))) ++i;
            const QString w = code.mid(s, i - s);

            // Peek ahead for function call '('
            int j = i;
            while (j < len && code[j] == QLatin1Char(' ')) ++j;
            const bool isCall = (j < len && code[j] == QLatin1Char('('));

            if (sCtrl.contains(w))
                span(cCtrl, w);
            else if (types.contains(w))
                span(cTy, w);
            else if (keywords.contains(w))
                span(cKw, w);
            else if (isCall)
                span(cFn, w);
            else if (!w.isEmpty() && w[0].isUpper())
                span(cTy, w);                    // PascalCase → likely a type
            else
                out += escapeHtml(w);
            continue;
        }

        // ── Default char ────────────────────────────────────────────
        emitCh(ch);
        ++i;
    }

    return out;
}

// ── Code block emitter ────────────────────────────────────────────────────────

static void emitCodeBlock(QString &html, QList<CodeBlock> &blocks,
                          const QString &lang, const QString &code, int &idx)
{
    CodeBlock block;
    block.index    = idx;
    block.language = lang;
    QString stripped;
    block.filePath = extractFilePathHint(code, &stripped);
    block.code     = stripped;
    blocks.append(block);

    const QString langLabel = lang.isEmpty()
        ? QStringLiteral("text")
        : lang.toLower();

    const QString codeHtml = highlightCode(lang, stripped);
    html += QStringLiteral("<pre><code class='language-%1'>%2</code></pre>")
                .arg(langLabel.toHtmlEscaped(), codeHtml);

    html += QStringLiteral(
        "<div style='font-size:11px; color:#7a7a7a; padding:2px 0 10px 2px;'>"
        "<a href='codeblock://%1/apply' style='color:#4daafc;text-decoration:none;'>Apply</a>"
        "&nbsp;&middot;&nbsp;"
        "<a href='codeblock://%1/insert' style='color:#4daafc;text-decoration:none;'>Insert</a>"
        "&nbsp;&middot;&nbsp;"
        "<a href='codeblock://%1/copy' style='color:#4daafc;text-decoration:none;'>Copy</a>")
        .arg(idx);

    static const QSet<QString> shellLangs = {
        QStringLiteral("sh"), QStringLiteral("bash"), QStringLiteral("shell"),
        QStringLiteral("zsh"), QStringLiteral("pwsh"), QStringLiteral("powershell"),
        QStringLiteral("bat"), QStringLiteral("cmd")
    };
    if (shellLangs.contains(langLabel)) {
        html += QStringLiteral("&nbsp;&middot;&nbsp;"
                               "<a href='codeblock://%1/run' style='color:#4daafc;"
                               "text-decoration:none;'>Run</a>").arg(idx);
    }

    html += QStringLiteral("</div>");
    ++idx;
}

// ── Main renderer ─────────────────────────────────────────────────────────────

RenderResult toHtmlWithActions(const QString &markdown)
{
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    RenderResult result;
    QString &html = result.html;

    bool inCodeBlock = false;
    QString codeAccum;
    QString codeLang;
    int codeIndex = 0;

    // List tracking
    bool inUl = false;
    bool inOl = false;
    // Blockquote tracking
    bool inBlockquote = false;
    // Table tracking
    bool inTable = false;
    bool tableHeaderDone = false;
    QStringList pendingTableHeader;

    // Helpers to close open block elements
    auto closeUl = [&]() { if (inUl) { html += QStringLiteral("</ul>"); inUl = false; } };
    auto closeOl = [&]() { if (inOl) { html += QStringLiteral("</ol>"); inOl = false; } };
    auto closeBq = [&]() { if (inBlockquote) { html += QStringLiteral("</blockquote>"); inBlockquote = false; } };
    auto closeTable = [&]() {
        if (inTable) {
            html += QStringLiteral("</tbody></table>");
            inTable = false;
            tableHeaderDone = false;
            pendingTableHeader.clear();
        }
    };
    auto closeAllBlocks = [&]() { closeUl(); closeOl(); closeBq(); closeTable(); };

    static const QRegularExpression reOrderedList(QStringLiteral("^(\\s*)(\\d+)\\.\\s+(.*)$"));

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];

        // ── Fenced code block ─────────────────────────────────────────────
        if (line.trimmed().startsWith(QLatin1String("```"))) {
            if (!inCodeBlock) {
                closeAllBlocks();
                inCodeBlock = true;
                codeLang = line.trimmed().mid(3).trimmed();
                codeAccum.clear();
            } else {
                emitCodeBlock(html, result.codeBlocks, codeLang, codeAccum, codeIndex);
                inCodeBlock = false;
                codeAccum.clear();
                codeLang.clear();
            }
            continue;
        }

        if (inCodeBlock) {
            if (!codeAccum.isEmpty())
                codeAccum += QLatin1Char('\n');
            codeAccum += line;
            continue;
        }

        const QString trimmed = line.trimmed();

        // ── Horizontal rule: --- / *** / ___ (3+ chars) ───────────────────
        if (trimmed.size() >= 3) {
            bool allDash = true, allStar = true, allUnder = true;
            for (const QChar &ch : trimmed) {
                if (ch != QLatin1Char('-') && ch != QLatin1Char(' ')) allDash = false;
                if (ch != QLatin1Char('*') && ch != QLatin1Char(' ')) allStar = false;
                if (ch != QLatin1Char('_') && ch != QLatin1Char(' ')) allUnder = false;
            }
            if (allDash || allStar || allUnder) {
                // Ensure at least 3 non-space chars
                int count = 0;
                for (const QChar &ch : trimmed)
                    if (ch != QLatin1Char(' ')) ++count;
                if (count >= 3 && !trimmed.startsWith(QLatin1String("- "))) {
                    closeAllBlocks();
                    html += QStringLiteral("<hr>");
                    continue;
                }
            }
        }

        // ── Table detection ───────────────────────────────────────────────
        if (trimmed.startsWith(QLatin1Char('|')) && trimmed.contains(QLatin1String("|"))) {
            if (!inTable && !isTableSepRow(trimmed)) {
                // First row = header candidate
                pendingTableHeader = splitTableCells(trimmed);
                // Peek next line for separator
                if (i + 1 < lines.size() && isTableSepRow(lines[i + 1].trimmed())) {
                    closeAllBlocks();
                    inTable = true;
                    html += QStringLiteral("<table><thead><tr>");
                    for (const QString &cell : std::as_const(pendingTableHeader))
                        html += QStringLiteral("<th>%1</th>").arg(processInline(cell));
                    html += QStringLiteral("</tr></thead><tbody>");
                    tableHeaderDone = false;
                    continue;
                }
            }
            if (inTable) {
                if (isTableSepRow(trimmed)) {
                    tableHeaderDone = true;
                    continue;  // skip separator row
                }
                // Data row
                const QStringList cells = splitTableCells(trimmed);
                html += QStringLiteral("<tr>");
                for (const QString &cell : cells)
                    html += QStringLiteral("<td>%1</td>").arg(processInline(cell));
                html += QStringLiteral("</tr>");
                continue;
            }
        } else {
            closeTable();
        }

        // ── Blockquote: > text ────────────────────────────────────────────
        if (trimmed.startsWith(QLatin1String("> ")) || trimmed == QLatin1String(">")) {
            closeUl(); closeOl(); closeTable();
            if (!inBlockquote) {
                inBlockquote = true;
                html += QStringLiteral("<blockquote>");
            }
            const QString content = (trimmed.size() > 2) ? trimmed.mid(2) : QString();
            html += processInline(content) + QStringLiteral("<br>");
            continue;
        } else {
            closeBq();
        }

        // ── Headers ───────────────────────────────────────────────────────
        if (trimmed.startsWith(QLatin1String("#### "))) {
            closeAllBlocks();
            html += QStringLiteral("<h4>%1</h4>").arg(processInline(trimmed.mid(5)));
            continue;
        }
        if (trimmed.startsWith(QLatin1String("### "))) {
            closeAllBlocks();
            html += QStringLiteral("<h3>%1</h3>").arg(processInline(trimmed.mid(4)));
            continue;
        }
        if (trimmed.startsWith(QLatin1String("## "))) {
            closeAllBlocks();
            html += QStringLiteral("<h2>%1</h2>").arg(processInline(trimmed.mid(3)));
            continue;
        }
        if (trimmed.startsWith(QLatin1String("# "))) {
            closeAllBlocks();
            html += QStringLiteral("<h1>%1</h1>").arg(processInline(trimmed.mid(2)));
            continue;
        }

        // ── Unordered list: - item / * item ───────────────────────────────
        if (trimmed.startsWith(QLatin1String("- ")) ||
            trimmed.startsWith(QLatin1String("* "))) {
            closeOl(); closeTable(); closeBq();
            if (!inUl) {
                inUl = true;
                html += QStringLiteral("<ul>");
            }
            html += QStringLiteral("<li>%1</li>").arg(processInline(trimmed.mid(2)));
            continue;
        }

        // ── Checkbox list: - [ ] / - [x] ─────────────────────────────────
        if (trimmed.startsWith(QLatin1String("- [ ] ")) ||
            trimmed.startsWith(QLatin1String("- [x] ")) ||
            trimmed.startsWith(QLatin1String("- [X] "))) {
            closeOl(); closeTable(); closeBq();
            if (!inUl) {
                inUl = true;
                html += QStringLiteral("<ul style='list-style:none;padding-left:0;'>");
            }
            const bool checked = trimmed[3] != QLatin1Char(' ');
            const QString checkbox = checked
                ? QStringLiteral("&#9745; ") : QStringLiteral("&#9744; ");
            html += QStringLiteral("<li>%1%2</li>")
                .arg(checkbox, processInline(trimmed.mid(6)));
            continue;
        }

        // ── Ordered list: 1. item / 2. item ──────────────────────────────
        const auto olMatch = reOrderedList.match(line);
        if (olMatch.hasMatch()) {
            closeUl(); closeTable(); closeBq();
            if (!inOl) {
                inOl = true;
                html += QStringLiteral("<ol>");
            }
            html += QStringLiteral("<li>%1</li>").arg(processInline(olMatch.captured(3)));
            continue;
        }

        // If we get here, close any open list/block
        closeUl(); closeOl();

        // ── Blank line ────────────────────────────────────────────────────
        if (trimmed.isEmpty()) {
            html += QStringLiteral("<br>");
            continue;
        }

        // ── Normal paragraph text ─────────────────────────────────────────
        html += processInline(trimmed) + QStringLiteral("<br>");
    }

    // Close any open blocks
    closeAllBlocks();

    // Handle unclosed code block
    if (inCodeBlock && !codeAccum.isEmpty())
        emitCodeBlock(html, result.codeBlocks, codeLang, codeAccum, codeIndex);

    return result;
}

QString toHtml(const QString &markdown)
{
    return toHtmlWithActions(markdown).html;
}

} // namespace MarkdownRenderer

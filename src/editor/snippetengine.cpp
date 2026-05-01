#include "snippetengine.h"

#include <QDebug>
#include <QRegularExpression>

// ── SnippetEngine ────────────────────────────────────────────────────────────

SnippetEngine::SnippetEngine(QObject *parent)
    : QObject(parent)
{
#ifndef NDEBUG
    // Smoke test the expander once per process. Failure here means the
    // placeholder regex / replacement loop has regressed.
    if (!runSelfTest()) {
        qWarning() << "SnippetEngine::runSelfTest() failed — placeholder "
                      "expansion is broken";
    }
#endif
}

void SnippetEngine::registerSnippet(const Snippet &s)
{
    // Overwrite any previous entry with the same (trigger, language) key so
    // re-registration is idempotent (handy for plugin reloads).
    for (int i = 0; i < m_snippets.size(); ++i) {
        if (m_snippets[i].trigger == s.trigger
            && m_snippets[i].language == s.language) {
            m_snippets[i] = s;
            return;
        }
    }
    m_snippets.append(s);
}

const SnippetEngine::Snippet *SnippetEngine::find(const QString &trigger,
                                                  const QString &language) const
{
    const Snippet *fallback = nullptr;
    for (const Snippet &s : m_snippets) {
        if (s.trigger != trigger)
            continue;
        if (!language.isEmpty() && s.language == language)
            return &s; // exact language match wins
        if (s.language.isEmpty() && !fallback)
            fallback = &s; // language-agnostic match
        if (language.isEmpty() && !fallback)
            fallback = &s;
    }
    return fallback;
}

void SnippetEngine::loadBuiltins()
{
    // ── C++ ──────────────────────────────────────────────────────────────
    registerSnippet({
        QStringLiteral("for"),
        QStringLiteral("for loop"),
        QStringLiteral("for(int ${1:i} = 0; ${1:i} < ${2:N}; ++${1:i}) {\n    ${0}\n}"),
        QStringLiteral("cpp"),
    });
    registerSnippet({
        QStringLiteral("if"),
        QStringLiteral("if statement"),
        QStringLiteral("if (${1:condition}) {\n    ${0}\n}"),
        QStringLiteral("cpp"),
    });
    registerSnippet({
        QStringLiteral("while"),
        QStringLiteral("while loop"),
        QStringLiteral("while (${1:condition}) {\n    ${0}\n}"),
        QStringLiteral("cpp"),
    });
    registerSnippet({
        QStringLiteral("class"),
        QStringLiteral("class definition"),
        QStringLiteral("class ${1:Name} {\npublic:\n    ${1:Name}();\n    ~${1:Name}();\n\n"
                       "private:\n    ${0}\n};"),
        QStringLiteral("cpp"),
    });
    registerSnippet({
        QStringLiteral("struct"),
        QStringLiteral("struct definition"),
        QStringLiteral("struct ${1:Name} {\n    ${0}\n};"),
        QStringLiteral("cpp"),
    });
    registerSnippet({
        QStringLiteral("switch"),
        QStringLiteral("switch statement"),
        QStringLiteral("switch (${1:value}) {\ncase ${2:label}:\n    ${0}\n    break;\ndefault:\n    break;\n}"),
        QStringLiteral("cpp"),
    });
    registerSnippet({
        QStringLiteral("lambda"),
        QStringLiteral("C++ lambda"),
        QStringLiteral("[${1:&}](${2:auto x}) {\n    ${0}\n}"),
        QStringLiteral("cpp"),
    });
    registerSnippet({
        QStringLiteral("inc"),
        QStringLiteral("#include <header>"),
        QStringLiteral("#include <${1:header}>${0}"),
        QStringLiteral("cpp"),
    });

    // ── Python ───────────────────────────────────────────────────────────
    registerSnippet({
        QStringLiteral("if"),
        QStringLiteral("if statement"),
        QStringLiteral("if ${1:condition}:\n    ${0}"),
        QStringLiteral("py"),
    });
    registerSnippet({
        QStringLiteral("for"),
        QStringLiteral("for loop"),
        QStringLiteral("for ${1:item} in ${2:iterable}:\n    ${0}"),
        QStringLiteral("py"),
    });
    registerSnippet({
        QStringLiteral("def"),
        QStringLiteral("function definition"),
        QStringLiteral("def ${1:name}(${2:args}):\n    ${0}"),
        QStringLiteral("py"),
    });
    registerSnippet({
        QStringLiteral("class"),
        QStringLiteral("class definition"),
        QStringLiteral("class ${1:Name}:\n    def __init__(self${2:, args}):\n        ${0}"),
        QStringLiteral("py"),
    });
    registerSnippet({
        QStringLiteral("try"),
        QStringLiteral("try/except"),
        QStringLiteral("try:\n    ${1:pass}\nexcept ${2:Exception} as ${3:e}:\n    ${0}"),
        QStringLiteral("py"),
    });
    registerSnippet({
        QStringLiteral("with"),
        QStringLiteral("with statement"),
        QStringLiteral("with ${1:expr} as ${2:name}:\n    ${0}"),
        QStringLiteral("py"),
    });
}

// ── expand() ─────────────────────────────────────────────────────────────────
//
// Single-pass scan of the body. We don't reuse QString::replace because we
// need to record the resulting placeholder positions. The grammar is:
//   ${N:default}   greedy match — `default` may not contain '}'
//   ${N}           empty default
//   $N             short form, N is a run of ASCII digits
// Anything else passes through verbatim, including escaped dollar signs (we
// support `\$` as a literal $).
SnippetEngine::ExpandResult SnippetEngine::expand(const QString &body)
{
    ExpandResult out;
    out.text.reserve(body.size());

    int i = 0;
    const int n = body.size();
    while (i < n) {
        const QChar c = body.at(i);

        // Literal escape: "\$" -> "$"
        if (c == QLatin1Char('\\') && i + 1 < n
            && body.at(i + 1) == QLatin1Char('$')) {
            out.text.append(QLatin1Char('$'));
            i += 2;
            continue;
        }

        if (c != QLatin1Char('$')) {
            out.text.append(c);
            ++i;
            continue;
        }

        // c == '$'. Try to match the three placeholder forms.
        // ── ${N} or ${N:default} ─────────────────────────────────────────
        if (i + 1 < n && body.at(i + 1) == QLatin1Char('{')) {
            // Read digits
            int j = i + 2;
            int digitStart = j;
            while (j < n && body.at(j).isDigit())
                ++j;
            if (j == digitStart || j >= n) {
                // Not a placeholder ("${" without digits) — pass through
                out.text.append(c);
                ++i;
                continue;
            }
            bool ok = false;
            const int idx = body.mid(digitStart, j - digitStart).toInt(&ok);
            if (!ok) {
                out.text.append(c);
                ++i;
                continue;
            }

            QString def;
            if (body.at(j) == QLatin1Char(':')) {
                ++j;
                const int defStart = j;
                while (j < n && body.at(j) != QLatin1Char('}'))
                    ++j;
                if (j >= n) {
                    // Unterminated — emit literal up to here
                    out.text.append(body.mid(i, j - i));
                    i = j;
                    continue;
                }
                def = body.mid(defStart, j - defStart);
            }
            if (j >= n || body.at(j) != QLatin1Char('}')) {
                // Unterminated (no closing brace)
                out.text.append(c);
                ++i;
                continue;
            }
            // j now points to the closing '}'
            const int rangeStart = out.text.size();
            out.text.append(def);
            const int rangeEnd = out.text.size();
            out.placeholderRanges[idx].append(qMakePair(rangeStart, rangeEnd));
            i = j + 1;
            continue;
        }

        // ── $N (short form) ──────────────────────────────────────────────
        if (i + 1 < n && body.at(i + 1).isDigit()) {
            int j = i + 1;
            const int digitStart = j;
            while (j < n && body.at(j).isDigit())
                ++j;
            bool ok = false;
            const int idx = body.mid(digitStart, j - digitStart).toInt(&ok);
            if (!ok) {
                out.text.append(c);
                ++i;
                continue;
            }
            const int rangeStart = out.text.size();
            // Empty default for short form
            const int rangeEnd = rangeStart;
            out.placeholderRanges[idx].append(qMakePair(rangeStart, rangeEnd));
            i = j;
            continue;
        }

        // Lone '$' — pass through
        out.text.append(c);
        ++i;
    }

    return out;
}

// ── Self-test ────────────────────────────────────────────────────────────────
bool SnippetEngine::runSelfTest()
{
    const QString body =
        QStringLiteral("for(int ${1:i} = 0; ${1:i} < ${2:N}; ++${1:i}) {\n    ${0}\n}");
    const ExpandResult r = expand(body);

    const QString expected =
        QStringLiteral("for(int i = 0; i < N; ++i) {\n    \n}");
    if (r.text != expected) {
        qWarning() << "SnippetEngine self-test: text mismatch.\n"
                      "  expected:" << expected
                   << "\n  got:" << r.text;
        return false;
    }

    // Tab stop 1 ("i") should appear three times.
    const auto stop1 = r.placeholderRanges.value(1);
    if (stop1.size() != 3) {
        qWarning() << "SnippetEngine self-test: expected 3 ranges for ${1}, got"
                   << stop1.size();
        return false;
    }
    // Each occurrence of ${1:i} expands to a single character "i" → end-start == 1.
    for (const auto &r1 : stop1) {
        if (r1.second - r1.first != 1) {
            qWarning() << "SnippetEngine self-test: ${1} range size != 1:"
                       << r1.first << r1.second;
            return false;
        }
        if (expected.mid(r1.first, 1) != QStringLiteral("i")) {
            qWarning() << "SnippetEngine self-test: ${1} range does not point at 'i'";
            return false;
        }
    }

    // Tab stop 2 ("N") should appear once.
    const auto stop2 = r.placeholderRanges.value(2);
    if (stop2.size() != 1 || stop2.first().second - stop2.first().first != 1
        || expected.mid(stop2.first().first, 1) != QStringLiteral("N")) {
        qWarning() << "SnippetEngine self-test: ${2} mismatch";
        return false;
    }

    // Tab stop 0 (final cursor) should be zero-width and live between the
    // four spaces of indentation and the closing newline.
    const auto stop0 = r.placeholderRanges.value(0);
    if (stop0.size() != 1 || stop0.first().first != stop0.first().second) {
        qWarning() << "SnippetEngine self-test: ${0} not zero-width";
        return false;
    }

    return true;
}

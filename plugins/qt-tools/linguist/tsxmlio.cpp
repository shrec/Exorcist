// tsxmlio.cpp — implementation of Qt Linguist .ts file I/O.
//
// Hand-rolled XML parser/writer (QXmlStreamReader / QXmlStreamWriter) that
// covers the subset of Linguist .ts the editor exposes: contexts, messages,
// source / translation / comment, locations and the translation `type`
// attribute.  Anything we don't understand is round-tripped by writing the
// fields we do know (we deliberately don't preserve unknown elements yet —
// future work; users editing .ts mostly want translation churn).
#include "tsxmlio.h"

#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace exo::forms::linguist {

// ── TsFile counters ─────────────────────────────────────────────────────────
int TsFile::totalCount() const
{
    int n = 0;
    for (const auto &c : contexts) {
        for (const auto &m : c.messages) {
            if (m.status != QStringLiteral("obsolete")
                && m.status != QStringLiteral("vanished"))
                ++n;
        }
    }
    return n;
}

int TsFile::finishedCount() const
{
    int n = 0;
    for (const auto &c : contexts) {
        for (const auto &m : c.messages) {
            if (m.status.isEmpty()) // empty status = finished/accepted
                ++n;
        }
    }
    return n;
}

int TsFile::unfinishedCount() const
{
    int n = 0;
    for (const auto &c : contexts) {
        for (const auto &m : c.messages) {
            if (m.status == QStringLiteral("unfinished"))
                ++n;
        }
    }
    return n;
}

int TsFile::obsoleteCount() const
{
    int n = 0;
    for (const auto &c : contexts) {
        for (const auto &m : c.messages) {
            if (m.status == QStringLiteral("obsolete")
                || m.status == QStringLiteral("vanished"))
                ++n;
        }
    }
    return n;
}

// ── Helpers ─────────────────────────────────────────────────────────────────
namespace {

// Drain text (including CDATA / entity refs) from the current element until
// its end tag.  Caller must be positioned on a StartElement.
QString readElementText(QXmlStreamReader &r)
{
    return r.readElementText(QXmlStreamReader::IncludeChildElements);
}

// Parse one <message> element.  Caller must be positioned on the StartElement.
TsMessage parseMessage(QXmlStreamReader &r)
{
    TsMessage m;
    while (!(r.isEndElement() && r.name() == QStringLiteral("message"))) {
        r.readNext();
        if (r.hasError()) break;
        if (r.isStartElement()) {
            const QStringView tag = r.name();
            if (tag == QStringLiteral("location")) {
                const QString file = r.attributes().value(QStringLiteral("filename")).toString();
                const int line     = r.attributes().value(QStringLiteral("line")).toInt();
                m.locations.append(qMakePair(file, line));
                // self-closing — drain to EndElement
                r.skipCurrentElement();
            } else if (tag == QStringLiteral("source")) {
                m.source = readElementText(r);
            } else if (tag == QStringLiteral("comment")
                       || tag == QStringLiteral("extracomment")) {
                // Treat comment/extracomment as a single concatenated hint.
                const QString c = readElementText(r);
                if (!m.comment.isEmpty()) m.comment += QLatin1Char('\n');
                m.comment += c;
            } else if (tag == QStringLiteral("translation")) {
                m.status      = r.attributes().value(QStringLiteral("type")).toString();
                m.translation = readElementText(r);
            } else {
                // Unknown — skip but keep parsing.
                r.skipCurrentElement();
            }
        }
        if (r.atEnd()) break;
    }
    return m;
}

TsContext parseContext(QXmlStreamReader &r)
{
    TsContext c;
    while (!(r.isEndElement() && r.name() == QStringLiteral("context"))) {
        r.readNext();
        if (r.hasError()) break;
        if (r.isStartElement()) {
            const QStringView tag = r.name();
            if (tag == QStringLiteral("name")) {
                c.name = readElementText(r);
            } else if (tag == QStringLiteral("message")) {
                c.messages.append(parseMessage(r));
            } else {
                r.skipCurrentElement();
            }
        }
        if (r.atEnd()) break;
    }
    return c;
}

} // namespace

// ── load ────────────────────────────────────────────────────────────────────
bool TsXmlIO::load(const QString &path, TsFile &out, QString *errorOut)
{
    out = TsFile{};

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QStringLiteral("Cannot open '%1': %2").arg(path, f.errorString());
        return false;
    }

    QXmlStreamReader r(&f);
    bool sawRoot = false;

    while (!r.atEnd() && !r.hasError()) {
        r.readNext();
        if (!r.isStartElement()) continue;

        if (r.name() == QStringLiteral("TS")) {
            sawRoot = true;
            const auto a = r.attributes();
            out.version        = a.value(QStringLiteral("version")).toString();
            if (out.version.isEmpty()) out.version = QStringLiteral("2.1");
            out.language       = a.value(QStringLiteral("language")).toString();
            out.sourceLanguage = a.value(QStringLiteral("sourcelanguage")).toString();
        } else if (r.name() == QStringLiteral("context") && sawRoot) {
            out.contexts.append(parseContext(r));
        }
    }

    if (r.hasError()) {
        if (errorOut)
            *errorOut = QStringLiteral("XML parse error: %1 (line %2 col %3)")
                            .arg(r.errorString())
                            .arg(r.lineNumber())
                            .arg(r.columnNumber());
        return false;
    }
    if (!sawRoot) {
        if (errorOut) *errorOut = QStringLiteral("Missing <TS> root element");
        return false;
    }
    return true;
}

// ── save ────────────────────────────────────────────────────────────────────
bool TsXmlIO::save(const QString &path, const TsFile &file, QString *errorOut)
{
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QStringLiteral("Cannot open '%1' for write: %2").arg(path, out.errorString());
        return false;
    }

    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.setAutoFormattingIndent(4);

    w.writeStartDocument(QStringLiteral("1.0"));
    w.writeDTD(QStringLiteral("<!DOCTYPE TS>"));

    w.writeStartElement(QStringLiteral("TS"));
    w.writeAttribute(QStringLiteral("version"),
                     file.version.isEmpty() ? QStringLiteral("2.1") : file.version);
    if (!file.language.isEmpty())
        w.writeAttribute(QStringLiteral("language"), file.language);
    if (!file.sourceLanguage.isEmpty())
        w.writeAttribute(QStringLiteral("sourcelanguage"), file.sourceLanguage);

    for (const auto &ctx : file.contexts) {
        w.writeStartElement(QStringLiteral("context"));
        w.writeTextElement(QStringLiteral("name"), ctx.name);

        for (const auto &m : ctx.messages) {
            w.writeStartElement(QStringLiteral("message"));

            for (const auto &loc : m.locations) {
                w.writeStartElement(QStringLiteral("location"));
                w.writeAttribute(QStringLiteral("filename"), loc.first);
                w.writeAttribute(QStringLiteral("line"), QString::number(loc.second));
                w.writeEndElement();
            }
            w.writeTextElement(QStringLiteral("source"), m.source);
            if (!m.comment.isEmpty())
                w.writeTextElement(QStringLiteral("comment"), m.comment);

            w.writeStartElement(QStringLiteral("translation"));
            if (!m.status.isEmpty())
                w.writeAttribute(QStringLiteral("type"), m.status);
            w.writeCharacters(m.translation);
            w.writeEndElement(); // translation

            w.writeEndElement(); // message
        }
        w.writeEndElement(); // context
    }

    w.writeEndElement();   // TS
    w.writeEndDocument();

    if (!out.commit()) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to commit '%1': %2").arg(path, out.errorString());
        return false;
    }
    return true;
}

} // namespace exo::forms::linguist

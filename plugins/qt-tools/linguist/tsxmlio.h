// tsxmlio.h — Qt Linguist (.ts) translation file I/O.
//
// Pure data + read/write helpers built on QXmlStreamReader / QXmlStreamWriter.
// Mirrors the design of forms/formeditor/uixmlio.h: a tiny static facade plus
// well-typed plain structs the editor binds to.
//
// The .ts schema (DOCTYPE TS, version 2.1) we support:
//
//   <TS version="2.1" language="en_US" sourcelanguage="en">
//     <context>
//       <name>MainWindow</name>
//       <message>
//         <location filename="../mainwindow.cpp" line="42"/>
//         <source>Hello</source>
//         <comment>greeting</comment>          // optional disambiguation
//         <translation type="unfinished">Hola</translation>
//       </message>
//     </context>
//   </TS>
//
// Status convention used here:
//   ""           → translated and accepted (no `type` attribute on <translation>)
//   "unfinished" → translator still has to revisit
//   "obsolete"   → source no longer exists, kept for re-use
//   "vanished"   → similar to obsolete (Qt 5.10+) — surfaced as obsolete in UI
#pragma once

#include <QList>
#include <QPair>
#include <QString>

namespace exo::forms::linguist {

struct TsMessage {
    QString source;
    QString translation;
    QString comment;                                   // disambiguation hint
    QString status;                                    // "" | "unfinished" | "obsolete" | "vanished"
    QList<QPair<QString, int>> locations;              // <file, line> pairs
};

struct TsContext {
    QString name;
    QList<TsMessage> messages;
};

struct TsFile {
    QString version{QStringLiteral("2.1")};
    QString language;
    QString sourceLanguage;
    QList<TsContext> contexts;

    // Convenience: total / finished / unfinished counts (obsolete excluded).
    int totalCount() const;
    int finishedCount() const;
    int unfinishedCount() const;
    int obsoleteCount() const;
};

class TsXmlIO {
public:
    /// Parse a .ts file at @p path.  Returns true on success and populates
    /// @p out.  On failure @p errorOut receives a human-readable message
    /// (file open error, XML parse error, missing root element, ...).
    static bool load(const QString &path, TsFile &out, QString *errorOut = nullptr);

    /// Serialize @p file to @p path in Qt Linguist .ts XML format.  Writes a
    /// proper XML prolog and DOCTYPE.  Returns true on success; on failure
    /// @p errorOut receives a human-readable message.
    static bool save(const QString &path, const TsFile &file, QString *errorOut = nullptr);
};

} // namespace exo::forms::linguist

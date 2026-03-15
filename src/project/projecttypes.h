#pragma once

#include <QString>
#include <QList>
#include <QStringList>

/// Project file extensions by language.
/// Each language has its own .<ext> file inside the project directory.
///
/// Pattern:  .ex<lang>prj
///   .excpprj   — C++
///   .excprj    — C
///   .exrsprj   — Rust
///   .expyprj   — Python
///   .exgoprj   — Go
///   .exjsprj   — JavaScript
///   .extsprj   — TypeScript
///   .exjvprj   — Java
///   .excsprj   — C#
///   .exswprj   — Swift
///   .exktprj   — Kotlin
///   .exzgprj   — Zig
///   .exluprj   — Lua
///   .exrbprj   — Ruby
///   .exphprj   — PHP
///   .exdtprj   — Dart
///   .exhsprj   — Haskell
///   .exscprj   — Scala
///   .exwbprj   — Web (HTML/CSS/JS)
///   .exprj     — Generic / unknown
///
/// The solution file .exsln references projects via their .ex*prj file path.

namespace ExProjectExt {

inline QString forLanguage(const QString &lang)
{
    if (lang == QLatin1String("C++"))        return QStringLiteral(".excpprj");
    if (lang == QLatin1String("C"))          return QStringLiteral(".excprj");
    if (lang == QLatin1String("Rust"))       return QStringLiteral(".exrsprj");
    if (lang == QLatin1String("Python"))     return QStringLiteral(".expyprj");
    if (lang == QLatin1String("Go"))         return QStringLiteral(".exgoprj");
    if (lang == QLatin1String("JavaScript")) return QStringLiteral(".exjsprj");
    if (lang == QLatin1String("TypeScript")) return QStringLiteral(".extsprj");
    if (lang == QLatin1String("Java"))       return QStringLiteral(".exjvprj");
    if (lang == QLatin1String("C#"))         return QStringLiteral(".excsprj");
    if (lang == QLatin1String("Swift"))      return QStringLiteral(".exswprj");
    if (lang == QLatin1String("Kotlin"))     return QStringLiteral(".exktprj");
    if (lang == QLatin1String("Zig"))        return QStringLiteral(".exzgprj");
    if (lang == QLatin1String("Lua"))        return QStringLiteral(".exluprj");
    if (lang == QLatin1String("Ruby"))       return QStringLiteral(".exrbprj");
    if (lang == QLatin1String("PHP"))        return QStringLiteral(".exphprj");
    if (lang == QLatin1String("Dart"))       return QStringLiteral(".exdtprj");
    if (lang == QLatin1String("Haskell"))    return QStringLiteral(".exhsprj");
    if (lang == QLatin1String("Scala"))      return QStringLiteral(".exscprj");
    if (lang == QLatin1String("Web"))        return QStringLiteral(".exwbprj");
    return QStringLiteral(".exprj");
}

inline QStringList allProjectExtensions()
{
    return {
        QStringLiteral("*.excpprj"), QStringLiteral("*.excprj"),
        QStringLiteral("*.exrsprj"), QStringLiteral("*.expyprj"),
        QStringLiteral("*.exgoprj"), QStringLiteral("*.exjsprj"),
        QStringLiteral("*.extsprj"), QStringLiteral("*.exjvprj"),
        QStringLiteral("*.excsprj"), QStringLiteral("*.exswprj"),
        QStringLiteral("*.exktprj"), QStringLiteral("*.exzgprj"),
        QStringLiteral("*.exluprj"), QStringLiteral("*.exrbprj"),
        QStringLiteral("*.exphprj"), QStringLiteral("*.exdtprj"),
        QStringLiteral("*.exhsprj"), QStringLiteral("*.exscprj"),
        QStringLiteral("*.exwbprj"), QStringLiteral("*.exprj"),
    };
}

inline QString languageFromExtension(const QString &ext)
{
    if (ext == QLatin1String(".excpprj"))  return QStringLiteral("C++");
    if (ext == QLatin1String(".excprj"))   return QStringLiteral("C");
    if (ext == QLatin1String(".exrsprj"))  return QStringLiteral("Rust");
    if (ext == QLatin1String(".expyprj"))  return QStringLiteral("Python");
    if (ext == QLatin1String(".exgoprj"))  return QStringLiteral("Go");
    if (ext == QLatin1String(".exjsprj"))  return QStringLiteral("JavaScript");
    if (ext == QLatin1String(".extsprj"))  return QStringLiteral("TypeScript");
    if (ext == QLatin1String(".exjvprj"))  return QStringLiteral("Java");
    if (ext == QLatin1String(".excsprj"))  return QStringLiteral("C#");
    if (ext == QLatin1String(".exswprj"))  return QStringLiteral("Swift");
    if (ext == QLatin1String(".exktprj"))  return QStringLiteral("Kotlin");
    if (ext == QLatin1String(".exzgprj"))  return QStringLiteral("Zig");
    if (ext == QLatin1String(".exluprj"))  return QStringLiteral("Lua");
    if (ext == QLatin1String(".exrbprj"))  return QStringLiteral("Ruby");
    if (ext == QLatin1String(".exphprj"))  return QStringLiteral("PHP");
    if (ext == QLatin1String(".exdtprj"))  return QStringLiteral("Dart");
    if (ext == QLatin1String(".exhsprj"))  return QStringLiteral("Haskell");
    if (ext == QLatin1String(".exscprj"))  return QStringLiteral("Scala");
    if (ext == QLatin1String(".exwbprj"))  return QStringLiteral("Web");
    return {};
}

} // namespace ExProjectExt

/// Represents a single project within a solution.
struct ExProject
{
    QString name;           ///< Display name
    QString rootPath;       ///< Absolute path to project directory
    QString projectFile;    ///< Path to .ex*prj file (relative to rootPath)
    QString language;       ///< Language identifier (C++, Rust, Python, etc.)
    QString templateId;     ///< Template ID that created this project
};

/// Solution file format (.exsln).
/// A solution groups multiple projects and references them by project file.
///
/// JSON format:
/// {
///     "name": "MySolution",
///     "version": 1,
///     "projects": [
///         {
///             "name": "App",
///             "path": "App",
///             "projectFile": "App.excpprj",
///             "language": "C++",
///             "templateId": "cpp-console"
///         }
///     ]
/// }
struct ExSolution
{
    QString name;
    QString filePath;
    QList<ExProject> projects;
};

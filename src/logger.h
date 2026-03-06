#pragma once

#include <QMessageLogContext>
#include <QString>

// Exorcist Logger
//
// Installs a Qt message handler that writes all qDebug / qInfo / qWarning /
// qCritical / qFatal output to:
//   1. A rotating log file next to the executable  (exorcist.log)
//   2. stderr (when a terminal is attached)
//
// Usage:
//   Logger::install();          // call once before QApplication::exec()
//   qDebug()   << "...";       // DEBUG
//   qInfo()    << "...";       // INFO
//   qWarning() << "...";       // WARN
//   qCritical()<< "...";       // ERROR
//
// The log file is truncated at startup (one log per session).

class Logger
{
public:
    static void install(const QString &logFilePath = {});
    static void uninstall();

    // Convenience: return the path of the current log file.
    static QString logFilePath();
};

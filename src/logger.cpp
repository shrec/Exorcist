#include "logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

namespace {

QFile   g_logFile;
QMutex  g_mutex;
QString g_logPath;

const char *levelStr(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return "DEBUG";
    case QtInfoMsg:     return "INFO ";
    case QtWarningMsg:  return "WARN ";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg:    return "FATAL";
    default:            return "?    ";
    }
}

void messageHandler(QtMsgType type,
                    const QMessageLogContext &ctx,
                    const QString &msg)
{
    const QString timestamp = QDateTime::currentDateTime()
        .toString("yyyy-MM-dd HH:mm:ss.zzz");

    // Build "file:line" from context when available.
    QString location;
    if (ctx.file) {
        // Strip full path down to filename only.
        const QString file = QString::fromUtf8(ctx.file);
        const int slash = qMax(file.lastIndexOf('/'), file.lastIndexOf('\\'));
        location = QString(" [%1:%2]").arg(file.mid(slash + 1)).arg(ctx.line);
    }

    const QString line = QString("%1 %2%3  %4\n")
        .arg(timestamp)
        .arg(QLatin1String(levelStr(type)))
        .arg(location)
        .arg(msg);

    QMutexLocker lock(&g_mutex);

    if (g_logFile.isOpen()) {
        g_logFile.write(line.toUtf8());
        g_logFile.flush();
    }

    // Also write to stderr so the developer sees output when running from a terminal.
    fprintf(stderr, "%s", qPrintable(line));

    if (type == QtFatalMsg) {
        g_logFile.close();
        abort();
    }
}

} // namespace

void Logger::install(const QString &logFilePath)
{
    QMutexLocker lock(&g_mutex);

    if (logFilePath.isEmpty()) {
        const QString dir = QCoreApplication::applicationDirPath();
        g_logPath = dir + "/exorcist.log";
    } else {
        g_logPath = logFilePath;
    }

    g_logFile.setFileName(g_logPath);
    // Truncate: one log file per session.
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);

    // Write a session header so log files are easy to distinguish.
    if (g_logFile.isOpen()) {
        const QString header = QString("=== Exorcist session started %1 ===\n")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
        g_logFile.write(header.toUtf8());
        g_logFile.flush();
    }

    qInstallMessageHandler(messageHandler);
}

void Logger::uninstall()
{
    qInstallMessageHandler(nullptr);
    QMutexLocker lock(&g_mutex);
    g_logFile.close();
}

QString Logger::logFilePath()
{
    return g_logPath;
}

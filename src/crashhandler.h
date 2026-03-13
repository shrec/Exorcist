#pragma once

#include <QString>
#include <QStringList>

/// Global crash handler that captures unhandled exceptions and fatal signals.
///
/// On Windows: installs a Structured Exception Handler that writes a minidump
/// (.dmp) and a detailed crash report (.txt) including register dump, faulting
/// module, loaded modules, last log lines, and symbolic stack trace.
///
/// On Linux/macOS: installs signal handlers for SIGSEGV, SIGABRT, SIGFPE,
/// SIGBUS, SIGILL that write a crash report with a symbolic stack trace.
///
/// Usage (call once at startup, before QApplication::exec):
///   CrashHandler::install();
///   CrashHandler::install("/custom/crash/dir");
///
/// Crash files are written to:
///   <crashDir>/crash_<timestamp>.txt   — human-readable report
///   <crashDir>/crash_<timestamp>.dmp   — Windows minidump (Windows only)
///
/// Default crash directory: <appDir>/crashes/
class CrashHandler
{
public:
    /// Install the global crash handler.
    /// @param crashDir  Directory to write crash files. If empty, defaults to
    ///                  <applicationDirPath>/crashes/.
    static void install(const QString &crashDir = {});

    /// Returns the directory where crash files are written.
    static QString crashDirectory();

    /// Record a log line into the crash ring buffer.
    /// Called automatically by Logger if CrashHandler is installed first.
    /// Thread-safe.
    static void recordLogLine(const QString &line);

    /// Returns the last N log lines (up to ring buffer capacity).
    static QStringList recentLogLines();

    /// Ring buffer capacity (number of log lines kept before crash).
    static constexpr int kRingBufferSize = 50;

private:
    CrashHandler() = default;
};

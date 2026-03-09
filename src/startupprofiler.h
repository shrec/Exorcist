#pragma once

#include <QElapsedTimer>
#include <QString>
#include <QVector>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#ifdef Q_OS_LINUX
#include <QFile>
#include <unistd.h>
#endif
#ifdef Q_OS_MAC
#include <sys/resource.h>
#endif

/// Lightweight startup profiler that logs phase durations and memory usage.
/// Usage:
///   StartupProfiler::instance().begin();        // at app start
///   StartupProfiler::instance().mark("phase");   // after each phase
///   StartupProfiler::instance().finish();         // log summary
class StartupProfiler
{
public:
    static StartupProfiler &instance()
    {
        static StartupProfiler s;
        return s;
    }

    void begin()
    {
        m_timer.start();
        m_marks.clear();
        m_marks.append({QStringLiteral("start"), 0});
    }

    void mark(const QString &label)
    {
        m_marks.append({label, m_timer.elapsed()});
    }

    void finish()
    {
        const qint64 total = m_timer.elapsed();
        QString report = QStringLiteral("=== Startup Profile ===\n");
        for (int i = 1; i < m_marks.size(); ++i) {
            const qint64 delta = m_marks[i].ms - m_marks[i - 1].ms;
            report += QStringLiteral("  %1: %2 ms\n")
                          .arg(m_marks[i].label, -30)
                          .arg(delta);
        }
        report += QStringLiteral("  Total: %1 ms\n").arg(total);
        report += QStringLiteral("  RSS: %1 MB\n").arg(currentRssMB(), 0, 'f', 1);
        qInfo().noquote() << report;
    }

    static double currentRssMB()
    {
#ifdef Q_OS_WIN
        // Windows: use GetProcessMemoryInfo
        return windowsRssMB();
#elif defined(Q_OS_LINUX)
        return linuxRssMB();
#elif defined(Q_OS_MAC)
        return macRssMB();
#else
        return 0.0;
#endif
    }

private:
    StartupProfiler() = default;

    struct Mark { QString label; qint64 ms; };
    QElapsedTimer m_timer;
    QVector<Mark> m_marks;

#ifdef Q_OS_WIN
    static double windowsRssMB()
    {
        // PROCESS_MEMORY_COUNTERS is in psapi.h / windows.h
        // Use GetProcessMemoryInfo from Kernel32 (Win7+) or Psapi
        using HANDLE = void *;
        struct PMC {
            unsigned long cb;
            unsigned long PageFaultCount;
            size_t PeakWorkingSetSize;
            size_t WorkingSetSize;
            size_t QuotaPeakPagedPoolUsage;
            size_t QuotaPagedPoolUsage;
            size_t QuotaPeakNonPagedPoolUsage;
            size_t QuotaNonPagedPoolUsage;
            size_t PagefileUsage;
            size_t PeakPagefileUsage;
        };
        // K32GetProcessMemoryInfo is available in kernel32.dll on Win7+
        auto mod = GetModuleHandleA("kernel32.dll");
        if (!mod) return 0.0;
        using Fn = int(__stdcall *)(HANDLE, PMC *, unsigned long);
        auto fn = reinterpret_cast<Fn>(GetProcAddress(mod, "K32GetProcessMemoryInfo"));
        if (!fn) return 0.0;
        PMC pmc{};
        pmc.cb = sizeof(pmc);
        if (fn(GetCurrentProcess(), &pmc, sizeof(pmc)))
            return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        return 0.0;
    }
#endif

#ifdef Q_OS_LINUX
    static double linuxRssMB()
    {
        // Read /proc/self/statm — field 1 is RSS in pages
        QFile f(QStringLiteral("/proc/self/statm"));
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray data = f.readAll();
            const QList<QByteArray> parts = data.split(' ');
            if (parts.size() >= 2) {
                bool ok = false;
                const qint64 pages = parts[1].toLongLong(&ok);
                if (ok) {
                    const long pageSize = sysconf(_SC_PAGESIZE);
                    return static_cast<double>(pages * pageSize) / (1024.0 * 1024.0);
                }
            }
        }
        return 0.0;
    }
#endif

#ifdef Q_OS_MAC
    static double macRssMB()
    {
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0)
            return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
        return 0.0;
    }
#endif
};

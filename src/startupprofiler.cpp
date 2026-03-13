#include "startupprofiler.h"

#include <QCoreApplication>
#include <QDebug>

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

StartupProfiler &StartupProfiler::instance()
{
    static StartupProfiler s;
    return s;
}

void StartupProfiler::begin()
{
    m_timer.start();
    m_marks.clear();
    m_marks.append({QStringLiteral("start"), 0});
}

void StartupProfiler::mark(const QString &label)
{
    m_marks.append({label, m_timer.elapsed()});
}

void StartupProfiler::finish()
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

    const double rss = currentRssMB();
    report += QStringLiteral("  RSS: %1 MB\n").arg(rss, 0, 'f', 1);

    // Budget checks
    if (total > kStartupBudgetMs)
        report += QStringLiteral("  ⚠ OVER BUDGET: startup %1 ms > %2 ms limit\n")
                      .arg(total).arg(kStartupBudgetMs);
    if (rss > kRssBudgetMB)
        report += QStringLiteral("  ⚠ OVER BUDGET: RSS %1 MB > %2 MB limit\n")
                      .arg(rss, 0, 'f', 1).arg(kRssBudgetMB, 0, 'f', 0);

    qInfo().noquote() << report;
}

double StartupProfiler::currentRssMB()
{
#ifdef Q_OS_WIN
    return windowsRssMB();
#elif defined(Q_OS_LINUX)
    return linuxRssMB();
#elif defined(Q_OS_MAC)
    return macRssMB();
#else
    return 0.0;
#endif
}

#ifdef Q_OS_WIN
double StartupProfiler::windowsRssMB()
{
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
double StartupProfiler::linuxRssMB()
{
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
double StartupProfiler::macRssMB()
{
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0)
        return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
    return 0.0;
}
#endif

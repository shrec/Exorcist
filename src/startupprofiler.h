#pragma once

#include <QElapsedTimer>
#include <QString>
#include <QVector>

/// Lightweight startup profiler that logs phase durations and memory usage.
/// Reports warnings when startup time or RSS exceed defined budgets.
///
/// Usage:
///   StartupProfiler::instance().begin();        // at app start
///   StartupProfiler::instance().mark("phase");   // after each phase
///   StartupProfiler::instance().finish();         // log summary + budget check
class StartupProfiler
{
public:
    // Performance budgets (from docs/manifesto.md)
    static constexpr qint64 kStartupBudgetMs = 300;   // cold start < 300 ms
    static constexpr double kRssBudgetMB     = 80.0;   // idle RAM < 80 MB

    static StartupProfiler &instance();

    void begin();
    void mark(const QString &label);
    void finish();

    static double currentRssMB();

private:
    StartupProfiler() = default;

    struct Mark { QString label; qint64 ms; };
    QElapsedTimer m_timer;
    QVector<Mark> m_marks;

    static double windowsRssMB();
    static double linuxRssMB();
    static double macRssMB();
};

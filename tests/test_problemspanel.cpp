#include <QtTest>
#include <QTreeWidget>

#include "problems/problemspanel.h"
#include "component/outputpanel.h"
#include "sdk/idiagnosticsservice.h"

// ── Mock IDiagnosticsService ──────────────────────────────────────────────────

class MockDiagnosticsService : public IDiagnosticsService
{
public:
    QList<SDKDiagnostic> diagnostics() const override { return m_diags; }

    QList<SDKDiagnostic> diagnosticsForFile(const QString &filePath) const override
    {
        QList<SDKDiagnostic> result;
        for (const auto &d : m_diags) {
            if (d.filePath == filePath)
                result.append(d);
        }
        return result;
    }

    int errorCount() const override
    {
        int n = 0;
        for (const auto &d : m_diags)
            if (d.severity == SDKDiagnostic::Error) ++n;
        return n;
    }

    int warningCount() const override
    {
        int n = 0;
        for (const auto &d : m_diags)
            if (d.severity == SDKDiagnostic::Warning) ++n;
        return n;
    }

    void addDiagnostic(const QString &file, int line, SDKDiagnostic::Severity sev,
                       const QString &msg, const QString &source = QStringLiteral("test"))
    {
        SDKDiagnostic d;
        d.filePath = file;
        d.line     = line;
        d.severity = sev;
        d.message  = msg;
        d.source   = source;
        m_diags.append(d);
    }

    QList<SDKDiagnostic> m_diags;
};

// ── Test class ────────────────────────────────────────────────────────────────

class TestProblemsPanel : public QObject
{
    Q_OBJECT

private slots:
    // ── Construction ──────────────────────────────────────────────────────

    void defaultState()
    {
        ProblemsPanel panel;
        QCOMPARE(panel.problemCount(), 0);
    }

    // ── With LSP diagnostics ──────────────────────────────────────────────

    void lspDiagnostics_basic()
    {
        MockDiagnosticsService mock;
        mock.addDiagnostic(QStringLiteral("/src/main.cpp"), 10,
                          SDKDiagnostic::Error, QStringLiteral("undeclared identifier"));
        mock.addDiagnostic(QStringLiteral("/src/main.cpp"), 20,
                          SDKDiagnostic::Warning, QStringLiteral("unused variable"));

        ProblemsPanel panel;
        panel.setDiagnosticsService(&mock);
        panel.refresh();

        QCOMPARE(panel.problemCount(), 2);
    }

    void lspDiagnostics_empty()
    {
        MockDiagnosticsService mock;
        ProblemsPanel panel;
        panel.setDiagnosticsService(&mock);
        panel.refresh();
        QCOMPARE(panel.problemCount(), 0);
    }

    void lspDiagnostics_allSeverities()
    {
        MockDiagnosticsService mock;
        mock.addDiagnostic(QStringLiteral("a.cpp"), 1, SDKDiagnostic::Error, QStringLiteral("err"));
        mock.addDiagnostic(QStringLiteral("b.cpp"), 2, SDKDiagnostic::Warning, QStringLiteral("warn"));
        mock.addDiagnostic(QStringLiteral("c.cpp"), 3, SDKDiagnostic::Info, QStringLiteral("info"));
        mock.addDiagnostic(QStringLiteral("d.cpp"), 4, SDKDiagnostic::Hint, QStringLiteral("hint"));

        ProblemsPanel panel;
        panel.setDiagnosticsService(&mock);
        panel.refresh();

        QCOMPARE(panel.problemCount(), 4);
    }

    // ── Navigation signal ─────────────────────────────────────────────────

    void navigateSignal()
    {
        ProblemsPanel panel;
        QSignalSpy spy(&panel, &ProblemsPanel::navigateToFile);
        QVERIFY(spy.isValid());
    }

    // ── Null service ──────────────────────────────────────────────────────

    void nullService_noCrash()
    {
        ProblemsPanel panel;
        panel.setDiagnosticsService(nullptr);
        panel.setOutputPanel(nullptr);
        panel.refresh(); // should not crash
        QCOMPARE(panel.problemCount(), 0);
    }

    // ── Multiple refreshes ────────────────────────────────────────────────

    void multipleRefreshes()
    {
        MockDiagnosticsService mock;
        mock.addDiagnostic(QStringLiteral("a.cpp"), 1, SDKDiagnostic::Error, QStringLiteral("err"));

        ProblemsPanel panel;
        panel.setDiagnosticsService(&mock);
        panel.refresh();
        QCOMPARE(panel.problemCount(), 1);

        // Add more diagnostics and refresh
        mock.addDiagnostic(QStringLiteral("b.cpp"), 2, SDKDiagnostic::Warning, QStringLiteral("warn"));
        panel.refresh();
        QCOMPARE(panel.problemCount(), 2);
    }

    // ── Diagnostics with source ───────────────────────────────────────────

    void diagnosticsWithSource()
    {
        MockDiagnosticsService mock;
        mock.addDiagnostic(QStringLiteral("main.cpp"), 5,
                          SDKDiagnostic::Error, QStringLiteral("error msg"),
                          QStringLiteral("clangd"));

        ProblemsPanel panel;
        panel.setDiagnosticsService(&mock);
        panel.refresh();
        QCOMPARE(panel.problemCount(), 1);
    }

    // ── Replace diagnostics service ───────────────────────────────────────

    void replaceDiagnosticsService()
    {
        MockDiagnosticsService mock1;
        mock1.addDiagnostic(QStringLiteral("a.cpp"), 1, SDKDiagnostic::Error, QStringLiteral("e1"));

        MockDiagnosticsService mock2;
        mock2.addDiagnostic(QStringLiteral("b.cpp"), 2, SDKDiagnostic::Warning, QStringLiteral("w1"));
        mock2.addDiagnostic(QStringLiteral("c.cpp"), 3, SDKDiagnostic::Error, QStringLiteral("e2"));

        ProblemsPanel panel;
        panel.setDiagnosticsService(&mock1);
        panel.refresh();
        QCOMPARE(panel.problemCount(), 1);

        panel.setDiagnosticsService(&mock2);
        panel.refresh();
        QCOMPARE(panel.problemCount(), 2);
    }
};

QTEST_MAIN(TestProblemsPanel)
#include "test_problemspanel.moc"

#pragma once

#include "../itool.h"

#include <QJsonArray>
#include <functional>

// ── BuildProjectTool ─────────────────────────────────────────────────────────
// Triggers a project build (CMake). The AI agent can verify its changes compile.

class BuildProjectTool : public ITool
{
public:
    // builder(target) → {success, output, exitCode}
    struct BuildResult {
        bool    success;
        QString output;    // combined stdout+stderr
        int     exitCode;
    };
    using Builder = std::function<BuildResult(const QString &target)>;

    explicit BuildProjectTool(Builder builder)
        : m_builder(std::move(builder)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("build_project");
        s.description = QStringLiteral(
            "Build the project using CMake. Runs 'cmake --build' on the "
            "active build configuration. Use this after making code changes "
            "to verify they compile. Optionally specify a target name.");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 120000;  // builds can take up to 2 min
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("target"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Optional build target. If empty, builds the default target.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString target = args[QLatin1String("target")].toString();
        const BuildResult result = m_builder(target);

        // Truncate very long output for model context
        constexpr int kMaxOutput = 30000;
        QString output = result.output;
        bool truncated = false;
        if (output.size() > kMaxOutput) {
            // Keep the last part (errors are usually at the end)
            output = QStringLiteral("... (truncated %1 chars) ...\n").arg(output.size() - kMaxOutput)
                   + output.right(kMaxOutput);
            truncated = true;
        }

        QString summary;
        if (result.success) {
            summary = QStringLiteral("Build succeeded (exit code %1)").arg(result.exitCode);
        } else {
            summary = QStringLiteral("Build FAILED (exit code %1)").arg(result.exitCode);
        }
        if (!target.isEmpty())
            summary += QStringLiteral(" [target: %1]").arg(target);
        summary += QStringLiteral("\n\n") + output;

        QJsonObject data;
        data[QLatin1String("success")]   = result.success;
        data[QLatin1String("exitCode")]  = result.exitCode;
        data[QLatin1String("truncated")] = truncated;

        return {result.success, data, summary, result.success ? QString{} : QStringLiteral("Build failed.")};
    }

private:
    Builder m_builder;
};

// ── RunTestsTool ─────────────────────────────────────────────────────────────
// Run project tests. Supports scoping: all, target, or specific test.

class RunTestsTool : public ITool
{
public:
    struct TestResult {
        bool    success;
        int     passed;
        int     failed;
        int     total;
        QString output;
    };
    using TestRunner = std::function<TestResult(const QString &scope, const QString &filter)>;

    explicit RunTestsTool(TestRunner runner)
        : m_runner(std::move(runner)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("run_tests");
        s.description = QStringLiteral(
            "Run project tests. Use this to verify changes don't break "
            "existing functionality. Can run all tests, a specific target, "
            "or filter by test name pattern.");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 120000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("scope"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Test scope: 'all' (default), 'failed' (re-run failed), "
                                    "or a target/executable name.")},
                }},
                {QStringLiteral("filter"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Optional test name filter (regex or glob). "
                                    "Only runs tests matching this pattern.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString scope  = args[QLatin1String("scope")].toString(QStringLiteral("all"));
        const QString filter = args[QLatin1String("filter")].toString();

        const TestResult result = m_runner(scope, filter);

        constexpr int kMaxOutput = 20000;
        QString output = result.output;
        if (output.size() > kMaxOutput)
            output = QStringLiteral("... (truncated) ...\n") + output.right(kMaxOutput);

        QString summary;
        if (result.success) {
            summary = QStringLiteral("Tests PASSED: %1/%2")
                          .arg(result.passed).arg(result.total);
        } else {
            summary = QStringLiteral("Tests FAILED: %1 passed, %2 failed out of %3")
                          .arg(result.passed).arg(result.failed).arg(result.total);
        }
        summary += QStringLiteral("\n\n") + output;

        QJsonObject data;
        data[QLatin1String("success")] = result.success;
        data[QLatin1String("passed")]  = result.passed;
        data[QLatin1String("failed")]  = result.failed;
        data[QLatin1String("total")]   = result.total;

        return {result.success, data, summary,
                result.success ? QString{} : QStringLiteral("Some tests failed.")};
    }

private:
    TestRunner m_runner;
};

// ── GetBuildTargetsTool ──────────────────────────────────────────────────────
// Lists available build targets (executables, libraries, test targets).

class GetBuildTargetsTool : public ITool
{
public:
    using TargetsGetter = std::function<QStringList()>;

    explicit GetBuildTargetsTool(TargetsGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("get_build_targets");
        s.description = QStringLiteral(
            "List available build targets (executables, libraries, tests). "
            "Use this to discover what can be built or run.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{}},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &) override
    {
        const QStringList targets = m_getter();
        if (targets.isEmpty())
            return {true, {}, QStringLiteral("No build targets found. "
                                             "Is CMake configured?"), {}};

        QString text = QStringLiteral("Build targets (%1):\n").arg(targets.size());
        for (const auto &t : targets)
            text += QStringLiteral("  - %1\n").arg(t);
        return {true, {}, text, {}};
    }

private:
    TargetsGetter m_getter;
};

// ── TestFailureTool ──────────────────────────────────────────────────────────
// Returns details of test failures from the most recent test run.
// The agent uses this to inspect failures without re-running tests.

class TestFailureTool : public ITool
{
public:
    using FailureGetter = std::function<RunTestsTool::TestResult()>;

    explicit TestFailureTool(FailureGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("test_failure");
        s.description = QStringLiteral(
            "Get details of test failures from the most recent test run. "
            "Returns failure count, output, and diagnostic information. "
            "Use this to examine test failures without re-running tests.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 5000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{}},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &) override
    {
        const RunTestsTool::TestResult result = m_getter();
        if (result.total == 0)
            return {true, {}, QStringLiteral("No test results available. Run tests first."), {}};

        if (result.success)
            return {true, {}, QStringLiteral("All %1 tests passed. No failures.").arg(result.total), {}};

        constexpr int kMaxOutput = 20000;
        QString output = result.output;
        if (output.size() > kMaxOutput)
            output = QStringLiteral("... (truncated) ...\n") + output.right(kMaxOutput);

        const QString summary = QStringLiteral(
            "Test failures: %1 failed, %2 passed out of %3 total.\n\n%4")
            .arg(result.failed).arg(result.passed).arg(result.total).arg(output);

        QJsonObject data;
        data[QLatin1String("failed")]  = result.failed;
        data[QLatin1String("passed")]  = result.passed;
        data[QLatin1String("total")]   = result.total;
        data[QLatin1String("success")] = result.success;

        return {false, data, summary, QStringLiteral("%1 test(s) failed.").arg(result.failed)};
    }

private:
    FailureGetter m_getter;
};

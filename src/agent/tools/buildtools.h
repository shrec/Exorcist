#pragma once

#include "../itool.h"

#include <atomic>
#include <functional>

// ── BuildProjectTool ─────────────────────────────────────────────────────────
// Triggers a project build (CMake). The AI agent can verify its changes compile.

class BuildProjectTool : public ITool
{
public:
    // builder(target, cancelled) → {success, output, exitCode}
    struct BuildResult {
        bool    success;
        QString output;    // combined stdout+stderr
        int     exitCode;
    };
    using Builder = std::function<BuildResult(const QString &target, std::atomic<bool> &cancelled)>;

    explicit BuildProjectTool(Builder builder)
        : m_builder(std::move(builder)) {}

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
    void cancel() override;

private:
    Builder m_builder;
    std::atomic<bool> m_cancelled{false};
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
    using TestRunner = std::function<TestResult(const QString &scope, const QString &filter,
                                                std::atomic<bool> &cancelled)>;

    explicit RunTestsTool(TestRunner runner)
        : m_runner(std::move(runner)) {}

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
    void cancel() override;

private:
    TestRunner m_runner;
    std::atomic<bool> m_cancelled{false};
};

// ── GetBuildTargetsTool ──────────────────────────────────────────────────────
// Lists available build targets (executables, libraries, test targets).

class GetBuildTargetsTool : public ITool
{
public:
    using TargetsGetter = std::function<QStringList()>;

    explicit GetBuildTargetsTool(TargetsGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &) override;

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

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &) override;

private:
    FailureGetter m_getter;
};

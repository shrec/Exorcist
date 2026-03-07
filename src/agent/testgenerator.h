#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>

// ─────────────────────────────────────────────────────────────────────────────
// TestGenerator — /tests slash command with framework auto-detection.
//
// Detects test framework from project files, generates test scaffolds,
// and builds AI context for comprehensive test generation.
// ─────────────────────────────────────────────────────────────────────────────

class TestGenerator : public QObject
{
    Q_OBJECT

public:
    explicit TestGenerator(QObject *parent = nullptr) : QObject(parent) {}

    enum Framework {
        Unknown,
        GoogleTest,
        Catch2,
        QtTest,
        Jest,
        Pytest,
        JUnit,
        NUnit,
        XUnit,
        RSpec,
        Mocha,
    };

    struct DetectionResult {
        Framework framework = Unknown;
        QString name;
        QString importStatement;
        QString testFilePattern;
        QString runCommand;
    };

    // Detect test framework from project files
    DetectionResult detectFramework(const QString &workspaceRoot) const
    {
        const QDir root(workspaceRoot);

        // C++ — Check CMakeLists.txt for GTest/Catch2
        if (root.exists(QStringLiteral("CMakeLists.txt"))) {
            QFile cmake(root.filePath(QStringLiteral("CMakeLists.txt")));
            if (cmake.open(QIODevice::ReadOnly)) {
                const QString content = QString::fromUtf8(cmake.readAll());
                if (content.contains(QStringLiteral("GTest"), Qt::CaseInsensitive) ||
                    content.contains(QStringLiteral("gtest"), Qt::CaseInsensitive))
                    return {GoogleTest, QStringLiteral("GoogleTest"),
                            QStringLiteral("#include <gtest/gtest.h>"),
                            QStringLiteral("*_test.cpp"), QStringLiteral("ctest --test-dir build")};
                if (content.contains(QStringLiteral("Catch2"), Qt::CaseInsensitive))
                    return {Catch2, QStringLiteral("Catch2"),
                            QStringLiteral("#include <catch2/catch_test_macros.hpp>"),
                            QStringLiteral("*_test.cpp"), QStringLiteral("ctest --test-dir build")};
                if (content.contains(QStringLiteral("Qt6Test"), Qt::CaseInsensitive) ||
                    content.contains(QStringLiteral("Qt5Test"), Qt::CaseInsensitive))
                    return {QtTest, QStringLiteral("Qt Test"),
                            QStringLiteral("#include <QTest>"),
                            QStringLiteral("tst_*.cpp"), QStringLiteral("ctest --test-dir build")};
            }
        }

        // JavaScript — package.json
        if (root.exists(QStringLiteral("package.json"))) {
            QFile pkg(root.filePath(QStringLiteral("package.json")));
            if (pkg.open(QIODevice::ReadOnly)) {
                const QString content = QString::fromUtf8(pkg.readAll());
                if (content.contains(QStringLiteral("jest")))
                    return {Jest, QStringLiteral("Jest"),
                            QStringLiteral("const { describe, it, expect } = require('@jest/globals');"),
                            QStringLiteral("*.test.js"), QStringLiteral("npm test")};
                if (content.contains(QStringLiteral("mocha")))
                    return {Mocha, QStringLiteral("Mocha"),
                            QStringLiteral("const { describe, it } = require('mocha');\nconst assert = require('assert');"),
                            QStringLiteral("*.test.js"), QStringLiteral("npm test")};
            }
        }

        // Python — pytest
        if (root.exists(QStringLiteral("pytest.ini")) ||
            root.exists(QStringLiteral("setup.cfg")) ||
            root.exists(QStringLiteral("pyproject.toml")))
            return {Pytest, QStringLiteral("pytest"),
                    QStringLiteral("import pytest"),
                    QStringLiteral("test_*.py"), QStringLiteral("pytest")};

        // Ruby — RSpec
        if (root.exists(QStringLiteral("Gemfile"))) {
            QFile gem(root.filePath(QStringLiteral("Gemfile")));
            if (gem.open(QIODevice::ReadOnly)) {
                if (QString::fromUtf8(gem.readAll()).contains(QStringLiteral("rspec")))
                    return {RSpec, QStringLiteral("RSpec"),
                            QStringLiteral("require 'rspec'"),
                            QStringLiteral("*_spec.rb"), QStringLiteral("bundle exec rspec")};
            }
        }

        return {Unknown, QStringLiteral("Unknown"), {}, {}, {}};
    }

    // Generate test file path for a source file
    QString testFilePath(const QString &sourceFile, const DetectionResult &fw) const
    {
        const QFileInfo info(sourceFile);
        const QString baseName = info.completeBaseName();
        const QString dir = info.absolutePath();

        switch (fw.framework) {
        case GoogleTest:
        case Catch2:
            return dir + QStringLiteral("/") + baseName + QStringLiteral("_test.cpp");
        case QtTest:
            return dir + QStringLiteral("/tst_") + baseName + QStringLiteral(".cpp");
        case Jest:
        case Mocha:
            return dir + QStringLiteral("/") + baseName + QStringLiteral(".test.js");
        case Pytest:
            return dir + QStringLiteral("/test_") + baseName + QStringLiteral(".py");
        case RSpec:
            return dir + QStringLiteral("/") + baseName + QStringLiteral("_spec.rb");
        default:
            return dir + QStringLiteral("/test_") + baseName + info.suffix();
        }
    }

    // Build AI prompt for test generation
    QString buildTestPrompt(const QString &code, const QString &filePath,
                            const QString &languageId,
                            const DetectionResult &fw,
                            const QString &customInstructions = {}) const
    {
        QString prompt = QStringLiteral(
            "Generate comprehensive unit tests for the following %1 code from `%2`.\n\n")
            .arg(languageId, filePath);

        if (fw.framework != Unknown)
            prompt += QStringLiteral("Use the **%1** testing framework.\n"
                                     "Import: `%2`\n\n")
                .arg(fw.name, fw.importStatement);

        prompt += QStringLiteral("```%1\n%2\n```\n\n").arg(languageId, code);

        prompt += QStringLiteral(
            "Requirements:\n"
            "- Test all public functions/methods\n"
            "- Include edge cases and error conditions\n"
            "- Use descriptive test names\n"
            "- Add setup/teardown if needed\n"
            "- Aim for high code coverage\n");

        if (!customInstructions.isEmpty())
            prompt += QStringLiteral("\n**Additional instructions:**\n%1\n").arg(customInstructions);

        return prompt;
    }

    // System prompt for test generation
    static QString testSystemPrompt()
    {
        return QStringLiteral(
            "You are an expert test engineer. Generate comprehensive, well-structured "
            "unit tests for the given code. Focus on:\n"
            "1. Testing all public interfaces\n"
            "2. Edge cases and boundary conditions\n"
            "3. Error handling paths\n"
            "4. Descriptive test names explaining what is tested\n"
            "5. Minimal mocking — prefer real objects when possible\n\n"
            "Output the complete test file content in a single code block.");
    }

    // Build context for fixing a failing test
    QString buildFixTestPrompt(const QString &testCode, const QString &sourceCode,
                                const QString &testOutput) const
    {
        return QStringLiteral(
            "The following test is failing. Fix either the test or the source code.\n\n"
            "**Test code:**\n```\n%1\n```\n\n"
            "**Source code:**\n```\n%2\n```\n\n"
            "**Test output:**\n```\n%3\n```\n\n"
            "Explain what's wrong and provide the fix.")
            .arg(testCode.left(6000), sourceCode.left(6000), testOutput.left(4000));
    }

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }
    QString workspaceRoot() const { return m_workspaceRoot; }

signals:
    void testsGenerated(const QString &testFilePath, const QString &content);

private:
    QString m_workspaceRoot;
};

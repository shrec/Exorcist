#pragma once

#include <QObject>
#include <QString>

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
    explicit TestGenerator(QObject *parent = nullptr);

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

    DetectionResult detectFramework(const QString &workspaceRoot) const;
    QString testFilePath(const QString &sourceFile, const DetectionResult &fw) const;

    QString buildTestPrompt(const QString &code, const QString &filePath,
                            const QString &languageId,
                            const DetectionResult &fw,
                            const QString &customInstructions = {}) const;

    static QString testSystemPrompt();

    QString buildFixTestPrompt(const QString &testCode, const QString &sourceCode,
                                const QString &testOutput) const;

    void setWorkspaceRoot(const QString &root);
    QString workspaceRoot() const { return m_workspaceRoot; }

signals:
    void testsGenerated(const QString &testFilePath, const QString &content);

private:
    QString m_workspaceRoot;
};

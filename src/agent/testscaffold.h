#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

/// Detects test frameworks and scaffolds test files
class TestScaffold : public QObject
{
    Q_OBJECT
public:
    explicit TestScaffold(QObject *parent = nullptr);

    enum Framework {
        Unknown,
        GoogleTest,
        Catch2,
        QtTest,
        Pytest,
        Jest,
        Mocha
    };

    /// Detect the framework used in the workspace
    Framework detectFramework(const QString &workspaceRoot) const;
    QString frameworkName(Framework fw) const;

    /// Generate a test file path from a source file path
    QString testFilePath(const QString &sourceFile, Framework fw) const;

    /// Generate test scaffold content for a source file
    QString generateScaffold(const QString &sourceFile,
                             const QString &sourceContent,
                             Framework fw) const;

signals:
    void scaffoldGenerated(const QString &testFile, const QString &content);
};

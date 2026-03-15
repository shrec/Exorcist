#pragma once

#include <QList>
#include <QObject>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// SetupTestsWizard — guides user through test framework setup.
//
// Detects project type, checks if test framework is configured, and
// provides scaffolding commands to set up testing infrastructure.
// ─────────────────────────────────────────────────────────────────────────────

class SetupTestsWizard : public QObject
{
    Q_OBJECT

public:
    explicit SetupTestsWizard(QObject *parent = nullptr);

    struct SetupStep {
        QString title;
        QString description;
        QString command;
        QString fileToCreate;
        QString fileContent;
    };

    QList<SetupStep> generateSetupSteps(const QString &workspaceRoot) const;
    QString buildSetupPrompt(const QString &workspaceRoot) const;

signals:
    void setupCompleted();
};

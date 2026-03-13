#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// ─────────────────────────────────────────────────────────────────────────────
// CommitMessageGenerator — generates git commit messages from staged diffs.
//
// Reads `git diff --cached` and produces a conventional commit message.
// Can optionally use AI (via IAIProvider) for smarter messages.
// ─────────────────────────────────────────────────────────────────────────────

class CommitMessageGenerator : public QObject
{
    Q_OBJECT

public:
    explicit CommitMessageGenerator(QObject *parent = nullptr)
        : QObject(parent) {}

    void setWorkspaceRoot(const QString &root) { m_root = root; }

    QString stagedDiff() const;
    QStringList stagedFiles() const;
    QString generateBasic() const;
    QString buildAIContext(const QString &customInstructions = {}) const;

signals:
    void messageGenerated(const QString &message);
    void generationFailed(const QString &error);

private:
    QString m_root;
};

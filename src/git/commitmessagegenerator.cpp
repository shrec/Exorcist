#include "commitmessagegenerator.h"

#include <QProcess>

QString CommitMessageGenerator::stagedDiff() const
{
    QProcess proc;
    proc.setWorkingDirectory(m_root);
    proc.start(QStringLiteral("git"),
                {QStringLiteral("diff"), QStringLiteral("--cached"),
                 QStringLiteral("--no-color")});
    if (!proc.waitForFinished(10000))
        return {};
    return QString::fromUtf8(proc.readAllStandardOutput());
}

QStringList CommitMessageGenerator::stagedFiles() const
{
    QProcess proc;
    proc.setWorkingDirectory(m_root);
    proc.start(QStringLiteral("git"),
                {QStringLiteral("diff"), QStringLiteral("--cached"),
                 QStringLiteral("--name-only")});
    if (!proc.waitForFinished(5000))
        return {};
    const QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (output.isEmpty()) return {};
    return output.split('\n');
}

QString CommitMessageGenerator::generateBasic() const
{
    const QStringList files = stagedFiles();
    if (files.isEmpty())
        return {};

    const QString diff = stagedDiff();
    if (diff.isEmpty())
        return {};

    int additions = 0, deletions = 0;
    const auto lines = diff.split('\n');
    for (const QString &line : lines) {
        if (line.startsWith('+') && !line.startsWith(QStringLiteral("+++")))
            ++additions;
        else if (line.startsWith('-') && !line.startsWith(QStringLiteral("---")))
            ++deletions;
    }

    QString prefix;
    if (files.size() == 1) {
        const QString &file = files.first();
        if (deletions == 0)
            prefix = QStringLiteral("Add");
        else if (additions == 0)
            prefix = QStringLiteral("Remove code from");
        else
            prefix = QStringLiteral("Update");

        const int slashIdx = file.lastIndexOf('/');
        const QString shortName = (slashIdx >= 0) ? file.mid(slashIdx + 1) : file;
        return QStringLiteral("%1 %2").arg(prefix, shortName);
    }

    if (deletions == 0 && additions > 0)
        prefix = QStringLiteral("Add");
    else if (additions == 0 && deletions > 0)
        prefix = QStringLiteral("Remove");
    else
        prefix = QStringLiteral("Update");

    QString commonDir;
    if (files.size() > 1) {
        const QString &first = files.first();
        const int lastSlash = first.lastIndexOf('/');
        if (lastSlash > 0) {
            const QString dir = first.left(lastSlash);
            bool allSame = true;
            for (const QString &f : files) {
                if (!f.startsWith(dir + '/')) {
                    allSame = false;
                    break;
                }
            }
            if (allSame) commonDir = dir;
        }
    }

    if (!commonDir.isEmpty())
        return QStringLiteral("%1 %2 files in %3")
            .arg(prefix)
            .arg(files.size())
            .arg(commonDir);

    return QStringLiteral("%1 %2 files").arg(prefix).arg(files.size());
}

QString CommitMessageGenerator::buildAIContext(const QString &customInstructions) const
{
    const QString diff = stagedDiff();
    const QStringList files = stagedFiles();

    QString context;
    context += QStringLiteral("Generate a git commit message for the following changes.\n");
    context += QStringLiteral("Use imperative mood, max 72 chars subject line.\n\n");

    if (!customInstructions.isEmpty()) {
        context += QStringLiteral("Custom instructions:\n");
        context += customInstructions + QStringLiteral("\n\n");
    }

    context += QStringLiteral("Changed files:\n");
    for (const QString &f : files)
        context += QStringLiteral("- %1\n").arg(f);

    const int maxDiffSize = 8000;
    if (diff.size() > maxDiffSize) {
        context += QStringLiteral("\nDiff (truncated to %1 chars):\n").arg(maxDiffSize);
        context += diff.left(maxDiffSize);
        context += QStringLiteral("\n...(truncated)");
    } else {
        context += QStringLiteral("\nDiff:\n");
        context += diff;
    }

    return context;
}

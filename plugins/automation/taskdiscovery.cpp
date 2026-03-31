#include "taskdiscovery.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

TaskDiscovery::TaskDiscovery(QObject *parent)
    : QObject(parent)
{
}

void TaskDiscovery::setWorkspaceRoot(const QString &root)
{
    if (m_root == root) return;
    m_root = root;
    refresh();
}

void TaskDiscovery::refresh()
{
    m_tasks.clear();
    if (m_root.isEmpty()) {
        emit tasksChanged();
        return;
    }

    const QDir dir(m_root);

    // Taskfile.yml / Taskfile.yaml  (go-task)
    for (const auto &name : { "Taskfile.yml", "Taskfile.yaml" }) {
        const QString p = dir.filePath(QLatin1String(name));
        if (QFileInfo::exists(p))
            discoverTaskfile(p);
    }

    // justfile / Justfile  (casey/just)
    for (const auto &name : { "justfile", "Justfile" }) {
        const QString p = dir.filePath(QLatin1String(name));
        if (QFileInfo::exists(p))
            discoverJustfile(p);
    }

    // Makefile
    {
        const QString p = dir.filePath(QStringLiteral("Makefile"));
        if (QFileInfo::exists(p))
            discoverMakefile(p);
    }

    // package.json scripts (npm/yarn/pnpm)
    {
        const QString p = dir.filePath(QStringLiteral("package.json"));
        if (QFileInfo::exists(p))
            discoverNpmScripts(p);
    }

    // Loose scripts in scripts/ or .scripts/
    for (const auto &sub : { "scripts", ".scripts" }) {
        const QString d = dir.filePath(QLatin1String(sub));
        if (QFileInfo(d).isDir())
            discoverScripts(d);
    }

    emit tasksChanged();
}

// ── Taskfile.yml (go-task YAML) ──────────────────────────────────────────────
// Minimal parse: look for top-level "tasks:" key, then each sub-key is a task name.
// Full YAML parsing would require a YAML library; we do regex-based extraction.

void TaskDiscovery::discoverTaskfile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    const QString source = QFileInfo(path).fileName();
    QTextStream in(&f);

    bool inTasks = false;
    static const QRegularExpression taskNameRe(QStringLiteral(R"(^  (\w[\w.-]*):)"));

    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.startsWith(QLatin1String("tasks:"))) {
            inTasks = true;
            continue;
        }
        if (inTasks) {
            // Stop at next top-level key
            if (!line.isEmpty() && !line.at(0).isSpace() && line.at(0) != QLatin1Char('#')) {
                inTasks = false;
                continue;
            }
            const auto m = taskNameRe.match(line);
            if (m.hasMatch()) {
                TaskEntry te;
                te.name = m.captured(1);
                te.command = QStringLiteral("task %1").arg(te.name);
                te.source = source;
                te.sourceType = QStringLiteral("taskfile");
                m_tasks.append(te);
            }
        }
    }
}

// ── justfile (casey/just) ────────────────────────────────────────────────────
// Recipes start at column 0, are identifiers followed by ':'.

void TaskDiscovery::discoverJustfile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    const QString source = QFileInfo(path).fileName();
    QTextStream in(&f);

    static const QRegularExpression recipeRe(QStringLiteral(R"(^(\w[\w-]*)\s*:)"));

    while (!in.atEnd()) {
        const QString line = in.readLine();
        const auto m = recipeRe.match(line);
        if (m.hasMatch()) {
            TaskEntry te;
            te.name = m.captured(1);
            te.command = QStringLiteral("just %1").arg(te.name);
            te.source = source;
            te.sourceType = QStringLiteral("justfile");
            m_tasks.append(te);
        }
    }
}

// ── Makefile ─────────────────────────────────────────────────────────────────
// Parse targets: lines matching "target: [deps]" at column 0.
// Skip built-in / pattern / dot-prefixed targets.

void TaskDiscovery::discoverMakefile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    const QString source = QFileInfo(path).fileName();
    QTextStream in(&f);

    static const QRegularExpression targetRe(QStringLiteral(R"(^([\w][\w.-]*)\s*:(?!=))"));

    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.startsWith(QLatin1Char('\t')) || line.startsWith(QLatin1Char('#')))
            continue;
        const auto m = targetRe.match(line);
        if (m.hasMatch()) {
            const QString name = m.captured(1);
            // Skip common internal targets
            if (name.startsWith(QLatin1Char('.')) || name == QLatin1String("FORCE"))
                continue;
            TaskEntry te;
            te.name = name;
            te.command = QStringLiteral("make %1").arg(name);
            te.source = source;
            te.sourceType = QStringLiteral("makefile");
            m_tasks.append(te);
        }
    }
}

// ── package.json scripts ─────────────────────────────────────────────────────

void TaskDiscovery::discoverNpmScripts(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;

    const QJsonObject scripts = doc.object().value(QLatin1String("scripts")).toObject();
    const QString source = QFileInfo(path).fileName();

    for (auto it = scripts.begin(); it != scripts.end(); ++it) {
        TaskEntry te;
        te.name = it.key();
        te.command = QStringLiteral("npm run %1").arg(it.key());
        te.description = it.value().toString();
        te.source = source;
        te.sourceType = QStringLiteral("npm");
        m_tasks.append(te);
    }
}

// ── Loose scripts ────────────────────────────────────────────────────────────

void TaskDiscovery::discoverScripts(const QString &dir)
{
    const QDir d(dir);
    const QStringList filters = {
        QStringLiteral("*.sh"), QStringLiteral("*.bash"),
        QStringLiteral("*.ps1"), QStringLiteral("*.py"),
        QStringLiteral("*.lua"), QStringLiteral("*.rb"),
    };

    const QFileInfoList entries = d.entryInfoList(filters, QDir::Files, QDir::Name);
    for (const QFileInfo &fi : entries) {
        TaskEntry te;
        te.name = fi.fileName();
        // Choose interpreter based on extension
        const QString ext = fi.suffix().toLower();
        if (ext == QLatin1String("py"))
            te.command = QStringLiteral("python \"%1\"").arg(fi.absoluteFilePath());
        else if (ext == QLatin1String("lua"))
            te.command = QStringLiteral("lua \"%1\"").arg(fi.absoluteFilePath());
        else if (ext == QLatin1String("rb"))
            te.command = QStringLiteral("ruby \"%1\"").arg(fi.absoluteFilePath());
        else if (ext == QLatin1String("ps1"))
            te.command = QStringLiteral("powershell -File \"%1\"").arg(fi.absoluteFilePath());
        else
            te.command = QStringLiteral("\"%1\"").arg(fi.absoluteFilePath());
        te.source = QStringLiteral("scripts/");
        te.sourceType = QStringLiteral("script");
        m_tasks.append(te);
    }
}

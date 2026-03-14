#include "packagemanagertools.h"
#include "../terminalsessionmanager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// ── PackageJsonInfoTool ──────────────────────────────────────────────────────

ToolSpec PackageJsonInfoTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("package_json_info");
    s.description = QStringLiteral(
        "Read and summarize the project's package.json: name, version, "
        "available scripts, dependencies, and devDependencies. "
        "Optionally reads a specific package.json path.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.contexts    = {QStringLiteral("web"), QStringLiteral("node")};
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to package.json. Defaults to workspace root.")}
            }}
        }}
    };
    return s;
}

ToolExecResult PackageJsonInfoTool::invoke(const QJsonObject &args)
{
    QString path = args[QLatin1String("path")].toString();
    if (path.isEmpty())
        path = QStringLiteral("package.json");

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {false, {}, {}, QStringLiteral("Cannot read: %1").arg(path)};

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (doc.isNull())
        return {false, {}, {}, QStringLiteral("JSON parse error: %1").arg(err.errorString())};

    QJsonObject pkg = doc.object();
    QJsonObject data;

    const QString name = pkg[QLatin1String("name")].toString();
    const QString version = pkg[QLatin1String("version")].toString();

    data[QStringLiteral("name")]    = name;
    data[QStringLiteral("version")] = version;

    // Collect scripts
    QJsonObject scripts = pkg[QLatin1String("scripts")].toObject();
    data[QStringLiteral("scripts")] = scripts;

    // Collect dependency counts
    QJsonObject deps    = pkg[QLatin1String("dependencies")].toObject();
    QJsonObject devDeps = pkg[QLatin1String("devDependencies")].toObject();
    data[QStringLiteral("dependencyCount")]    = deps.size();
    data[QStringLiteral("devDependencyCount")] = devDeps.size();

    QString text = QStringLiteral("%1@%2\n").arg(name, version);
    if (!scripts.isEmpty()) {
        text += QStringLiteral("\nScripts:\n");
        for (auto it = scripts.begin(); it != scripts.end(); ++it)
            text += QStringLiteral("  %1: %2\n").arg(it.key(), it.value().toString());
    }
    text += QStringLiteral("\nDependencies: %1, DevDependencies: %2\n")
                .arg(deps.size()).arg(devDeps.size());

    return {true, data, text, {}};
}

// ── NpmRunTool ───────────────────────────────────────────────────────────────

ToolSpec NpmRunTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("npm_run");
    s.description = QStringLiteral(
        "Run an npm/yarn/pnpm script defined in package.json. "
        "Automatically detects the package manager (checks for lock files). "
        "Supports background execution for dev servers and watch tasks.");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 120000;
    s.contexts    = {QStringLiteral("web"), QStringLiteral("node")};
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("script"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The npm script name to run (e.g. 'build', 'dev', 'test').")}
            }},
            {QStringLiteral("isBackground"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("Run in background (for dev servers, watchers). Default: false.")}
            }},
            {QStringLiteral("extraArgs"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Additional arguments to pass after the script.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("script")}}
    };
    return s;
}

ToolExecResult NpmRunTool::invoke(const QJsonObject &args)
{
    const QString script = args[QLatin1String("script")].toString();
    if (script.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: script")};

    // Validate script name
    if (script.contains(QLatin1Char(';')) || script.contains(QLatin1Char('|'))
        || script.contains(QLatin1Char('&')) || script.contains(QLatin1Char('`')))
        return {false, {}, {}, QStringLiteral("Invalid script name: %1").arg(script)};

    const QString extraArgs = args[QLatin1String("extraArgs")].toString();
    const bool background = args[QLatin1String("isBackground")].toBool(false);

    const QString pm = detectPackageManager();
    QString cmd = QStringLiteral("%1 run %2").arg(pm, script);
    if (!extraArgs.isEmpty())
        cmd += QStringLiteral(" -- %1").arg(extraArgs);

    if (background) {
        const QString id = m_mgr->runBackground(cmd, QStringLiteral("npm run %1").arg(script));
        QJsonObject data;
        data[QStringLiteral("id")]         = id;
        data[QStringLiteral("isBackground")] = true;
        data[QStringLiteral("command")]    = cmd;
        return {true, data,
                QStringLiteral("Started '%1' in background (session: %2)").arg(cmd, id),
                {}};
    }

    TerminalSession s = m_mgr->runForeground(cmd, 120000);
    QJsonObject data;
    data[QStringLiteral("exitCode")] = s.exitCode;

    QString text = QStringLiteral("$ %1\n").arg(cmd);
    if (!s.stdOut.isEmpty()) text += s.stdOut + QLatin1Char('\n');
    if (!s.stdErr.isEmpty()) text += s.stdErr + QLatin1Char('\n');

    return {s.exitCode == 0, data, text,
            s.exitCode == 0 ? QString() : s.stdErr};
}

QString NpmRunTool::detectPackageManager()
{
    if (QFile::exists(QStringLiteral("pnpm-lock.yaml")))
        return QStringLiteral("pnpm");
    if (QFile::exists(QStringLiteral("yarn.lock")))
        return QStringLiteral("yarn");
    if (QFile::exists(QStringLiteral("bun.lockb")))
        return QStringLiteral("bun");
    return QStringLiteral("npm");
}

// ── InstallNodePackagesTool ──────────────────────────────────────────────────

ToolSpec InstallNodePackagesTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("install_node_packages");
    s.description = QStringLiteral(
        "Install Node.js packages. If no packages specified, runs 'npm install' "
        "(or yarn/pnpm equivalent) to install from package.json. Otherwise "
        "installs the specified packages. Auto-detects package manager.");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 180000;
    s.contexts    = {QStringLiteral("web"), QStringLiteral("node")};
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("packages"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")}
                }},
                {QStringLiteral("description"),
                 QStringLiteral("Package names to install. Omit to install from package.json.")}
            }},
            {QStringLiteral("dev"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("Install as devDependency. Default: false.")}
            }}
        }}
    };
    return s;
}

ToolExecResult InstallNodePackagesTool::invoke(const QJsonObject &args)
{
    const QJsonArray pkgs = args[QLatin1String("packages")].toArray();
    const bool dev = args[QLatin1String("dev")].toBool(false);

    QStringList pkgNames;
    for (const auto &p : pkgs) {
        const QString name = p.toString();
        // Validate (no shell injection)
        if (name.contains(QLatin1Char(';')) || name.contains(QLatin1Char('|'))
            || name.contains(QLatin1Char('&')) || name.contains(QLatin1Char('`')))
            return {false, {}, {},
                    QStringLiteral("Invalid package name: %1").arg(name)};
        pkgNames.append(name);
    }

    const QString pm = detectPackageManager();
    QString cmd;

    if (pkgNames.isEmpty()) {
        cmd = QStringLiteral("%1 install").arg(pm);
    } else {
        const QString devFlag = dev
            ? (pm == QLatin1String("yarn") ? QStringLiteral(" --dev")
                                           : QStringLiteral(" --save-dev"))
            : QString();
        cmd = QStringLiteral("%1 %2 %3%4")
                  .arg(pm,
                       pm == QLatin1String("yarn") ? QStringLiteral("add")
                                                   : QStringLiteral("install"),
                       pkgNames.join(QLatin1Char(' ')),
                       devFlag);
    }

    TerminalSession s = m_mgr->runForeground(cmd, 180000);
    QJsonObject data;
    data[QStringLiteral("exitCode")] = s.exitCode;
    data[QStringLiteral("command")]  = cmd;

    QString text = QStringLiteral("$ %1\n").arg(cmd);
    if (!s.stdOut.isEmpty()) text += s.stdOut + QLatin1Char('\n');
    if (!s.stdErr.isEmpty()) text += s.stdErr + QLatin1Char('\n');

    return {s.exitCode == 0, data, text,
            s.exitCode == 0 ? QString() : s.stdErr};
}

QString InstallNodePackagesTool::detectPackageManager()
{
    if (QFile::exists(QStringLiteral("pnpm-lock.yaml")))
        return QStringLiteral("pnpm");
    if (QFile::exists(QStringLiteral("yarn.lock")))
        return QStringLiteral("yarn");
    if (QFile::exists(QStringLiteral("bun.lockb")))
        return QStringLiteral("bun");
    return QStringLiteral("npm");
}

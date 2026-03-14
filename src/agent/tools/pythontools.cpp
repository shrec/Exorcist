#include "pythontools.h"
#include "../terminalsessionmanager.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>

// ── PythonEnvTool ────────────────────────────────────────────────────────────

ToolSpec PythonEnvTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("python_env");
    s.description = QStringLiteral(
        "Get Python environment information: interpreter path, version, "
        "active virtual environment, and installed packages. Use before "
        "running Python scripts or installing packages.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 15000;
    s.contexts    = {QStringLiteral("python")};
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("listPackages"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("If true, include pip freeze output (installed packages). Default: false.")}
            }}
        }}
    };
    return s;
}

ToolExecResult PythonEnvTool::invoke(const QJsonObject &args)
{
    const bool listPkgs = args[QLatin1String("listPackages")].toBool(false);

    // Build a compound command to gather Python environment info
    QString cmd;
#ifdef Q_OS_WIN
    cmd = QStringLiteral(
        "python --version 2>&1 && python -c \"import sys; print('Path:', sys.executable); "
        "print('Prefix:', sys.prefix); print('Base:', sys.base_prefix)\"");
    if (listPkgs)
        cmd += QStringLiteral(" && pip freeze 2>&1");
#else
    cmd = QStringLiteral(
        "python3 --version 2>&1 && python3 -c \"import sys; print('Path:', sys.executable); "
        "print('Prefix:', sys.prefix); print('Base:', sys.base_prefix)\"");
    if (listPkgs)
        cmd += QStringLiteral(" && pip3 freeze 2>&1");
#endif

    TerminalSession s = m_mgr->runForeground(cmd, 15000);

    QJsonObject data;
    data[QStringLiteral("exitCode")] = s.exitCode;

    QString text;
    if (s.exitCode != 0) {
        text = QStringLiteral("Python not found or error occurred.\n");
        if (!s.stdErr.isEmpty())
            text += s.stdErr;
    } else {
        text = s.stdOut;
    }

    return {s.exitCode == 0, data, text,
            s.exitCode == 0 ? QString() : QStringLiteral("Python detection failed")};
}

// ── InstallPythonPackagesTool ────────────────────────────────────────────────

ToolSpec InstallPythonPackagesTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("install_python_packages");
    s.description = QStringLiteral(
        "Install Python packages using pip. Accepts a list of package "
        "names (e.g. ['requests', 'flask>=2.0']). Runs in the current "
        "Python environment.");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 120000;
    s.contexts    = {QStringLiteral("python")};
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("packages"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")}
                }},
                {QStringLiteral("description"),
                 QStringLiteral("List of package names/specifiers to install.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("packages")}}
    };
    return s;
}

ToolExecResult InstallPythonPackagesTool::invoke(const QJsonObject &args)
{
    const QJsonArray pkgs = args[QLatin1String("packages")].toArray();
    if (pkgs.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: packages")};

    QStringList pkgNames;
    for (const auto &p : pkgs)
        pkgNames.append(p.toString());

    // Validate package names (no shell injection)
    for (const QString &name : pkgNames) {
        if (name.contains(QLatin1Char(';')) || name.contains(QLatin1Char('|'))
            || name.contains(QLatin1Char('&')) || name.contains(QLatin1Char('`')))
            return {false, {}, {},
                    QStringLiteral("Invalid package name: %1").arg(name)};
    }

#ifdef Q_OS_WIN
    const QString pip = QStringLiteral("pip");
#else
    const QString pip = QStringLiteral("pip3");
#endif

    const QString cmd = QStringLiteral("%1 install %2")
                            .arg(pip, pkgNames.join(QLatin1Char(' ')));

    TerminalSession s = m_mgr->runForeground(cmd, 120000);

    QJsonObject data;
    data[QStringLiteral("exitCode")] = s.exitCode;

    QString text = QStringLiteral("pip install %1\n").arg(pkgNames.join(QLatin1Char(' ')));
    if (!s.stdOut.isEmpty()) text += s.stdOut + QLatin1Char('\n');
    if (!s.stdErr.isEmpty()) text += s.stdErr + QLatin1Char('\n');

    return {s.exitCode == 0, data, text,
            s.exitCode == 0 ? QString() : s.stdErr};
}

// ── RunPythonTool ────────────────────────────────────────────────────────────

ToolSpec RunPythonTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("run_python");
    s.description = QStringLiteral(
        "Run a Python code snippet in the current Python interpreter. "
        "The code is passed via stdin to avoid shell escaping issues. "
        "Returns stdout, stderr, and exit code.");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 60000;
    s.contexts    = {QStringLiteral("python")};
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("code"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The Python code to execute.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("code")}}
    };
    return s;
}

ToolExecResult RunPythonTool::invoke(const QJsonObject &args)
{
    const QString code = args[QLatin1String("code")].toString();
    if (code.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: code")};

    // Use QProcess directly for stdin piping
    QProcess proc;
    if (!m_mgr)
        return {false, {}, {}, QStringLiteral("Session manager not available")};

#ifdef Q_OS_WIN
    proc.setProgram(QStringLiteral("python"));
#else
    proc.setProgram(QStringLiteral("python3"));
#endif
    proc.setArguments({QStringLiteral("-u"), QStringLiteral("-c"), code});
    proc.start();

    if (!proc.waitForStarted(5000))
        return {false, {}, {}, QStringLiteral("Failed to start Python interpreter")};

    proc.closeWriteChannel();
    proc.waitForFinished(60000);

    const int exitCode = proc.exitCode();
    const QString stdOut = QString::fromUtf8(proc.readAllStandardOutput());
    const QString stdErr = QString::fromUtf8(proc.readAllStandardError());

    QJsonObject data;
    data[QStringLiteral("exitCode")] = exitCode;

    QString text;
    if (!stdOut.isEmpty()) text += stdOut + QLatin1Char('\n');
    if (!stdErr.isEmpty()) text += QStringLiteral("stderr:\n") + stdErr + QLatin1Char('\n');

    return {exitCode == 0, data, text,
            exitCode == 0 ? QString() : stdErr};
}

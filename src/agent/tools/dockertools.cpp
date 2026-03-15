#include "dockertools.h"

#include <QDir>
#include <QJsonArray>

// ── DockerTool ────────────────────────────────────────────────────────────────

DockerTool::DockerTool(IProcess *process)
    : m_process(process)
{
}

ToolSpec DockerTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("docker");
    s.description = QStringLiteral(
        "Manage Docker containers and images. Supported actions:\n"
        "- ps: List running containers (add 'all: true' for stopped too)\n"
        "- build: Build an image from a Dockerfile\n"
        "- run: Run a container from an image\n"
        "- exec: Execute a command in a running container\n"
        "- stop: Stop a running container\n"
        "- rm: Remove a container\n"
        "- images: List images\n"
        "- logs: Get container logs\n"
        "- inspect: Inspect a container or image");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 120000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("action"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("ps"), QStringLiteral("build"),
                    QStringLiteral("run"), QStringLiteral("exec"),
                    QStringLiteral("stop"), QStringLiteral("rm"),
                    QStringLiteral("images"), QStringLiteral("logs"),
                    QStringLiteral("inspect")}},
                {QStringLiteral("description"),
                 QStringLiteral("The Docker action to perform.")}
            }},
            {QStringLiteral("container"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Container name or ID (for exec, stop, rm, logs, inspect).")}
            }},
            {QStringLiteral("image"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Image name[:tag] (for build, run, inspect).")}
            }},
            {QStringLiteral("command"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Command to run (for run, exec). "
                                "Use shell syntax — will be passed to 'sh -c'.")}
            }},
            {QStringLiteral("dockerfile"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to Dockerfile (for build). Defaults to './Dockerfile'.")}
            }},
            {QStringLiteral("args"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                {QStringLiteral("description"),
                 QStringLiteral("Additional raw arguments passed to docker CLI.")}
            }},
            {QStringLiteral("all"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("For 'ps': show all containers including stopped.")}
            }},
            {QStringLiteral("tail"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("For 'logs': number of lines from the end (default 100).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("action")}}
    };
    return s;
}

ToolExecResult DockerTool::invoke(const QJsonObject &args)
{
    const QString action    = args[QLatin1String("action")].toString();
    const QString container = args[QLatin1String("container")].toString();
    const QString image     = args[QLatin1String("image")].toString();
    const QString command   = args[QLatin1String("command")].toString();
    const QString dockerfile= args[QLatin1String("dockerfile")].toString();
    const bool    showAll   = args[QLatin1String("all")].toBool(false);
    const int     tail      = args[QLatin1String("tail")].toInt(100);

    // Collect extra args
    QStringList extraArgs;
    const QJsonArray argsArr = args[QLatin1String("args")].toArray();
    for (const auto &v : argsArr)
        extraArgs << v.toString();

    if (action == QLatin1String("ps")) {
        QStringList da{QStringLiteral("ps"), QStringLiteral("--format"),
                       QStringLiteral("table {{.ID}}\t{{.Image}}\t{{.Status}}\t{{.Names}}")};
        if (showAll)
            da << QStringLiteral("-a");
        da << extraArgs;
        return runDocker(da, 15000);
    }

    if (action == QLatin1String("images")) {
        QStringList da{QStringLiteral("images"), QStringLiteral("--format"),
                       QStringLiteral("table {{.Repository}}\t{{.Tag}}\t{{.Size}}\t{{.ID}}")};
        da << extraArgs;
        return runDocker(da, 15000);
    }

    if (action == QLatin1String("build")) {
        if (image.isEmpty())
            return {false, {}, {}, QStringLiteral("'image' (tag name) is required for build.")};
        QStringList da{QStringLiteral("build"), QStringLiteral("-t"), image};
        if (!dockerfile.isEmpty())
            da << QStringLiteral("-f") << dockerfile;
        da << extraArgs;
        // Build context = workspace root
        da << (m_workspaceRoot.isEmpty() ? QStringLiteral(".") : m_workspaceRoot);
        return runDocker(da, 300000);  // 5 min for builds
    }

    if (action == QLatin1String("run")) {
        if (image.isEmpty())
            return {false, {}, {}, QStringLiteral("'image' is required for run.")};
        QStringList da{QStringLiteral("run"), QStringLiteral("--rm")};
        da << extraArgs << image;
        if (!command.isEmpty())
            da << QStringLiteral("sh") << QStringLiteral("-c") << command;
        return runDocker(da, 120000);
    }

    if (action == QLatin1String("exec")) {
        if (container.isEmpty())
            return {false, {}, {}, QStringLiteral("'container' is required for exec.")};
        if (command.isEmpty())
            return {false, {}, {}, QStringLiteral("'command' is required for exec.")};
        QStringList da{QStringLiteral("exec"), container,
                       QStringLiteral("sh"), QStringLiteral("-c"), command};
        da << extraArgs;
        return runDocker(da, 60000);
    }

    if (action == QLatin1String("stop")) {
        if (container.isEmpty())
            return {false, {}, {}, QStringLiteral("'container' is required for stop.")};
        QStringList da{QStringLiteral("stop"), container};
        da << extraArgs;
        return runDocker(da, 30000);
    }

    if (action == QLatin1String("rm")) {
        if (container.isEmpty())
            return {false, {}, {}, QStringLiteral("'container' is required for rm.")};
        QStringList da{QStringLiteral("rm"), container};
        da << extraArgs;
        return runDocker(da, 15000);
    }

    if (action == QLatin1String("logs")) {
        if (container.isEmpty())
            return {false, {}, {}, QStringLiteral("'container' is required for logs.")};
        QStringList da{QStringLiteral("logs"), QStringLiteral("--tail"),
                       QString::number(tail), container};
        da << extraArgs;
        return runDocker(da, 15000);
    }

    if (action == QLatin1String("inspect")) {
        const QString target = !container.isEmpty() ? container : image;
        if (target.isEmpty())
            return {false, {}, {},
                    QStringLiteral("'container' or 'image' is required for inspect.")};
        QStringList da{QStringLiteral("inspect"), target};
        da << extraArgs;
        return runDocker(da, 15000);
    }

    return {false, {}, {},
            QStringLiteral("Unknown action: '%1'").arg(action)};
}

ToolExecResult DockerTool::runDocker(const QStringList &args, int timeoutMs)
{
    if (!m_process)
        return {false, {}, {}, QStringLiteral("Process runner not available.")};

    const ProcessResult r = m_process->run(QStringLiteral("docker"), args, timeoutMs);

    if (r.timedOut)
        return {false, {}, {},
                QStringLiteral("Docker command timed out after %1ms").arg(timeoutMs)};

    // Combine stdout and stderr
    QString output = r.stdOut;
    if (!r.stdErr.isEmpty()) {
        if (!output.isEmpty())
            output += QLatin1Char('\n');
        output += r.stdErr;
    }

    // Truncate to 20KB
    constexpr int kMaxOutput = 20000;
    if (output.size() > kMaxOutput) {
        output = QStringLiteral("... (truncated %1 chars) ...\n")
                     .arg(output.size() - kMaxOutput)
               + output.right(kMaxOutput);
    }

    if (!r.success && r.exitCode != 0)
        return {false, {}, {},
                QStringLiteral("Docker exited with code %1:\n%2")
                    .arg(r.exitCode).arg(output)};

    return {true, {}, output, {}};
}

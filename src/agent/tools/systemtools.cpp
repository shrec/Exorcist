#include "systemtools.h"

#include <QHostAddress>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QProcess>
#include <QTcpSocket>

// ── ProcessManagementTool ────────────────────────────────────────────────────

ToolSpec ProcessManagementTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("process_manager");
    s.description = QStringLiteral(
        "Manage system processes and check ports. Operations:\n"
        "  'list' — list running processes (optional filter by name)\n"
        "  'check_port' — check if a TCP port is in use\n"
        "  'find_by_port' — find which process is using a port\n"
        "  'kill' — kill a process by PID (use with caution)");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 15000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("operation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("One of: list, check_port, find_by_port, kill")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("list"), QStringLiteral("check_port"),
                    QStringLiteral("find_by_port"), QStringLiteral("kill")}}
            }},
            {QStringLiteral("filter"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Process name filter for 'list'.")}
            }},
            {QStringLiteral("port"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("TCP port number for check_port/find_by_port.")}
            }},
            {QStringLiteral("pid"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Process ID to kill.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
    };
    return s;
}

ToolExecResult ProcessManagementTool::invoke(const QJsonObject &args)
{
    const QString op = args[QLatin1String("operation")].toString();

    if (op == QLatin1String("list"))
        return listProcesses(args[QLatin1String("filter")].toString());
    if (op == QLatin1String("check_port"))
        return checkPort(args[QLatin1String("port")].toInt());
    if (op == QLatin1String("find_by_port"))
        return findByPort(args[QLatin1String("port")].toInt());
    if (op == QLatin1String("kill"))
        return killProcess(args[QLatin1String("pid")].toInt());

    return {false, {}, {},
        QStringLiteral("Unknown operation: %1").arg(op)};
}

ToolExecResult ProcessManagementTool::listProcesses(const QString &filter)
{
    QProcess proc;
#ifdef Q_OS_WIN
    proc.start(QStringLiteral("tasklist"), {QStringLiteral("/FO"), QStringLiteral("CSV")});
#else
    proc.start(QStringLiteral("ps"), {QStringLiteral("aux")});
#endif
    proc.waitForFinished(10000);
    QString output = proc.readAllStandardOutput();

    if (!filter.isEmpty()) {
        QStringList lines = output.split(QLatin1Char('\n'));
        QStringList filtered;
        for (int i = 0; i < lines.size(); ++i) {
            if (i == 0 || lines[i].contains(filter, Qt::CaseInsensitive))
                filtered << lines[i];
        }
        output = filtered.join(QLatin1Char('\n'));
    }

    // Truncate if too large
    if (output.size() > 32768)
        output = output.left(32768) + QStringLiteral("\n... (truncated)");

    return {true, {}, output, {}};
}

ToolExecResult ProcessManagementTool::checkPort(int port)
{
    if (port <= 0 || port > 65535)
        return {false, {}, {}, QStringLiteral("Invalid port number.")};

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
    bool inUse = socket.waitForConnected(1000);
    socket.close();

    return {true, {},
        inUse ? QStringLiteral("Port %1 is IN USE.").arg(port)
              : QStringLiteral("Port %1 is AVAILABLE.").arg(port), {}};
}

ToolExecResult ProcessManagementTool::findByPort(int port)
{
    if (port <= 0 || port > 65535)
        return {false, {}, {}, QStringLiteral("Invalid port number.")};

    QProcess proc;
#ifdef Q_OS_WIN
    proc.start(QStringLiteral("netstat"),
        {QStringLiteral("-ano"), QStringLiteral("-p"), QStringLiteral("TCP")});
#else
    proc.start(QStringLiteral("lsof"),
        {QStringLiteral("-i"), QStringLiteral(":%1").arg(port), QStringLiteral("-P")});
#endif
    proc.waitForFinished(10000);
    const QString output = proc.readAllStandardOutput();

    // Filter for our port
    QStringList lines = output.split(QLatin1Char('\n'));
    QStringList matches;
    const QString portStr = QString::number(port);
    for (const QString &line : lines) {
        if (line.contains(QLatin1Char(':') + portStr))
            matches << line.trimmed();
    }

    if (matches.isEmpty())
        return {true, {},
            QStringLiteral("No process found using port %1.").arg(port), {}};

    return {true, {}, matches.join(QLatin1Char('\n')), {}};
}

ToolExecResult ProcessManagementTool::killProcess(int pid)
{
    if (pid <= 0)
        return {false, {}, {}, QStringLiteral("Invalid PID.")};

    QProcess proc;
#ifdef Q_OS_WIN
    proc.start(QStringLiteral("taskkill"),
        {QStringLiteral("/PID"), QString::number(pid), QStringLiteral("/F")});
#else
    proc.start(QStringLiteral("kill"), {QString::number(pid)});
#endif
    proc.waitForFinished(5000);
    const QString output = proc.readAllStandardOutput() + proc.readAllStandardError();

    if (proc.exitCode() != 0)
        return {false, {}, {},
            QStringLiteral("Failed to kill PID %1: %2").arg(pid).arg(output)};

    return {true, {},
        QStringLiteral("Killed process %1.").arg(pid), {}};
}

// ── NetworkTool ──────────────────────────────────────────────────────────────

ToolSpec NetworkTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("network");
    s.description = QStringLiteral(
        "Network diagnostic tools. Operations:\n"
        "  'ping' — ping a host (ICMP echo)\n"
        "  'dns' — DNS lookup for a hostname\n"
        "  'port_scan' — check if specific ports are open on a host\n"
        "  'local_ip' — show local network interfaces");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 30000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("operation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("One of: ping, dns, port_scan, local_ip")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("ping"), QStringLiteral("dns"),
                    QStringLiteral("port_scan"), QStringLiteral("local_ip")}}
            }},
            {QStringLiteral("host"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Hostname or IP address.")}
            }},
            {QStringLiteral("ports"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")}}},
                {QStringLiteral("description"),
                 QStringLiteral("List of ports to scan (for port_scan).")}
            }},
            {QStringLiteral("count"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Number of ping packets (default: 4).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
    };
    return s;
}

ToolExecResult NetworkTool::invoke(const QJsonObject &args)
{
    const QString op = args[QLatin1String("operation")].toString();

    if (op == QLatin1String("ping"))
        return ping(args);
    if (op == QLatin1String("dns"))
        return dnsLookup(args);
    if (op == QLatin1String("port_scan"))
        return portScan(args);
    if (op == QLatin1String("local_ip"))
        return localIp();

    return {false, {}, {},
        QStringLiteral("Unknown operation: %1").arg(op)};
}

ToolExecResult NetworkTool::ping(const QJsonObject &args)
{
    const QString host = args[QLatin1String("host")].toString();
    if (host.isEmpty())
        return {false, {}, {}, QStringLiteral("'host' is required for 'ping'.")};

    const int count = qBound(1, args[QLatin1String("count")].toInt(4), 10);

    QProcess proc;
#ifdef Q_OS_WIN
    proc.start(QStringLiteral("ping"),
        {QStringLiteral("-n"), QString::number(count), host});
#else
    proc.start(QStringLiteral("ping"),
        {QStringLiteral("-c"), QString::number(count), host});
#endif
    proc.waitForFinished(count * 5000 + 5000);
    const QString output = proc.readAllStandardOutput() + proc.readAllStandardError();

    return {proc.exitCode() == 0, {}, output,
            proc.exitCode() != 0 ? QStringLiteral("Ping failed.") : QString()};
}

ToolExecResult NetworkTool::dnsLookup(const QJsonObject &args)
{
    const QString host = args[QLatin1String("host")].toString();
    if (host.isEmpty())
        return {false, {}, {}, QStringLiteral("'host' is required for 'dns'.")};

    const QHostInfo info = QHostInfo::fromName(host);
    if (info.error() != QHostInfo::NoError)
        return {false, {}, {},
            QStringLiteral("DNS lookup failed: %1").arg(info.errorString())};

    QStringList addresses;
    for (const QHostAddress &addr : info.addresses())
        addresses << addr.toString();

    QString result = QStringLiteral("Host: %1\n").arg(host);
    if (!info.hostName().isEmpty())
        result += QStringLiteral("Hostname: %1\n").arg(info.hostName());
    result += QStringLiteral("Addresses:\n");
    for (const QString &a : addresses)
        result += QStringLiteral("  %1\n").arg(a);

    return {true, {}, result, {}};
}

ToolExecResult NetworkTool::portScan(const QJsonObject &args)
{
    const QString host = args[QLatin1String("host")].toString();
    if (host.isEmpty())
        return {false, {}, {}, QStringLiteral("'host' is required for 'port_scan'.")};

    const QJsonArray portsArr = args[QLatin1String("ports")].toArray();
    if (portsArr.isEmpty())
        return {false, {}, {},
            QStringLiteral("'ports' array is required for 'port_scan'.")};

    // Limit scan to 100 ports
    if (portsArr.size() > 100)
        return {false, {}, {},
            QStringLiteral("Maximum 100 ports per scan.")};

    QString result = QStringLiteral("Port scan for %1:\n").arg(host);
    for (const auto &p : portsArr) {
        const int port = p.toInt();
        if (port <= 0 || port > 65535) continue;

        QTcpSocket socket;
        socket.connectToHost(host, static_cast<quint16>(port));
        bool open = socket.waitForConnected(2000);
        socket.close();

        result += QStringLiteral("  %1: %2\n")
            .arg(port)
            .arg(open ? QStringLiteral("OPEN") : QStringLiteral("CLOSED"));
    }

    return {true, {}, result, {}};
}

ToolExecResult NetworkTool::localIp()
{
    QString result = QStringLiteral("Local network interfaces:\n");
    const auto allAddrs = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : allAddrs) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))
            continue;
        result += QStringLiteral("\n%1 (%2):\n")
            .arg(iface.name(), iface.humanReadableName());
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            result += QStringLiteral("  %1/%2\n")
                .arg(entry.ip().toString())
                .arg(entry.prefixLength());
        }
    }

    return {true, {}, result, {}};
}

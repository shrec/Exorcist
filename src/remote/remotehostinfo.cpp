#include "remotehostinfo.h"
#include "sshsession.h"

// ── RemoteHostInfo helpers ────────────────────────────────────────────────────

CpuArch RemoteHostInfo::archFromUname(const QString &unameM)
{
    const QString u = unameM.trimmed().toLower();
    if (u == QLatin1String("x86_64") || u == QLatin1String("amd64"))
        return CpuArch::X86_64;
    if (u == QLatin1String("aarch64") || u == QLatin1String("arm64"))
        return CpuArch::AArch64;
    if (u.startsWith(QLatin1String("armv")) || u == QLatin1String("armhf"))
        return CpuArch::ARM32;
    if (u == QLatin1String("riscv64"))
        return CpuArch::RiscV64;
    if (u == QLatin1String("riscv32"))
        return CpuArch::RiscV32;
    if (u == QLatin1String("i386") || u == QLatin1String("i486")
        || u == QLatin1String("i586") || u == QLatin1String("i686"))
        return CpuArch::X86;
    if (u.startsWith(QLatin1String("mips64")))
        return CpuArch::MIPS64;
    if (u.startsWith(QLatin1String("ppc64")) || u == QLatin1String("ppc64le"))
        return CpuArch::PPC64;
    if (u == QLatin1String("s390x"))
        return CpuArch::S390X;
    if (u == QLatin1String("loongarch64"))
        return CpuArch::LoongArch64;
    return CpuArch::Unknown;
}

QString RemoteHostInfo::archLabel(CpuArch a)
{
    switch (a) {
    case CpuArch::X86_64:      return QStringLiteral("x86_64");
    case CpuArch::AArch64:     return QStringLiteral("AArch64 (ARM64)");
    case CpuArch::ARM32:       return QStringLiteral("ARM32");
    case CpuArch::RiscV64:     return QStringLiteral("RISC-V 64");
    case CpuArch::RiscV32:     return QStringLiteral("RISC-V 32");
    case CpuArch::X86:         return QStringLiteral("x86 (32-bit)");
    case CpuArch::MIPS64:      return QStringLiteral("MIPS64");
    case CpuArch::PPC64:       return QStringLiteral("PowerPC 64");
    case CpuArch::S390X:       return QStringLiteral("s390x");
    case CpuArch::LoongArch64: return QStringLiteral("LoongArch64");
    case CpuArch::Unknown:     return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

QString RemoteHostInfo::targetTriple() const
{
    QString archPart;
    switch (arch) {
    case CpuArch::X86_64:      archPart = QStringLiteral("x86_64");  break;
    case CpuArch::AArch64:     archPart = QStringLiteral("aarch64"); break;
    case CpuArch::ARM32:       archPart = QStringLiteral("arm");     break;
    case CpuArch::RiscV64:     archPart = QStringLiteral("riscv64"); break;
    case CpuArch::RiscV32:     archPart = QStringLiteral("riscv32"); break;
    case CpuArch::X86:         archPart = QStringLiteral("i686");    break;
    case CpuArch::MIPS64:      archPart = QStringLiteral("mips64");  break;
    case CpuArch::PPC64:       archPart = QStringLiteral("ppc64le"); break;
    case CpuArch::S390X:       archPart = QStringLiteral("s390x");   break;
    case CpuArch::LoongArch64: archPart = QStringLiteral("loongarch64"); break;
    case CpuArch::Unknown:     return {};
    }

    // Determine OS part
    const QString os = osName.toLower();
    if (os == QLatin1String("linux"))
        return archPart + QStringLiteral("-linux-gnu");
    if (os == QLatin1String("darwin"))
        return archPart + QStringLiteral("-apple-darwin");
    if (os.contains(QLatin1String("bsd")))
        return archPart + QStringLiteral("-unknown-freebsd");

    return archPart + QStringLiteral("-unknown-linux-gnu");
}

QString RemoteHostInfo::displayString() const
{
    QStringList parts;
    parts << osName;
    if (!archString.isEmpty())
        parts << archString;
    if (!distro.isEmpty()) {
        QString d = distro;
        if (!distroVersion.isEmpty())
            d += QLatin1Char(' ') + distroVersion;
        parts << d;
    }
    if (cpuCores > 0)
        parts << QStringLiteral("%1 cores").arg(cpuCores);
    if (memoryMB > 0)
        parts << QStringLiteral("%1 MB RAM").arg(memoryMB);
    return parts.join(QStringLiteral(" · "));
}

// ── RemoteHostProber ──────────────────────────────────────────────────────────

RemoteHostProber::RemoteHostProber(QObject *parent)
    : QObject(parent)
{
}

void RemoteHostProber::probe(SshSession *session)
{
    m_session = session;
    m_info = {};
    m_pendingProbes = 4;

    if (!session || !session->isConnected()) {
        emit probeError(tr("No active SSH session"));
        return;
    }

    // 1. uname -a (arch + os + kernel)
    const int r1 = session->runCommand(QStringLiteral("uname -m && uname -s && uname -r"));
    auto c1 = std::make_shared<QMetaObject::Connection>();
    *c1 = connect(session, &SshSession::commandFinished,
                  this, [this, r1, c1](int reqId, int exitCode, const QString &output) {
        if (reqId != r1) return;
        disconnect(*c1);
        if (exitCode == 0)
            parseUnameResult(output);
        --m_pendingProbes;
        checkComplete();
    });

    // 2. /etc/os-release (distro info)
    const int r2 = session->runCommand(QStringLiteral("cat /etc/os-release 2>/dev/null || echo NONE"));
    auto c2 = std::make_shared<QMetaObject::Connection>();
    *c2 = connect(session, &SshSession::commandFinished,
                  this, [this, r2, c2](int reqId, int exitCode, const QString &output) {
        if (reqId != r2) return;
        disconnect(*c2);
        if (exitCode == 0)
            parseOsRelease(output);
        --m_pendingProbes;
        checkComplete();
    });

    // 3. CPU/memory info
    const int r3 = session->runCommand(
        QStringLiteral("nproc 2>/dev/null && "
                        "awk '/MemTotal/{print int($2/1024)}' /proc/meminfo 2>/dev/null"));
    auto c3 = std::make_shared<QMetaObject::Connection>();
    *c3 = connect(session, &SshSession::commandFinished,
                  this, [this, r3, c3](int reqId, int exitCode, const QString &output) {
        if (reqId != r3) return;
        disconnect(*c3);
        if (exitCode == 0)
            parseCpuMemory(output);
        --m_pendingProbes;
        checkComplete();
    });

    // 4. Available toolchains
    const int r4 = session->runCommand(
        QStringLiteral(
            "for t in gcc g++ clang clang++ rustc go; do command -v $t >/dev/null 2>&1 && echo \"compiler:$t\"; done; "
            "for t in cmake make ninja meson cargo; do command -v $t >/dev/null 2>&1 && echo \"build:$t\"; done; "
            "for t in apt dnf yum pacman apk zypper brew; do command -v $t >/dev/null 2>&1 && echo \"pkg:$t\"; done; "
            "for t in gdb lldb; do command -v $t >/dev/null 2>&1 && echo \"dbg:$t\"; done"
        ));
    auto c4 = std::make_shared<QMetaObject::Connection>();
    *c4 = connect(session, &SshSession::commandFinished,
                  this, [this, r4, c4](int reqId, int exitCode, const QString &output) {
        if (reqId != r4) return;
        disconnect(*c4);
        if (exitCode == 0)
            parseToolchains(output);
        --m_pendingProbes;
        checkComplete();
    });
}

void RemoteHostProber::parseUnameResult(const QString &output)
{
    const QStringList lines = output.trimmed().split(QLatin1Char('\n'));
    if (lines.size() >= 1) {
        m_info.archString = lines.at(0).trimmed();
        m_info.arch = RemoteHostInfo::archFromUname(m_info.archString);
    }
    if (lines.size() >= 2)
        m_info.osName = lines.at(1).trimmed();
    if (lines.size() >= 3)
        m_info.osRelease = lines.at(2).trimmed();
}

void RemoteHostProber::parseOsRelease(const QString &output)
{
    if (output.trimmed() == QLatin1String("NONE"))
        return;

    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed();
        QString val = line.mid(eq + 1).trimmed();
        // Strip quotes
        if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')))
            val = val.mid(1, val.size() - 2);

        if (key == QLatin1String("NAME") || key == QLatin1String("PRETTY_NAME"))
            m_info.distro = val;
        else if (key == QLatin1String("VERSION_ID"))
            m_info.distroVersion = val;
    }
}

void RemoteHostProber::parseCpuMemory(const QString &output)
{
    const QStringList lines = output.trimmed().split(QLatin1Char('\n'));
    if (lines.size() >= 1)
        m_info.cpuCores = lines.at(0).trimmed().toInt();
    if (lines.size() >= 2)
        m_info.memoryMB = lines.at(1).trimmed().toLongLong();
}

void RemoteHostProber::parseToolchains(const QString &output)
{
    const QStringList lines = output.trimmed().split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) continue;
        const QString category = line.left(colon).trimmed();
        const QString tool     = line.mid(colon + 1).trimmed();
        if (tool.isEmpty()) continue;

        if (category == QLatin1String("compiler"))
            m_info.compilers << tool;
        else if (category == QLatin1String("build"))
            m_info.buildSystems << tool;
        else if (category == QLatin1String("pkg"))
            m_info.packageManagers << tool;
        else if (category == QLatin1String("dbg"))
            m_info.debuggers << tool;
    }
}

void RemoteHostProber::checkComplete()
{
    if (m_pendingProbes <= 0)
        emit probeFinished(m_info);
}

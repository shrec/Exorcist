#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

class SshSession;

/// Supported CPU architecture families.
enum class CpuArch
{
    Unknown,
    X86_64,     // amd64, x86_64
    AArch64,    // arm64, aarch64
    ARM32,      // armv7l, armhf
    RiscV64,    // riscv64
    RiscV32,    // riscv32
    X86,        // i386, i686
    MIPS64,     // mips64, mips64el
    PPC64,      // ppc64, ppc64le
    S390X,      // s390x
    LoongArch64 // loongarch64
};

/// Remote host hardware and OS information, detected via SSH probing.
struct RemoteHostInfo
{
    CpuArch     arch        = CpuArch::Unknown;
    QString     archString;     // raw output of `uname -m`  (e.g. "aarch64")
    QString     osName;         // `uname -s`                (e.g. "Linux")
    QString     osRelease;      // `uname -r`                (e.g. "6.1.0-rpi")
    QString     distro;         // from /etc/os-release NAME  (e.g. "Ubuntu")
    QString     distroVersion;  // from /etc/os-release VERSION_ID (e.g. "24.04")
    int         cpuCores    = 0;
    qint64      memoryMB    = 0;

    // Detected toolchains available on the remote host
    QStringList compilers;      // e.g. {"gcc", "g++", "clang", "clang++"}
    QStringList buildSystems;   // e.g. {"cmake", "make", "ninja", "meson"}
    QStringList packageManagers;// e.g. {"apt", "dnf", "pacman", "apk"}
    QStringList debuggers;      // e.g. {"gdb", "lldb"}

    /// The `--target` triple for cross-compilation (e.g. "aarch64-linux-gnu").
    QString targetTriple() const;

    /// Friendly display string (e.g. "Linux aarch64 · Ubuntu 24.04 · 4 cores · 4096 MB").
    QString displayString() const;

    /// Convert raw `uname -m` to CpuArch enum.
    static CpuArch archFromUname(const QString &unameM);

    /// Human-readable architecture label.
    static QString archLabel(CpuArch a);
};

/// Probes a remote host via SSH to detect architecture, OS, toolchains,
/// and hardware info.  All operations are async — results via signals.
class RemoteHostProber : public QObject
{
    Q_OBJECT

public:
    explicit RemoteHostProber(QObject *parent = nullptr);

    /// Start probing the given session.  Emits probeFinished when done.
    void probe(SshSession *session);

    /// Latest probe result.
    const RemoteHostInfo &info() const { return m_info; }

signals:
    /// Emitted when probing is complete.
    void probeFinished(const RemoteHostInfo &info);
    /// Emitted if probing fails.
    void probeError(const QString &error);

private:
    void parseUnameResult(const QString &output);
    void parseOsRelease(const QString &output);
    void parseCpuMemory(const QString &output);
    void parseToolchains(const QString &output);
    void checkComplete();

    SshSession   *m_session = nullptr;
    RemoteHostInfo m_info;
    int m_pendingProbes = 0;
};

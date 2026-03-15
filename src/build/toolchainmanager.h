#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>

// ── Toolchain types ───────────────────────────────────────────────────────────

/// A single detected compiler, debugger, or build system tool.
struct ToolInfo
{
    QString name;       // e.g. "gcc", "clang++", "cl.exe"
    QString path;       // absolute path to executable
    QString version;    // version string (e.g. "13.2.0")

    bool isValid() const { return !path.isEmpty(); }
};

/// Represents a complete C/C++ toolchain (compiler + linker + debugger).
struct Toolchain
{
    QString id;                 // unique id (e.g. "gcc-13", "msvc-2022", "clang-17")
    QString displayName;        // human-readable (e.g. "GCC 13.2 (MinGW)")

    ToolInfo cCompiler;         // gcc, clang, cl.exe
    ToolInfo cxxCompiler;       // g++, clang++, cl.exe
    ToolInfo debugger;          // gdb, lldb
    ToolInfo linker;            // ld, lld, link.exe

    enum class Type { GCC, Clang, MSVC, MinGW, LlvmMinGW, Unknown };
    Type type = Type::Unknown;

    /// Target triple (e.g. "x86_64-w64-mingw32")
    QString targetTriple;

    bool isValid() const { return cxxCompiler.isValid(); }
};

/// Detected build system tool.
struct BuildSystemInfo
{
    QString name;       // "cmake", "ninja", "make", "msbuild"
    QString path;
    QString version;

    bool isValid() const { return !path.isEmpty(); }
};

// ── ToolchainManager ──────────────────────────────────────────────────────────

/// Detects and manages C/C++ toolchains on the local machine.
/// Scans PATH and well-known install locations for compilers,
/// debuggers, and build systems.
class ToolchainManager : public QObject
{
    Q_OBJECT

public:
    explicit ToolchainManager(QObject *parent = nullptr);

    /// Run full detection. Emits detectionFinished when done.
    void detectAll();

    /// Detected toolchains (available after detectAll).
    QList<Toolchain> toolchains() const { return m_toolchains; }

    /// Detected build systems.
    QList<BuildSystemInfo> buildSystems() const { return m_buildSystems; }

    /// Find a specific build system by name.
    BuildSystemInfo findBuildSystem(const QString &name) const;

    /// Get the preferred/default toolchain. Returns invalid if none detected.
    Toolchain defaultToolchain() const;

    /// Set the preferred toolchain by id.
    void setDefaultToolchain(const QString &id);

    /// Find clangd on the system.
    ToolInfo findClangd() const;

    /// Extract a version number (e.g. "17.0.6") from version output.
    QString extractVersion(const QString &output) const;

signals:
    void detectionFinished();
    void detectionError(const QString &error);

private:
    void detectCompilers();
    void detectBuildSystems();
    void detectDebuggers();
    void matchToolchains();

    ToolInfo probeExecutable(const QString &name,
                             const QStringList &versionArgs = {QStringLiteral("--version")}) const;
    ToolInfo probeAtPath(const QString &path,
                         const QStringList &versionArgs = {QStringLiteral("--version")}) const;
    QStringList searchPaths() const;
#ifdef Q_OS_WIN
    void detectMsvcToolchains();
#endif

    QList<Toolchain> m_toolchains;
    QList<BuildSystemInfo> m_buildSystems;
    QMap<QString, ToolInfo> m_compilerCache;
    QMap<QString, ToolInfo> m_debuggerCache;
    QString m_defaultToolchainId;
};

#include "toolchainmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <QSettings>
#include <windows.h>
#endif

ToolchainManager::ToolchainManager(QObject *parent)
    : QObject(parent)
{
}

// ── Public API ────────────────────────────────────────────────────────────────

void ToolchainManager::detectAll()
{
    m_toolchains.clear();
    m_buildSystems.clear();
    m_compilerCache.clear();
    m_debuggerCache.clear();

    detectCompilers();
    detectDebuggers();
    detectBuildSystems();
    matchToolchains();

    emit detectionFinished();
}

BuildSystemInfo ToolchainManager::findBuildSystem(const QString &name) const
{
    for (const auto &bs : m_buildSystems) {
        if (bs.name.compare(name, Qt::CaseInsensitive) == 0)
            return bs;
    }
    return {};
}

Toolchain ToolchainManager::defaultToolchain() const
{
    if (!m_defaultToolchainId.isEmpty()) {
        for (const auto &tc : m_toolchains) {
            if (tc.id == m_defaultToolchainId)
                return tc;
        }
    }
    // Return first valid toolchain
    for (const auto &tc : m_toolchains) {
        if (tc.isValid()) return tc;
    }
    return {};
}

void ToolchainManager::setDefaultToolchain(const QString &id)
{
    m_defaultToolchainId = id;
}

ToolInfo ToolchainManager::findClangd() const
{
    // Try clangd in PATH first
    ToolInfo info = probeExecutable(QStringLiteral("clangd"));
    if (info.isValid()) return info;

#ifdef Q_OS_WIN
    // Check common LLVM install locations on Windows
    const QStringList llvmPaths = {
        QStringLiteral("C:/Program Files/LLVM/bin/clangd.exe"),
        QStringLiteral("C:/Program Files (x86)/LLVM/bin/clangd.exe"),
    };
    for (const auto &p : llvmPaths) {
        info = probeAtPath(p);
        if (info.isValid()) return info;
    }

    // Check LLVM MinGW
    const QStringList envPaths = QProcessEnvironment::systemEnvironment()
                                     .value(QStringLiteral("PATH"))
                                     .split(QLatin1Char(';'));
    for (const auto &dir : envPaths) {
        if (dir.contains(QLatin1String("llvm"), Qt::CaseInsensitive)) {
            const QString candidate = dir + QStringLiteral("/clangd.exe");
            if (QFileInfo::exists(candidate)) {
                info = probeAtPath(candidate);
                if (info.isValid()) return info;
            }
        }
    }
#else
    // Common Linux/macOS locations
    const QStringList paths = {
        QStringLiteral("/usr/bin/clangd"),
        QStringLiteral("/usr/local/bin/clangd"),
        QStringLiteral("/usr/lib/llvm-17/bin/clangd"),
        QStringLiteral("/usr/lib/llvm-16/bin/clangd"),
        QStringLiteral("/usr/lib/llvm-15/bin/clangd"),
        QStringLiteral("/opt/homebrew/bin/clangd"),
    };
    for (const auto &p : paths) {
        info = probeAtPath(p);
        if (info.isValid()) return info;
    }
#endif

    return {};
}

// ── Compiler detection ────────────────────────────────────────────────────────

void ToolchainManager::detectCompilers()
{
    // Probe common C compilers
    const QStringList cNames = {
        QStringLiteral("gcc"),
        QStringLiteral("clang"),
#ifdef Q_OS_WIN
        QStringLiteral("cl"),
#endif
    };

    for (const auto &name : cNames) {
        ToolInfo info = probeExecutable(name);
        if (info.isValid())
            m_compilerCache.insert(QStringLiteral("c_") + name, info);
    }

    // Probe common C++ compilers
    const QStringList cxxNames = {
        QStringLiteral("g++"),
        QStringLiteral("clang++"),
#ifdef Q_OS_WIN
        QStringLiteral("cl"),
#endif
    };

    for (const auto &name : cxxNames) {
        ToolInfo info = probeExecutable(name);
        if (info.isValid())
            m_compilerCache.insert(QStringLiteral("cxx_") + name, info);
    }

#ifdef Q_OS_WIN
    // Detect MSVC via VS installation
    detectMsvcToolchains();
#endif
}

// ── Debugger detection ────────────────────────────────────────────────────────

void ToolchainManager::detectDebuggers()
{
    const QStringList names = {
        QStringLiteral("gdb"),
        QStringLiteral("lldb"),
    };

    for (const auto &name : names) {
        ToolInfo info = probeExecutable(name);
        if (info.isValid())
            m_debuggerCache.insert(name, info);
    }
}

// ── Build system detection ────────────────────────────────────────────────────

void ToolchainManager::detectBuildSystems()
{
    struct Candidate {
        QString name;
        QStringList versionArgs;
    };

    const QList<Candidate> candidates = {
        {QStringLiteral("cmake"),   {QStringLiteral("--version")}},
        {QStringLiteral("ninja"),   {QStringLiteral("--version")}},
        {QStringLiteral("make"),    {QStringLiteral("--version")}},
#ifdef Q_OS_WIN
        {QStringLiteral("msbuild"), {QStringLiteral("/version")}},
#endif
    };

    for (const auto &c : candidates) {
        ToolInfo info = probeExecutable(c.name, c.versionArgs);
        if (info.isValid()) {
            BuildSystemInfo bs;
            bs.name = c.name;
            bs.path = info.path;
            bs.version = info.version;
            m_buildSystems.append(bs);
        }
    }
}

// ── Toolchain assembly ────────────────────────────────────────────────────────

void ToolchainManager::matchToolchains()
{
    // GCC toolchain
    if (m_compilerCache.contains(QStringLiteral("cxx_g++"))) {
        Toolchain tc;
        tc.cxxCompiler = m_compilerCache.value(QStringLiteral("cxx_g++"));
        tc.cCompiler = m_compilerCache.value(QStringLiteral("c_gcc"));

        // Determine if MinGW
        const bool isMinGW = tc.cxxCompiler.version.contains(
            QLatin1String("mingw"), Qt::CaseInsensitive);

#ifdef Q_OS_WIN
        tc.type = isMinGW ? Toolchain::Type::MinGW : Toolchain::Type::GCC;
#else
        Q_UNUSED(isMinGW);
        tc.type = Toolchain::Type::GCC;
#endif

        // Extract version for id
        const QString ver = tc.cxxCompiler.version.section(QLatin1Char(' '), 0, 0);
        tc.id = QStringLiteral("gcc-") + ver;
        tc.displayName = QStringLiteral("GCC %1").arg(ver);
#ifdef Q_OS_WIN
        if (tc.type == Toolchain::Type::MinGW)
            tc.displayName += QStringLiteral(" (MinGW)");
#endif

        // Prefer GDB for GCC
        if (m_debuggerCache.contains(QStringLiteral("gdb")))
            tc.debugger = m_debuggerCache.value(QStringLiteral("gdb"));

        // Try to detect target triple
        {
            QProcess proc;
            proc.start(tc.cxxCompiler.path, {QStringLiteral("-dumpmachine")});
            if (proc.waitForFinished(3000) && proc.exitCode() == 0)
                tc.targetTriple = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        }

        m_toolchains.append(tc);
    }

    // Clang toolchain
    if (m_compilerCache.contains(QStringLiteral("cxx_clang++"))) {
        Toolchain tc;
        tc.cxxCompiler = m_compilerCache.value(QStringLiteral("cxx_clang++"));
        tc.cCompiler = m_compilerCache.value(QStringLiteral("c_clang"));

        // Check for LLVM MinGW
        const bool isLlvmMinGW = tc.cxxCompiler.path.contains(
            QLatin1String("llvm-mingw"), Qt::CaseInsensitive);

#ifdef Q_OS_WIN
        tc.type = isLlvmMinGW ? Toolchain::Type::LlvmMinGW : Toolchain::Type::Clang;
#else
        Q_UNUSED(isLlvmMinGW);
        tc.type = Toolchain::Type::Clang;
#endif

        const QString ver = extractVersion(tc.cxxCompiler.version);
        tc.id = QStringLiteral("clang-") + ver;
        tc.displayName = QStringLiteral("Clang %1").arg(ver);
#ifdef Q_OS_WIN
        if (tc.type == Toolchain::Type::LlvmMinGW)
            tc.displayName += QStringLiteral(" (LLVM MinGW)");
#endif

        // Prefer LLDB for Clang, fall back to GDB
        if (m_debuggerCache.contains(QStringLiteral("lldb")))
            tc.debugger = m_debuggerCache.value(QStringLiteral("lldb"));
        else if (m_debuggerCache.contains(QStringLiteral("gdb")))
            tc.debugger = m_debuggerCache.value(QStringLiteral("gdb"));

        // Target triple
        {
            QProcess proc;
            proc.start(tc.cxxCompiler.path, {QStringLiteral("-dumpmachine")});
            if (proc.waitForFinished(3000) && proc.exitCode() == 0)
                tc.targetTriple = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        }

        m_toolchains.append(tc);
    }

#ifdef Q_OS_WIN
    // MSVC toolchain (cl.exe)
    if (m_compilerCache.contains(QStringLiteral("cxx_cl"))) {
        Toolchain tc;
        tc.type = Toolchain::Type::MSVC;
        tc.cxxCompiler = m_compilerCache.value(QStringLiteral("cxx_cl"));
        tc.cCompiler = tc.cxxCompiler; // cl handles both C and C++
        tc.id = QStringLiteral("msvc");
        tc.displayName = QStringLiteral("MSVC (cl.exe)");
        tc.targetTriple = QStringLiteral("x86_64-pc-windows-msvc");
        m_toolchains.append(tc);
    }
#endif
}

// ── Probing helpers ───────────────────────────────────────────────────────────

ToolInfo ToolchainManager::probeExecutable(const QString &name,
                                           const QStringList &versionArgs) const
{
    const QString path = QStandardPaths::findExecutable(name);
    if (path.isEmpty()) return {};

    return probeAtPath(path, versionArgs);
}

ToolInfo ToolchainManager::probeAtPath(const QString &path,
                                       const QStringList &versionArgs) const
{
    if (!QFileInfo::exists(path)) return {};

    ToolInfo info;
    info.name = QFileInfo(path).baseName();
    info.path = path;

    // Try to get version
#ifdef Q_OS_WIN
    // Suppress "missing DLL" system error dialogs on Windows
    const UINT oldMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    QProcess proc;
    proc.start(path, versionArgs);
    if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
        const QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (output.isEmpty()) {
            // Some tools (like cl.exe) output version on stderr
            info.version = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        } else {
            info.version = output;
        }
    } else {
        // Process failed to start or crashed — mark as invalid
        info = {};
    }
#ifdef Q_OS_WIN
    SetErrorMode(oldMode);
#endif

    return info;
}

QString ToolchainManager::extractVersion(const QString &output) const
{
    // Match patterns like "13.2.0", "17.0.6", etc.
    static const QRegularExpression versionRe(QStringLiteral(R"((\d+\.\d+(?:\.\d+)?))"));
    const auto match = versionRe.match(output);
    if (match.hasMatch())
        return match.captured(1);
    return output.section(QLatin1Char('\n'), 0, 0).trimmed();
}

QStringList ToolchainManager::searchPaths() const
{
    QStringList paths;

    // System PATH
    const QString pathEnv = QProcessEnvironment::systemEnvironment()
                                .value(QStringLiteral("PATH"));
#ifdef Q_OS_WIN
    paths = pathEnv.split(QLatin1Char(';'), Qt::SkipEmptyParts);

    // Add well-known Windows install locations
    paths << QStringLiteral("C:/Program Files/LLVM/bin")
          << QStringLiteral("C:/Program Files (x86)/LLVM/bin")
          << QStringLiteral("C:/msys64/mingw64/bin")
          << QStringLiteral("C:/msys64/ucrt64/bin")
          << QStringLiteral("C:/msys64/clang64/bin");
#else
    paths = pathEnv.split(QLatin1Char(':'), Qt::SkipEmptyParts);

    // Common Unix locations
    paths << QStringLiteral("/usr/bin")
          << QStringLiteral("/usr/local/bin")
          << QStringLiteral("/opt/homebrew/bin");
#endif

    paths.removeDuplicates();
    return paths;
}

// ── MSVC detection (Windows-only) ─────────────────────────────────────────────

#ifdef Q_OS_WIN
void ToolchainManager::detectMsvcToolchains()
{
    // Use vswhere.exe to find Visual Studio installations
    const QString vswhere = QStringLiteral(
        "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe");

    if (!QFileInfo::exists(vswhere)) return;

    QProcess proc;
    proc.start(vswhere, {
        QStringLiteral("-latest"),
        QStringLiteral("-products"), QStringLiteral("*"),
        QStringLiteral("-requires"), QStringLiteral("Microsoft.VisualStudio.Component.VC.Tools.x86.x64"),
        QStringLiteral("-property"), QStringLiteral("installationPath"),
    });

    if (!proc.waitForFinished(10000) || proc.exitCode() != 0) return;

    const QString vsPath = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (vsPath.isEmpty()) return;

    // Find cl.exe under the VS installation
    const QString toolsDir = vsPath + QStringLiteral("/VC/Tools/MSVC");
    QDir dir(toolsDir);
    if (!dir.exists()) return;

    const QStringList versions = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (versions.isEmpty()) return;

    // Use the latest version
    const QString latestVersion = versions.last();
    const QString clPath = toolsDir + QLatin1Char('/') + latestVersion
                           + QStringLiteral("/bin/Hostx64/x64/cl.exe");

    if (QFileInfo::exists(clPath)) {
        ToolInfo info;
        info.name = QStringLiteral("cl");
        info.path = clPath;
        info.version = latestVersion;
        m_compilerCache.insert(QStringLiteral("c_cl"), info);
        m_compilerCache.insert(QStringLiteral("cxx_cl"), info);
    }
}
#endif

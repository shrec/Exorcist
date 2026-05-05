#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QUuid>

// ── Kit — Unified Build Environment Bundle ───────────────────────────────────
//
// A Kit bundles everything needed to configure, build, and debug a project:
//   - C/C++ compiler (toolchain identity)
//   - Debugger
//   - CMake executable
//   - Preferred generator (Ninja, Makefiles, etc.)
//   - Target triple
//   - Optional sysroot / Qt installation path (future)
//
// Kits are detected automatically from the local machine and persisted
// to settings.  Users can also create custom kits.
//
// Design: Kit is a value type.  IKitManager is the management interface.

struct ToolInfo;
struct Toolchain;
struct BuildSystemInfo;

/// A single Kit — everything needed to build + debug one configuration.
struct Kit
{
    /// Stable unique id (persisted across sessions).
    QString id;

    /// Human-readable display name (e.g. "Clang 17 — Ninja — LLDB").
    QString displayName;

    /// Whether this kit was auto-detected (true) or user-created (false).
    bool autoDetected = true;

    // ── Compiler ──────────────────────────────────────────────────────────

    /// Toolchain type (GCC, Clang, MSVC, MinGW, LlvmMinGW).
    enum class CompilerType { GCC, Clang, MSVC, MinGW, LlvmMinGW, Unknown };
    CompilerType compilerType = CompilerType::Unknown;

    /// C compiler path.
    QString cCompilerPath;

    /// C++ compiler path.
    QString cxxCompilerPath;

    /// Compiler version string.
    QString compilerVersion;

    /// Target triple (e.g. "x86_64-w64-mingw32").
    QString targetTriple;

    // ── Debugger ──────────────────────────────────────────────────────────

    /// Debugger executable path (gdb, lldb, cdb).
    QString debuggerPath;

    /// Debugger name for display.
    QString debuggerName;

    // ── Build system ──────────────────────────────────────────────────────

    /// CMake executable path.
    QString cmakePath;

    /// CMake version string.
    QString cmakeVersion;

    /// Preferred generator (e.g. "Ninja", "Unix Makefiles", "Visual Studio 17 2022").
    QString generator;

    /// Generator executable path (e.g. path to ninja or make).
    QString generatorPath;

    // ── Validation ────────────────────────────────────────────────────────

    /// A kit is valid if it has at least a C++ compiler and CMake.
    bool isValid() const { return !cxxCompilerPath.isEmpty() && !cmakePath.isEmpty(); }

    /// Generate a display name from components.
    static QString buildDisplayName(const QString &compilerName,
                                    const QString &compilerVer,
                                    const QString &gen,
                                    const QString &dbg);
};

/// Kit manager interface — detects, persists, and manages kits.
///
/// Core code and plugins resolve this via ServiceRegistry("kitManager").
/// The manager auto-detects available kits on startup and allows
/// the user to select an active kit per workspace.
class IKitManager : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    virtual ~IKitManager() = default;

    // ── Query ─────────────────────────────────────────────────────────────

    /// All available kits (auto-detected + user-defined).
    virtual QList<Kit> kits() const = 0;

    /// The currently active kit (returns invalid Kit if none).
    virtual Kit activeKit() const = 0;

    /// Find a kit by id.
    virtual Kit kit(const QString &id) const = 0;

    // ── Mutation ──────────────────────────────────────────────────────────

    /// Set the active kit by id.
    virtual void setActiveKit(const QString &id) = 0;

    /// Add a user-defined kit.  Returns the assigned id.
    virtual QString addKit(const Kit &kit) = 0;

    /// Remove a kit by id (only user-defined kits can be removed).
    virtual bool removeKit(const QString &id) = 0;

    /// Update an existing kit.
    virtual bool updateKit(const Kit &kit) = 0;

    // ── Detection ─────────────────────────────────────────────────────────

    /// Run full auto-detection (async). Emits detectionFinished when done.
    virtual void detectKits() = 0;

signals:
    /// Detection completed. kits() is now populated.
    void detectionFinished();

    /// Active kit changed (by user selection or auto-detection).
    void activeKitChanged(const Kit &kit);

    /// Kit list changed (added, removed, or updated).
    void kitsChanged();
};

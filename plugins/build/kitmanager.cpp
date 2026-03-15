#include "kitmanager.h"
#include "toolchainmanager.h"

#include <QSettings>
#include <QUuid>
#include <QtConcurrent>

// ── Construction ──────────────────────────────────────────────────────────────

KitManager::KitManager(ToolchainManager *tcMgr, QObject *parent)
    : IKitManager(parent)
    , m_tcMgr(tcMgr)
{
    loadFromSettings();
}

// ── IKitManager — Query ──────────────────────────────────────────────────────

QList<Kit> KitManager::kits() const
{
    return m_kits;
}

Kit KitManager::activeKit() const
{
    if (m_activeKitId.isEmpty() && !m_kits.isEmpty())
        return m_kits.first();

    for (const auto &k : m_kits) {
        if (k.id == m_activeKitId)
            return k;
    }
    return {};
}

Kit KitManager::kit(const QString &id) const
{
    for (const auto &k : m_kits) {
        if (k.id == id)
            return k;
    }
    return {};
}

// ── IKitManager — Mutation ───────────────────────────────────────────────────

void KitManager::setActiveKit(const QString &id)
{
    if (m_activeKitId == id)
        return;

    m_activeKitId = id;
    saveToSettings();
    emit activeKitChanged(activeKit());
}

QString KitManager::addKit(const Kit &newKit)
{
    Kit k = newKit;
    if (k.id.isEmpty())
        k.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    k.autoDetected = false;

    m_kits.append(k);
    saveToSettings();
    emit kitsChanged();
    return k.id;
}

bool KitManager::removeKit(const QString &id)
{
    for (int i = 0; i < m_kits.size(); ++i) {
        if (m_kits[i].id == id && !m_kits[i].autoDetected) {
            m_kits.removeAt(i);
            if (m_activeKitId == id) {
                m_activeKitId.clear();
                emit activeKitChanged(activeKit());
            }
            saveToSettings();
            emit kitsChanged();
            return true;
        }
    }
    return false;
}

bool KitManager::updateKit(const Kit &updated)
{
    for (int i = 0; i < m_kits.size(); ++i) {
        if (m_kits[i].id == updated.id) {
            m_kits[i] = updated;
            saveToSettings();
            emit kitsChanged();
            if (m_activeKitId == updated.id)
                emit activeKitChanged(updated);
            return true;
        }
    }
    return false;
}

// ── IKitManager — Detection ─────────────────────────────────────────────────

void KitManager::detectKits()
{
    // When ToolchainManager finishes detection, assemble kits.
    connect(m_tcMgr, &ToolchainManager::detectionFinished,
            this, [this]() {
        assembleKitsFromToolchains();
        saveToSettings();
        emit detectionFinished();
        emit kitsChanged();

        // Auto-select first kit if none active
        if (m_activeKitId.isEmpty() && !m_kits.isEmpty()) {
            m_activeKitId = m_kits.first().id;
            emit activeKitChanged(activeKit());
        }
    }, Qt::SingleShotConnection);

    m_tcMgr->detectAll();
}

// ── Private — Kit Assembly ───────────────────────────────────────────────────

static Kit::CompilerType mapType(Toolchain::Type t)
{
    switch (t) {
    case Toolchain::Type::GCC:       return Kit::CompilerType::GCC;
    case Toolchain::Type::Clang:     return Kit::CompilerType::Clang;
    case Toolchain::Type::MSVC:      return Kit::CompilerType::MSVC;
    case Toolchain::Type::MinGW:     return Kit::CompilerType::MinGW;
    case Toolchain::Type::LlvmMinGW: return Kit::CompilerType::LlvmMinGW;
    default:                         return Kit::CompilerType::Unknown;
    }
}

void KitManager::assembleKitsFromToolchains()
{
    // Preserve user-defined kits
    QList<Kit> userKits;
    for (const auto &k : m_kits) {
        if (!k.autoDetected)
            userKits.append(k);
    }

    m_kits.clear();

    const auto toolchains = m_tcMgr->toolchains();
    const auto buildSystems = m_tcMgr->buildSystems();

    // Find CMake and preferred generator
    BuildSystemInfo cmake;
    BuildSystemInfo ninja;
    BuildSystemInfo make;

    for (const auto &bs : buildSystems) {
        if (bs.name.compare(QLatin1String("cmake"), Qt::CaseInsensitive) == 0)
            cmake = bs;
        else if (bs.name.compare(QLatin1String("ninja"), Qt::CaseInsensitive) == 0)
            ninja = bs;
        else if (bs.name.compare(QLatin1String("make"), Qt::CaseInsensitive) == 0)
            make = bs;
    }

    for (const auto &tc : toolchains) {
        if (!tc.isValid())
            continue;

        Kit k;
        k.id = tc.id; // use toolchain id as base (stable across sessions)
        k.autoDetected = true;
        k.compilerType = mapType(tc.type);
        k.cCompilerPath = tc.cCompiler.path;
        k.cxxCompilerPath = tc.cxxCompiler.path;
        k.compilerVersion = m_tcMgr->extractVersion(tc.cxxCompiler.version);
        k.targetTriple = tc.targetTriple;

        // Debugger
        k.debuggerPath = tc.debugger.path;
        k.debuggerName = tc.debugger.name;

        // CMake
        if (cmake.isValid()) {
            k.cmakePath = cmake.path;
            k.cmakeVersion = cmake.version;
        }

        // Generator — prefer Ninja, fall back to Make/Makefiles
        if (ninja.isValid()) {
            k.generator = QStringLiteral("Ninja");
            k.generatorPath = ninja.path;
        } else if (make.isValid()) {
#ifdef Q_OS_WIN
            k.generator = QStringLiteral("MinGW Makefiles");
#else
            k.generator = QStringLiteral("Unix Makefiles");
#endif
            k.generatorPath = make.path;
        } else if (tc.type == Toolchain::Type::MSVC) {
            k.generator = QStringLiteral("Visual Studio 17 2022");
        }

        // Display name
        QString compName;
        switch (tc.type) {
        case Toolchain::Type::GCC:       compName = QStringLiteral("GCC");           break;
        case Toolchain::Type::Clang:     compName = QStringLiteral("Clang");         break;
        case Toolchain::Type::MSVC:      compName = QStringLiteral("MSVC");          break;
        case Toolchain::Type::MinGW:     compName = QStringLiteral("GCC (MinGW)");   break;
        case Toolchain::Type::LlvmMinGW: compName = QStringLiteral("Clang (LLVM MinGW)"); break;
        default:                         compName = QStringLiteral("Unknown");        break;
        }

        k.displayName = Kit::buildDisplayName(
            compName, k.compilerVersion, k.generator, k.debuggerName);

        m_kits.append(k);
    }

    // Restore user-defined kits
    m_kits.append(userKits);
}

// ── Private — Persistence ────────────────────────────────────────────────────

void KitManager::loadFromSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("KitManager"));

    m_activeKitId = settings.value(QStringLiteral("activeKit")).toString();

    const int count = settings.beginReadArray(QStringLiteral("userKits"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        Kit k;
        k.id              = settings.value(QStringLiteral("id")).toString();
        k.displayName     = settings.value(QStringLiteral("displayName")).toString();
        k.autoDetected    = false;
        k.cCompilerPath   = settings.value(QStringLiteral("cCompilerPath")).toString();
        k.cxxCompilerPath = settings.value(QStringLiteral("cxxCompilerPath")).toString();
        k.compilerVersion = settings.value(QStringLiteral("compilerVersion")).toString();
        k.targetTriple    = settings.value(QStringLiteral("targetTriple")).toString();
        k.debuggerPath    = settings.value(QStringLiteral("debuggerPath")).toString();
        k.debuggerName    = settings.value(QStringLiteral("debuggerName")).toString();
        k.cmakePath       = settings.value(QStringLiteral("cmakePath")).toString();
        k.cmakeVersion    = settings.value(QStringLiteral("cmakeVersion")).toString();
        k.generator       = settings.value(QStringLiteral("generator")).toString();
        k.generatorPath   = settings.value(QStringLiteral("generatorPath")).toString();

        const int typeVal = settings.value(QStringLiteral("compilerType"), 5).toInt();
        k.compilerType = static_cast<Kit::CompilerType>(typeVal);

        if (!k.id.isEmpty())
            m_kits.append(k);
    }
    settings.endArray();

    settings.endGroup();
}

void KitManager::saveToSettings() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("KitManager"));

    settings.setValue(QStringLiteral("activeKit"), m_activeKitId);

    // Only persist user-defined kits (auto-detected are re-detected)
    int userIdx = 0;
    settings.beginWriteArray(QStringLiteral("userKits"));
    for (const auto &k : m_kits) {
        if (k.autoDetected)
            continue;
        settings.setArrayIndex(userIdx++);
        settings.setValue(QStringLiteral("id"), k.id);
        settings.setValue(QStringLiteral("displayName"), k.displayName);
        settings.setValue(QStringLiteral("compilerType"), static_cast<int>(k.compilerType));
        settings.setValue(QStringLiteral("cCompilerPath"), k.cCompilerPath);
        settings.setValue(QStringLiteral("cxxCompilerPath"), k.cxxCompilerPath);
        settings.setValue(QStringLiteral("compilerVersion"), k.compilerVersion);
        settings.setValue(QStringLiteral("targetTriple"), k.targetTriple);
        settings.setValue(QStringLiteral("debuggerPath"), k.debuggerPath);
        settings.setValue(QStringLiteral("debuggerName"), k.debuggerName);
        settings.setValue(QStringLiteral("cmakePath"), k.cmakePath);
        settings.setValue(QStringLiteral("cmakeVersion"), k.cmakeVersion);
        settings.setValue(QStringLiteral("generator"), k.generator);
        settings.setValue(QStringLiteral("generatorPath"), k.generatorPath);
    }
    settings.endArray();

    settings.endGroup();
}

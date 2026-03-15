#pragma once

#include "build/kit.h"

class ToolchainManager;
class QSettings;

/// Concrete kit manager — detects kits from ToolchainManager results,
/// persists user selections, and manages the active kit.
///
/// Detection workflow:
///   1. ToolchainManager::detectAll() discovers compilers, debuggers, build systems
///   2. KitManager::detectKits() calls ToolchainManager, then assembles Kit objects
///   3. Kits are written to QSettings for persistence
///   4. User selects active kit (per-workspace or global default)
class KitManager : public IKitManager
{
    Q_OBJECT

public:
    explicit KitManager(ToolchainManager *tcMgr, QObject *parent = nullptr);

    // ── IKitManager ───────────────────────────────────────────────────────
    QList<Kit> kits() const override;
    Kit activeKit() const override;
    Kit kit(const QString &id) const override;
    void setActiveKit(const QString &id) override;
    QString addKit(const Kit &kit) override;
    bool removeKit(const QString &id) override;
    bool updateKit(const Kit &kit) override;
    void detectKits() override;

private:
    void assembleKitsFromToolchains();
    void loadFromSettings();
    void saveToSettings() const;

    ToolchainManager *m_tcMgr;
    QList<Kit> m_kits;
    QString m_activeKitId;
};

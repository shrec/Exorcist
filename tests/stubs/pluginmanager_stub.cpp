// Minimal PluginManager stubs for test linking.
// ProfileManager calls setPluginDisabled / activateByEvent through a pointer
// guarded by if(m_pluginMgr). In tests m_pluginMgr is nullptr so these never
// execute, but the linker still needs the symbols.
//
// We include enough headers to make PluginManager's forward-declared unique_ptr
// members complete, so the destructor implicit instantiation succeeds.

#include <QLibrary>
#include "sdk/cabi/cabi_bridge.h"
#include "sdk/permissionguard.h"
#include "pluginmanager.h"

PluginManager::PluginManager(QObject *parent) : QObject(parent) {}
PluginManager::~PluginManager() = default;

void PluginManager::setPluginDisabled(const QString &, bool) {}
bool PluginManager::isPluginDisabled(const QString &) const { return false; }
int  PluginManager::activateByEvent(const QString &) { return 0; }

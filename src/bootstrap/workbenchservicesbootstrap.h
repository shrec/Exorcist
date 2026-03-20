#pragma once

#include <QObject>

class ServiceRegistry;
class ThemeManager;
class KeymapManager;
class FileWatchService;

/// Owns and registers the non-visual workbench services that were previously
/// held as raw members in MainWindow.
///
/// Services registered by this bootstrap:
///   "themeManager"  → ThemeManager*
///   "keymapManager" → KeymapManager*
///   "fileWatcher"   → FileWatchService*
class WorkbenchServicesBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit WorkbenchServicesBootstrap(QObject *parent = nullptr);

    void initialize(ServiceRegistry *services);

    ThemeManager     *themeManager()  const { return m_themeManager;  }
    KeymapManager    *keymapManager() const { return m_keymapManager; }
    FileWatchService *fileWatcher()   const { return m_fileWatcher;   }

private:
    ThemeManager     *m_themeManager  = nullptr;
    KeymapManager    *m_keymapManager = nullptr;
    FileWatchService *m_fileWatcher   = nullptr;
};

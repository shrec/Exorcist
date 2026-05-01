#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QStackedWidget;
class QPushButton;
class QToolButton;
class PluginManager;
class PluginMarketplaceService;

/// Panel for browsing installed and available plugins.
///
/// Shows two tabs: Installed (from PluginManager) and Available (from a
/// JSON registry file). Each plugin is rendered as a card with name,
/// version, author, description, and action buttons (Enable/Disable,
/// Open Folder, Reload, or Install).
class PluginGalleryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PluginGalleryPanel(QWidget *parent = nullptr);

    /// Set the plugin manager to populate the Installed tab.
    void setPluginManager(PluginManager *manager);

    /// Set the marketplace service used by the "Install from URL…" button.
    /// When set, an extra toolbar action becomes available that opens the
    /// InstallPluginUrlDialog and refreshes the gallery on success.
    void setMarketplaceService(PluginMarketplaceService *service);

    /// Load available plugins from a JSON registry file.
    /// The file is an array of objects with: id, name, version, author,
    /// description, homepage, downloadUrl.
    void loadRegistryFromFile(const QString &jsonPath);

    /// Refresh the installed plugins list.
    void refreshInstalled();

signals:
    /// Emitted when the user wants to install a plugin by download URL.
    void installRequested(const QString &pluginId, const QString &downloadUrl);

    /// Emitted when the user toggles a plugin enabled/disabled.
    void pluginToggled(const QString &pluginId, bool enabled);

    /// Emitted when the user requests a plugin reload (best-effort).
    void pluginReloadRequested(const QString &pluginId);

private slots:
    void onInstalledItemClicked(QListWidgetItem *item);
    void onAvailableItemClicked(QListWidgetItem *item);
    void onFilterTextChanged(const QString &text);
    void onInstallFromUrlClicked();

private:
    void setupUi();
    void populateInstalled();
    void populateAvailable();
    void updateEmptyStates();

    struct InstalledRow {
        QString id;
        QString name;
        QString version;
        QString author;
        QString description;
        QString filePath;   // .dll / .so or .lua path
        bool    isLua = false;
        bool    disabled = false;
    };

    struct RegistryEntry {
        QString id;
        QString name;
        QString version;
        QString author;
        QString description;
        QString homepage;
        QString downloadUrl;
    };

    /// Build the card widget displayed inside a QListWidgetItem for the
    /// Installed list.
    QWidget *buildInstalledCard(const InstalledRow &row);

    /// Build the card widget displayed inside a QListWidgetItem for the
    /// Available list.
    QWidget *buildAvailableCard(const RegistryEntry &entry, bool alreadyInstalled);

    PluginManager            *m_pluginManager     = nullptr;
    PluginMarketplaceService *m_marketplaceService = nullptr;
    QLineEdit                *m_filterEdit         = nullptr;
    QToolButton              *m_installUrlBtn      = nullptr;
    QListWidget              *m_installedList      = nullptr;
    QListWidget              *m_availableList      = nullptr;
    QLabel                   *m_installedEmpty     = nullptr;
    QLabel                   *m_availableEmpty     = nullptr;
    QStackedWidget           *m_detailStack        = nullptr;
    QLabel                   *m_detailName         = nullptr;
    QLabel                   *m_detailVersion      = nullptr;
    QLabel                   *m_detailAuthor       = nullptr;
    QLabel                   *m_detailDesc         = nullptr;
    QLabel                   *m_detailHomepage     = nullptr;
    QPushButton              *m_actionButton       = nullptr;

    /// Plugin IDs installed in the current session, used to render a
    /// "(just installed)" badge for ~60 seconds after install.
    QSet<QString>     m_recentlyInstalled;
    QHash<QString, qint64> m_recentlyInstalledAt;

    QList<RegistryEntry> m_registryEntries;
};

#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QStackedWidget;
class QPushButton;
class PluginManager;

/// Panel for browsing installed and available plugins.
///
/// Shows two tabs: Installed (from PluginManager) and Available (from a
/// JSON registry file). Each plugin card shows name, version, author,
/// description, and an enable/disable or install button.
class PluginGalleryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PluginGalleryPanel(QWidget *parent = nullptr);

    /// Set the plugin manager to populate the Installed tab.
    void setPluginManager(PluginManager *manager);

    /// Load available plugins from a JSON registry file.
    /// The file is an array of objects with: id, name, version, author,
    /// description, homepage, downloadUrl.
    void loadRegistryFromFile(const QString &jsonPath);

    /// Refresh the installed plugins list.
    void refreshInstalled();

signals:
    /// Emitted when the user wants to install a plugin by download URL.
    void installRequested(const QString &pluginId, const QString &downloadUrl);

private slots:
    void onInstalledItemClicked(QListWidgetItem *item);
    void onAvailableItemClicked(QListWidgetItem *item);
    void onFilterTextChanged(const QString &text);

private:
    void setupUi();
    void populateInstalled();
    void populateAvailable();
    QWidget *createDetailWidget(const QString &name, const QString &version,
                                const QString &author, const QString &description,
                                const QString &homepage, bool installed);

    PluginManager *m_pluginManager = nullptr;
    QLineEdit     *m_filterEdit    = nullptr;
    QListWidget   *m_installedList = nullptr;
    QListWidget   *m_availableList = nullptr;
    QStackedWidget *m_detailStack  = nullptr;
    QLabel        *m_detailName    = nullptr;
    QLabel        *m_detailVersion = nullptr;
    QLabel        *m_detailAuthor  = nullptr;
    QLabel        *m_detailDesc    = nullptr;
    QLabel        *m_detailHomepage = nullptr;
    QPushButton   *m_actionButton  = nullptr;

    struct RegistryEntry {
        QString id;
        QString name;
        QString version;
        QString author;
        QString description;
        QString homepage;
        QString downloadUrl;
    };
    QList<RegistryEntry> m_registryEntries;
};

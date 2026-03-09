#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class ThemeManager;

/// Panel for browsing, previewing and installing colour themes.
/// Shows installed themes (Dark, Light, any loaded custom) and available
/// themes from a JSON registry file.
class ThemeGalleryPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ThemeGalleryPanel(QWidget *parent = nullptr);

    void setThemeManager(ThemeManager *mgr);

    /// Load available themes from a JSON registry file.
    /// Format: [{ "name", "author", "version", "description", "file", "previewColors": {...} }, ...]
    void loadRegistryFromFile(const QString &path);

    void refreshInstalled();

signals:
    void themeApplied(const QString &themeId);

private slots:
    void onInstalledItemClicked(QListWidgetItem *item);
    void onAvailableItemClicked(QListWidgetItem *item);
    void onFilterTextChanged(const QString &text);

private:
    void setupUi();
    void populateInstalled();
    void populateAvailable();
    void showDetail(const QString &name, const QString &author,
                    const QString &version, const QString &description,
                    bool canApply);

    struct RegistryEntry {
        QString name;
        QString author;
        QString version;
        QString description;
        QString file;         // path or URL to theme JSON
    };

    ThemeManager         *m_themeManager = nullptr;
    QLineEdit            *m_filter       = nullptr;
    QListWidget          *m_installedList = nullptr;
    QListWidget          *m_availableList = nullptr;
    QLabel               *m_detailName   = nullptr;
    QLabel               *m_detailAuthor = nullptr;
    QLabel               *m_detailVersion = nullptr;
    QLabel               *m_detailDesc   = nullptr;
    QPushButton          *m_actionBtn    = nullptr;
    QWidget              *m_detailPanel  = nullptr;
    QVector<RegistryEntry> m_registry;
};

#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QProgressBar;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class PluginMarketplaceService;

/// Modal dialog that lets the user install a plugin from a Git URL or
/// .zip download URL. Shows live clone/extract/build progress in an
/// inline log area instead of an external console window.
///
/// Source-type detection:
///   - URLs ending in `.git` or hosted on github.com / gitlab.com etc.
///     default to "Git Clone".
///   - URLs ending in `.zip` (or containing `archive/refs/heads/...zip`)
///     default to "ZIP Download".
/// The user may override the auto-detected source type from the combo.
///
/// All work is delegated to PluginMarketplaceService::installFromUrl();
/// the dialog binds onto its installProgress / installLog /
/// installFinished signals to drive the UI.
class InstallPluginUrlDialog : public QDialog
{
    Q_OBJECT

public:
    enum class SourceType {
        AutoDetect = 0,
        GitClone   = 1,
        ZipDownload = 2,
    };

    explicit InstallPluginUrlDialog(PluginMarketplaceService *service,
                                    QWidget *parent = nullptr);
    ~InstallPluginUrlDialog() override;

    /// Last successfully installed plugin ID (empty on failure or cancel).
    QString installedPluginId() const { return m_installedId; }

private slots:
    void onUrlChanged(const QString &text);
    void onInstallClicked();
    void onCancelClicked();
    void onProgress(int percent, const QString &message);
    void onLog(const QString &line);
    void onFinished(bool success, const QString &pluginId,
                    const QString &errorMessage);

private:
    void setupUi();
    void applyDarkTheme();
    SourceType autoDetectSourceType(const QString &url) const;
    void setBusy(bool busy);
    void appendLog(const QString &line, const QString &color = QString());

    PluginMarketplaceService *m_service = nullptr;

    // Inputs
    QLineEdit      *m_urlEdit       = nullptr;
    QComboBox      *m_sourceTypeBox = nullptr;
    QLineEdit      *m_subdirEdit    = nullptr;
    QCheckBox      *m_buildAfter    = nullptr;
    QCheckBox      *m_activateAfter = nullptr;

    // Progress / output
    QProgressBar   *m_progress      = nullptr;
    QLabel         *m_statusLabel   = nullptr;
    QPlainTextEdit *m_logView       = nullptr;

    // Buttons
    QPushButton    *m_installBtn    = nullptr;
    QPushButton    *m_cancelBtn     = nullptr;
    QPushButton    *m_closeBtn      = nullptr;

    bool     m_busy         = false;
    bool     m_userOverrode = false;  // user manually changed the source type
    QString  m_installedId;
};

#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QFormLayout;

/// Kit Manager — manage Qt kits (pairings of Qt version + C++ toolchain).
///
/// A "kit" bundles together everything needed to build/run a project for a
/// specific Qt version + toolchain combo: qmake path, C++ compiler, CMake,
/// debugger, and (optionally) a sysroot. Kits are persisted to QSettings
/// under "kits/<id>/<field>" and one kit may be marked as the default.
///
/// The dialog auto-detects:
///   - Qt installations under `C:/Qt/<version>/<compiler>/bin/qmake.exe`
///   - C++ compilers via `where` (cl, clang, clang++, g++, gcc, mingw32-gcc)
///
/// Persistence-only: this dialog does not wire detected kits into the live
/// ToolchainManager. That is intentionally out of scope (TODO: bridge to
/// ToolchainManager / build system once the Kit ↔ Toolchain mapping is
/// finalized).
class KitManagerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KitManagerDialog(QWidget *parent = nullptr);
    ~KitManagerDialog() override;

    /// Lightweight in-memory representation of a kit. The dialog reads/writes
    /// these to QSettings and keeps a `QList<Kit>` as the working model while
    /// the dialog is open. No external code should rely on this struct yet.
    struct Kit {
        QString id;          ///< Stable identifier (used as QSettings group key).
        QString name;        ///< Human-readable name shown in the list.
        QString qtPath;      ///< Path to `qmake.exe` (or qmake binary).
        QString compiler;    ///< Path to C++ compiler.
        QString cmake;       ///< Path to cmake.
        QString debugger;    ///< Path to debugger (gdb/lldb/cdb).
        QString sysroot;     ///< Optional sysroot for cross-compilation.
        bool    isDefault = false;
    };

private slots:
    void onKitSelectionChanged();
    void onAddKit();
    void onRemoveKit();
    void onSetDefault();
    void onAutoDetect();
    void onSave();
    void onBrowseQt();
    void onBrowseCompiler();
    void onBrowseCMake();
    void onBrowseDebugger();
    void onBrowseSysroot();

private:
    void buildUi();
    void applyVsDarkTheme();
    void loadKitsFromSettings();
    void saveKitsToSettings();
    void refreshKitList();
    void populateForm(const Kit &kit);
    void commitFormToCurrentKit();
    void detectQtInstallations();
    void detectCompilers();
    QString makeUniqueKitId(const QString &base) const;

    QList<Kit>        m_kits;
    int               m_currentIndex = -1;
    bool              m_loadingForm  = false;
    QStringList       m_detectedQt;
    QStringList       m_detectedCompilers;

    // Left pane.
    QListWidget *m_kitList = nullptr;

    // Right pane (form fields).
    QLineEdit *m_nameEdit     = nullptr;
    QComboBox *m_qtCombo      = nullptr;
    QComboBox *m_compilerCombo = nullptr;
    QLineEdit *m_cmakeEdit    = nullptr;
    QLineEdit *m_debuggerEdit = nullptr;
    QLineEdit *m_sysrootEdit  = nullptr;
    QCheckBox *m_defaultCheck = nullptr;

    // Buttons.
    QPushButton *m_addBtn      = nullptr;
    QPushButton *m_removeBtn   = nullptr;
    QPushButton *m_defaultBtn  = nullptr;
    QPushButton *m_detectBtn   = nullptr;
    QPushButton *m_saveBtn     = nullptr;
    QPushButton *m_cancelBtn   = nullptr;

    QLabel *m_statusLabel = nullptr;
};

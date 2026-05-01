#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/// New Qt Class wizard dialog.
///
/// Generates a Q_OBJECT-enabled C++ class skeleton with optional QObject /
/// QWidget / QDialog / QMainWindow base, signals and slots. Optionally
/// generates a Qt Designer .ui form and patches the nearest CMakeLists.txt
/// target so the new files participate in the build.
///
/// Layout:
///   - Class name (e.g. "MyWidget")
///   - Base class combo (QObject, QWidget, QDialog, QMainWindow, QFrame,
///     QPushButton, QListView, "Custom..." → free-text base class name)
///   - Header guard prefix (auto-filled from class name)
///   - Output folder + browse button
///   - Checkboxes: include .ui file, add to CMakeLists.txt, open after create
///   - List of common signals/slots that may be toggled on/off
class NewQtClassWizard : public QDialog
{
    Q_OBJECT

public:
    explicit NewQtClassWizard(QWidget *parent = nullptr);

    /// Path to the generated header file (valid after accept()).
    QString headerPath() const { return m_headerPath; }

    /// Path to the generated source file (valid after accept()).
    QString sourcePath() const { return m_sourcePath; }

    /// Path to the generated .ui file (may be empty).
    QString uiPath() const { return m_uiPath; }

private slots:
    void onBrowseFolder();
    void onCreate();
    void onClassNameChanged(const QString &text);
    void onBaseClassChanged(int index);
    void updateCreateButton();

private:
    bool validate(QString *errorOut) const;

    /// Resolve the base class spelling (handles "Custom..." case).
    QString effectiveBaseClass() const;

    /// True for QWidget-derived bases (constructor takes QWidget*).
    static bool baseIsWidget(const QString &base);

    /// Build header text.
    QString buildHeader(const QString &className,
                        const QString &baseClass,
                        const QString &guardPrefix,
                        bool includeUi,
                        const QStringList &signals_,
                        const QStringList &slots_) const;

    /// Build source text.
    QString buildSource(const QString &className,
                        const QString &baseClass,
                        bool includeUi,
                        const QStringList &slots_) const;

    /// Build empty .ui XML stub.
    QString buildUiForm(const QString &className,
                        const QString &baseClass) const;

    /// Walk up directory tree looking for nearest CMakeLists.txt.
    static QString findNearestCMakeLists(const QString &startDir);

    /// Append generated filenames to nearest CMakeLists.txt's source list.
    /// Returns true on success.
    bool patchCMakeLists(const QString &cmakePath,
                         const QString &headerName,
                         const QString &sourceName,
                         const QString &uiName) const;

    QLineEdit   *m_classNameEdit  = nullptr;
    QComboBox   *m_baseCombo      = nullptr;
    QLineEdit   *m_customBaseEdit = nullptr;
    QLineEdit   *m_guardEdit      = nullptr;
    QLineEdit   *m_folderEdit     = nullptr;
    QPushButton *m_browseBtn      = nullptr;
    QCheckBox   *m_uiFileCheck    = nullptr;
    QCheckBox   *m_addCMakeCheck  = nullptr;
    QCheckBox   *m_openCheck      = nullptr;
    QListWidget *m_signalsList    = nullptr;
    QListWidget *m_slotsList      = nullptr;
    QPushButton *m_createBtn      = nullptr;
    QPushButton *m_cancelBtn      = nullptr;
    QLabel      *m_hintLabel      = nullptr;

    bool m_guardEditedManually = false;

    QString m_headerPath;
    QString m_sourcePath;
    QString m_uiPath;
};

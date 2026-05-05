#pragma once

#include <QDialog>
#include "sdk/kit.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QWidget;
class IKitManager;
class ProjectTemplateRegistry;

/// New Project wizard dialog.
///
/// Two-step flow:
///   1. Pick a language (left panel) → see templates (right panel)
///   2. Enter project name + location → Create
///
/// Templates are dynamically loaded from ProjectTemplateRegistry,
/// which collects providers from plugins + built-in templates.
///
/// When an IKitManager is provided, a Kit selector is shown for
/// C++/C projects (non-MSVC kits only).
class NewProjectWizard : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectWizard(ProjectTemplateRegistry *registry,
                              const QString &defaultLocation,
                              IKitManager *kitManager = nullptr,
                              QWidget *parent = nullptr);

    /// The created project path (valid after accept()).
    QString createdProjectPath() const;

    /// The selected language (valid after accept()).
    QString selectedLanguage() const;

    /// The selected template ID (valid after accept()).
    QString selectedTemplateId() const;

    /// The selected kit (valid after accept(); may be empty if no kit selected).
    Kit selectedKit() const;

private slots:
    void onLanguageSelected();
    void onTemplateSelected();
    void onBrowseLocation();
    void onCreateClicked();

private:
    void populateLanguages();
    void populateTemplates(const QString &language);
    void populateKits(const QString &language);
    void updateCreateButton();

    static bool languageUsesKit(const QString &language);

    ProjectTemplateRegistry *m_registry;
    IKitManager             *m_kitManager = nullptr;

    QListWidget *m_languageList = nullptr;
    QListWidget *m_templateList = nullptr;
    QLabel      *m_templateDesc = nullptr;
    QLineEdit   *m_nameEdit     = nullptr;
    QLineEdit   *m_locationEdit = nullptr;

    // Kit selector row (shown only for C++/C)
    QWidget     *m_kitRow       = nullptr;
    QComboBox   *m_kitCombo     = nullptr;

    QString m_createdPath;
    QString m_selectedLanguage;
    QString m_selectedTemplateId;
    Kit     m_selectedKit;
};

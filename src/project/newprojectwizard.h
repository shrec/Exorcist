#pragma once

#include <QDialog>

class QLineEdit;
class QListWidget;
class QLabel;
class ProjectTemplateRegistry;

/// New Project wizard dialog.
///
/// Two-step flow:
///   1. Pick a language (left panel) → see templates (right panel)
///   2. Enter project name + location → Create
///
/// Templates are dynamically loaded from ProjectTemplateRegistry,
/// which collects providers from plugins + built-in templates.
class NewProjectWizard : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectWizard(ProjectTemplateRegistry *registry,
                              const QString &defaultLocation,
                              QWidget *parent = nullptr);

    /// The created project path (valid after accept()).
    QString createdProjectPath() const;

    /// The selected language (valid after accept()).
    QString selectedLanguage() const;

    /// The selected template ID (valid after accept()).
    QString selectedTemplateId() const;

private slots:
    void onLanguageSelected();
    void onTemplateSelected();
    void onBrowseLocation();
    void onCreateClicked();

private:
    void populateLanguages();
    void populateTemplates(const QString &language);
    void updateCreateButton();

    ProjectTemplateRegistry *m_registry;

    QListWidget *m_languageList = nullptr;
    QListWidget *m_templateList = nullptr;
    QLabel      *m_templateDesc = nullptr;
    QLineEdit   *m_nameEdit     = nullptr;
    QLineEdit   *m_locationEdit = nullptr;

    QString m_createdPath;
    QString m_selectedLanguage;
    QString m_selectedTemplateId;
};

#include "newprojectwizard.h"
#include "projecttemplateregistry.h"
#include "iprojecttemplateprovider.h"

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

// ── NewProjectWizard ─────────────────────────────────────────────────────────

NewProjectWizard::NewProjectWizard(ProjectTemplateRegistry *registry,
                                   const QString &defaultLocation,
                                   QWidget *parent)
    : QDialog(parent)
    , m_registry(registry)
{
    setWindowTitle(tr("New Project"));
    setMinimumSize(700, 480);
    resize(780, 520);

    // ── Dark theme styling ────────────────────────────────────────────────
    setStyleSheet(QStringLiteral(
        "NewProjectWizard { background: #1e1e1e; }"
        "QLabel { color: #cccccc; }"
        "QListWidget { background: #252526; border: 1px solid #3e3e42; "
        "  color: #cccccc; font-size: 13px; }"
        "QListWidget::item { padding: 6px 10px; }"
        "QListWidget::item:selected { background: #094771; }"
        "QListWidget::item:hover { background: #2a2d2e; }"
        "QLineEdit { background: #3c3c3c; border: 1px solid #3e3e42; "
        "  color: #cccccc; padding: 6px; font-size: 13px; }"
        "QLineEdit:focus { border-color: #007acc; }"
        "QPushButton { background: #0e639c; color: white; border: none; "
        "  padding: 8px 20px; font-size: 13px; border-radius: 2px; }"
        "QPushButton:hover { background: #1177bb; }"
        "QPushButton:disabled { background: #3e3e42; color: #888; }"
        "QPushButton#browseBtn { background: #3c3c3c; color: #cccccc; "
        "  padding: 6px 12px; }"
        "QPushButton#browseBtn:hover { background: #505050; }"
        "QPushButton#cancelBtn { background: transparent; color: #cccccc; "
        "  border: 1px solid #3e3e42; }"
        "QPushButton#cancelBtn:hover { background: #2a2d2e; }"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // ── Title ─────────────────────────────────────────────────────────────
    auto *titleLabel = new QLabel(tr("Create a new project"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: 300; color: #cccccc; margin-bottom: 4px;"));
    mainLayout->addWidget(titleLabel);

    // ── Two-column: Languages | Templates ─────────────────────────────────
    auto *columnsLayout = new QHBoxLayout;
    columnsLayout->setSpacing(12);

    // Left: Language list
    auto *langCol = new QVBoxLayout;
    auto *langLabel = new QLabel(tr("Language"), this);
    langLabel->setStyleSheet(QStringLiteral(
        "font-size: 12px; font-weight: 600; color: #888; margin-bottom: 2px;"));
    langCol->addWidget(langLabel);

    m_languageList = new QListWidget(this);
    m_languageList->setFixedWidth(200);
    m_languageList->setIconSize(QSize(24, 24));
    langCol->addWidget(m_languageList);
    columnsLayout->addLayout(langCol);

    // Right: Template list + description
    auto *tmplCol = new QVBoxLayout;
    auto *tmplLabel = new QLabel(tr("Template"), this);
    tmplLabel->setStyleSheet(QStringLiteral(
        "font-size: 12px; font-weight: 600; color: #888; margin-bottom: 2px;"));
    tmplCol->addWidget(tmplLabel);

    m_templateList = new QListWidget(this);
    m_templateList->setIconSize(QSize(20, 20));
    tmplCol->addWidget(m_templateList, 1);

    m_templateDesc = new QLabel(this);
    m_templateDesc->setWordWrap(true);
    m_templateDesc->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #888; padding: 6px 0;"));
    tmplCol->addWidget(m_templateDesc);

    columnsLayout->addLayout(tmplCol, 1);
    mainLayout->addLayout(columnsLayout, 1);

    // ── Project name + location ───────────────────────────────────────────
    auto *formLayout = new QHBoxLayout;
    formLayout->setSpacing(8);

    auto *nameLabel = new QLabel(tr("Project name:"), this);
    nameLabel->setStyleSheet(QStringLiteral("font-size: 13px;"));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("MyProject"));

    auto *locLabel = new QLabel(tr("Location:"), this);
    locLabel->setStyleSheet(QStringLiteral("font-size: 13px;"));
    m_locationEdit = new QLineEdit(this);
    m_locationEdit->setText(defaultLocation.isEmpty()
                            ? QDir::homePath() : defaultLocation);

    auto *browseBtn = new QPushButton(tr("..."), this);
    browseBtn->setObjectName(QStringLiteral("browseBtn"));
    browseBtn->setFixedWidth(36);

    formLayout->addWidget(nameLabel);
    formLayout->addWidget(m_nameEdit, 1);
    formLayout->addSpacing(12);
    formLayout->addWidget(locLabel);
    formLayout->addWidget(m_locationEdit, 2);
    formLayout->addWidget(browseBtn);
    mainLayout->addLayout(formLayout);

    // ── Buttons ───────────────────────────────────────────────────────────
    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    auto *cancelBtn = new QPushButton(tr("Cancel"), this);
    cancelBtn->setObjectName(QStringLiteral("cancelBtn"));

    auto *createBtn = new QPushButton(tr("Create"), this);
    createBtn->setObjectName(QStringLiteral("createBtn"));
    createBtn->setEnabled(false);
    createBtn->setDefault(true);

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(createBtn);
    mainLayout->addLayout(btnLayout);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_languageList, &QListWidget::currentItemChanged,
            this, &NewProjectWizard::onLanguageSelected);
    connect(m_templateList, &QListWidget::currentItemChanged,
            this, &NewProjectWizard::onTemplateSelected);
    connect(m_nameEdit, &QLineEdit::textChanged,
            this, [this]() { updateCreateButton(); });
    connect(browseBtn, &QPushButton::clicked,
            this, &NewProjectWizard::onBrowseLocation);
    connect(cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);
    connect(createBtn, &QPushButton::clicked,
            this, &NewProjectWizard::onCreateClicked);

    populateLanguages();
}

QString NewProjectWizard::createdProjectPath() const
{
    return m_createdPath;
}

void NewProjectWizard::populateLanguages()
{
    m_languageList->clear();
    const QStringList langs = m_registry->languages();

    if (langs.isEmpty()) {
        m_languageList->addItem(tr("No templates available"));
        m_languageList->item(0)->setFlags(Qt::NoItemFlags);
        return;
    }

    for (const QString &lang : langs) {
        // Use the icon from the first template for this language
        const auto tmpls = m_registry->templatesForLanguage(lang);
        QIcon langIcon;
        for (const auto &t : tmpls) {
            if (!t.icon.isEmpty()) {
                langIcon = QIcon(t.icon);
                break;
            }
        }
        auto *item = new QListWidgetItem(langIcon, lang);
        m_languageList->addItem(item);
    }

    m_languageList->setCurrentRow(0);
}

void NewProjectWizard::populateTemplates(const QString &language)
{
    m_templateList->clear();
    m_templateDesc->clear();

    const auto templates = m_registry->templatesForLanguage(language);
    for (const auto &t : templates) {
        QIcon tmplIcon;
        if (!t.icon.isEmpty())
            tmplIcon = QIcon(t.icon);
        auto *item = new QListWidgetItem(tmplIcon, t.name);
        item->setData(Qt::UserRole, t.id);
        item->setData(Qt::UserRole + 1, t.description);
        m_templateList->addItem(item);
    }

    if (m_templateList->count() > 0)
        m_templateList->setCurrentRow(0);
}

void NewProjectWizard::onLanguageSelected()
{
    auto *item = m_languageList->currentItem();
    if (!item || !(item->flags() & Qt::ItemIsSelectable))
        return;
    populateTemplates(item->text());
    updateCreateButton();
}

void NewProjectWizard::onTemplateSelected()
{
    auto *item = m_templateList->currentItem();
    if (item) {
        m_templateDesc->setText(item->data(Qt::UserRole + 1).toString());
    } else {
        m_templateDesc->clear();
    }
    updateCreateButton();
}

void NewProjectWizard::onBrowseLocation()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Project Location"), m_locationEdit->text());
    if (!dir.isEmpty())
        m_locationEdit->setText(dir);
}

void NewProjectWizard::onCreateClicked()
{
    auto *tmplItem = m_templateList->currentItem();
    if (!tmplItem)
        return;

    const QString templateId = tmplItem->data(Qt::UserRole).toString();
    const QString name = m_nameEdit->text().trimmed();
    const QString location = m_locationEdit->text().trimmed();

    if (name.isEmpty() || location.isEmpty())
        return;

    const QString projectDir = QDir(location).filePath(name);

    QString error;
    if (!m_registry->createProject(templateId, name, projectDir, &error)) {
        QMessageBox::warning(this, tr("Error"), error);
        return;
    }

    m_createdPath = projectDir;
    accept();
}

void NewProjectWizard::updateCreateButton()
{
    auto *createBtn = findChild<QPushButton *>(QStringLiteral("createBtn"));
    if (!createBtn)
        return;

    const bool valid = m_templateList->currentItem()
                       && !m_nameEdit->text().trimmed().isEmpty()
                       && !m_locationEdit->text().trimmed().isEmpty();
    createBtn->setEnabled(valid);
}

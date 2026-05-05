#include "newprojectwizard.h"
#include "projecttemplateregistry.h"
#include "iprojecttemplateprovider.h"
#include "sdk/kit.h"

#include <QComboBox>
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

bool NewProjectWizard::languageUsesKit(const QString &language)
{
    return language == QLatin1String("C++")
        || language == QLatin1String("C")
        || language == QLatin1String("Qt")
        || language == QLatin1String("Zig");
}

// ── NewProjectWizard ─────────────────────────────────────────────────────────

NewProjectWizard::NewProjectWizard(ProjectTemplateRegistry *registry,
                                   const QString &defaultLocation,
                                   IKitManager *kitManager,
                                   QWidget *parent)
    : QDialog(parent)
    , m_registry(registry)
    , m_kitManager(kitManager)
{
    setWindowTitle(tr("New Project"));
    setMinimumSize(700, 520);
    resize(800, 580);

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
        "QComboBox { background: #3c3c3c; border: 1px solid #3e3e42; "
        "  color: #cccccc; padding: 5px 8px; font-size: 13px; min-height: 28px; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: #252526; color: #cccccc; "
        "  selection-background-color: #094771; border: 1px solid #3e3e42; }"
        "QPushButton { background: #0e639c; color: white; border: none; "
        "  padding: 8px 20px; font-size: 13px; border-radius: 2px; }"
        "QPushButton:hover { background: #1177bb; }"
        "QPushButton:disabled { background: #3e3e42; color: #888; }"
        "QPushButton#browseBtn { background: #3c3c3c; color: #cccccc; padding: 6px 12px; }"
        "QPushButton#browseBtn:hover { background: #505050; }"
        "QPushButton#cancelBtn { background: transparent; color: #cccccc; "
        "  border: 1px solid #3e3e42; }"
        "QPushButton#cancelBtn:hover { background: #2a2d2e; }"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto *titleLabel = new QLabel(tr("Create a new project"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: 300; color: #cccccc; margin-bottom: 4px;"));
    mainLayout->addWidget(titleLabel);

    // Two-column: Languages | Templates
    auto *columnsLayout = new QHBoxLayout;
    columnsLayout->setSpacing(12);

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

    // Kit selector row (C++/C/Zig only, no MSVC)
    m_kitRow = new QWidget(this);
    auto *kitLayout = new QHBoxLayout(m_kitRow);
    kitLayout->setContentsMargins(0, 0, 0, 0);
    kitLayout->setSpacing(8);
    auto *kitLabel = new QLabel(tr("Kit:"), m_kitRow);
    kitLabel->setStyleSheet(QStringLiteral("font-size: 13px; min-width: 80px;"));
    kitLayout->addWidget(kitLabel);
    m_kitCombo = new QComboBox(m_kitRow);
    m_kitCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    kitLayout->addWidget(m_kitCombo, 1);
    m_kitRow->setVisible(false);
    mainLayout->addWidget(m_kitRow);

    // Name + location
    auto *formLayout = new QHBoxLayout;
    formLayout->setSpacing(8);
    auto *nameLabel = new QLabel(tr("Project name:"), this);
    nameLabel->setStyleSheet(QStringLiteral("font-size: 13px;"));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("MyProject"));
    auto *locLabel = new QLabel(tr("Location:"), this);
    locLabel->setStyleSheet(QStringLiteral("font-size: 13px;"));
    m_locationEdit = new QLineEdit(this);
    m_locationEdit->setText(defaultLocation.isEmpty() ? QDir::homePath() : defaultLocation);
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

    // Buttons
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

    connect(m_languageList, &QListWidget::currentItemChanged,
            this, &NewProjectWizard::onLanguageSelected);
    connect(m_templateList, &QListWidget::currentItemChanged,
            this, &NewProjectWizard::onTemplateSelected);
    connect(m_nameEdit, &QLineEdit::textChanged,
            this, [this]() { updateCreateButton(); });
    connect(browseBtn, &QPushButton::clicked, this, &NewProjectWizard::onBrowseLocation);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(createBtn, &QPushButton::clicked, this, &NewProjectWizard::onCreateClicked);

    populateLanguages();
}

QString NewProjectWizard::createdProjectPath() const { return m_createdPath; }
QString NewProjectWizard::selectedLanguage()   const { return m_selectedLanguage; }
QString NewProjectWizard::selectedTemplateId() const { return m_selectedTemplateId; }
Kit     NewProjectWizard::selectedKit()        const { return m_selectedKit; }

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
        QIcon icon;
        for (const auto &t : m_registry->templatesForLanguage(lang))
            if (!t.icon.isEmpty()) { icon = QIcon(t.icon); break; }
        m_languageList->addItem(new QListWidgetItem(icon, lang));
    }
    m_languageList->setCurrentRow(0);
}

void NewProjectWizard::populateTemplates(const QString &language)
{
    m_templateList->clear();
    m_templateDesc->clear();
    for (const auto &t : m_registry->templatesForLanguage(language)) {
        QIcon icon;
        if (!t.icon.isEmpty()) icon = QIcon(t.icon);
        auto *item = new QListWidgetItem(icon, t.name);
        item->setData(Qt::UserRole,     t.id);
        item->setData(Qt::UserRole + 1, t.description);
        m_templateList->addItem(item);
    }
    if (m_templateList->count() > 0)
        m_templateList->setCurrentRow(0);
}

void NewProjectWizard::populateKits(const QString &language)
{
    const bool show = m_kitManager && languageUsesKit(language);
    m_kitRow->setVisible(show);
    m_kitCombo->clear();
    if (!show) return;

    bool any = false;
    for (const Kit &k : m_kitManager->kits()) {
        if (k.compilerType == Kit::CompilerType::MSVC) continue;
        if (!k.isValid()) continue;
        m_kitCombo->addItem(k.displayName, k.id);
        any = true;
    }

    if (!any) {
        m_kitCombo->addItem(tr("No kits detected — install GCC, Clang, or MinGW"), QString());
        m_kitCombo->setEnabled(false);
    } else {
        m_kitCombo->setEnabled(true);
        const Kit active = m_kitManager->activeKit();
        if (!active.id.isEmpty()) {
            const int idx = m_kitCombo->findData(active.id);
            if (idx >= 0) m_kitCombo->setCurrentIndex(idx);
        }
    }
}

void NewProjectWizard::onLanguageSelected()
{
    auto *item = m_languageList->currentItem();
    if (!item || !(item->flags() & Qt::ItemIsSelectable)) return;
    populateTemplates(item->text());
    populateKits(item->text());
    updateCreateButton();
}

void NewProjectWizard::onTemplateSelected()
{
    auto *item = m_templateList->currentItem();
    m_templateDesc->setText(item ? item->data(Qt::UserRole + 1).toString() : QString());
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
    if (!tmplItem) return;

    const QString templateId = tmplItem->data(Qt::UserRole).toString();
    const QString name       = m_nameEdit->text().trimmed();
    const QString location   = m_locationEdit->text().trimmed();
    if (name.isEmpty() || location.isEmpty()) return;

    const QString projectDir = QDir(location).filePath(name);
    QString error;
    if (!m_registry->createProject(templateId, name, projectDir, &error)) {
        QMessageBox::warning(this, tr("Error"), error);
        return;
    }

    m_createdPath        = projectDir;
    m_selectedTemplateId = templateId;
    if (auto *langItem = m_languageList->currentItem())
        m_selectedLanguage = langItem->text();

    // Capture selected kit
    m_selectedKit = Kit{};
    if (m_kitManager && m_kitRow->isVisible() && m_kitCombo->isEnabled()) {
        const QString kitId = m_kitCombo->currentData().toString();
        if (!kitId.isEmpty())
            m_selectedKit = m_kitManager->kit(kitId);
    }

    accept();
}

void NewProjectWizard::updateCreateButton()
{
    auto *btn = findChild<QPushButton *>(QStringLiteral("createBtn"));
    if (!btn) return;
    btn->setEnabled(m_templateList->currentItem()
                    && !m_nameEdit->text().trimmed().isEmpty()
                    && !m_locationEdit->text().trimmed().isEmpty());
}

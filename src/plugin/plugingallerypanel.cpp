#include "plugingallerypanel.h"
#include "../pluginmanager.h"

#include <QColor>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

// ── Styling ───────────────────────────────────────────────────────────────────

static const char *kPanelStyle = R"(
QListWidget {
    background: #1e1e1e;
    border: none;
    font-size: 13px;
}
QListWidget::item {
    padding: 8px 12px;
    border-bottom: 1px solid #2d2d2d;
    color: #cccccc;
}
QListWidget::item:selected {
    background: #094771;
    color: #ffffff;
}
QListWidget::item:hover {
    background: #2a2d2e;
}
QLineEdit {
    background: #3c3c3c;
    border: 1px solid #3c3c3c;
    border-radius: 4px;
    color: #cccccc;
    padding: 6px 10px;
    font-size: 13px;
}
QLineEdit:focus {
    border-color: #007acc;
}
QTabWidget::pane {
    border: none;
    background: #1e1e1e;
}
QTabBar::tab {
    background: #2d2d2d;
    color: #969696;
    padding: 8px 16px;
    border: none;
    border-bottom: 2px solid transparent;
}
QTabBar::tab:selected {
    color: #ffffff;
    border-bottom-color: #007acc;
}
QTabBar::tab:hover {
    color: #cccccc;
}
QPushButton {
    background: #0e639c;
    color: #ffffff;
    border: none;
    border-radius: 4px;
    padding: 6px 16px;
    font-size: 12px;
}
QPushButton:hover {
    background: #1177bb;
}
QPushButton:disabled {
    background: #3c3c3c;
    color: #666666;
}
)";

// ── Constructor ───────────────────────────────────────────────────────────────

PluginGalleryPanel::PluginGalleryPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void PluginGalleryPanel::setupUi()
{
    setStyleSheet(QString::fromLatin1(kPanelStyle));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Filter input
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Search plugins..."));
    m_filterEdit->setClearButtonEnabled(true);
    mainLayout->addWidget(m_filterEdit);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &PluginGalleryPanel::onFilterTextChanged);

    // Splitter: left list | right detail
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter, 1);

    // Left: tabs for Installed / Available
    auto *tabWidget = new QTabWidget(this);
    m_installedList = new QListWidget(this);
    m_availableList = new QListWidget(this);
    tabWidget->addTab(m_installedList, tr("Installed"));
    tabWidget->addTab(m_availableList, tr("Available"));
    splitter->addWidget(tabWidget);

    connect(m_installedList, &QListWidget::itemClicked,
            this, &PluginGalleryPanel::onInstalledItemClicked);
    connect(m_availableList, &QListWidget::itemClicked,
            this, &PluginGalleryPanel::onAvailableItemClicked);

    // Right: detail panel
    auto *detailContainer = new QWidget(this);
    auto *detailLayout = new QVBoxLayout(detailContainer);
    detailLayout->setContentsMargins(16, 16, 16, 16);
    detailLayout->setSpacing(8);

    m_detailName = new QLabel(this);
    m_detailName->setStyleSheet(
        QStringLiteral("font-size: 18px; font-weight: bold; color: #ffffff;"));
    detailLayout->addWidget(m_detailName);

    m_detailVersion = new QLabel(this);
    m_detailVersion->setStyleSheet(QStringLiteral("color: #969696; font-size: 12px;"));
    detailLayout->addWidget(m_detailVersion);

    m_detailAuthor = new QLabel(this);
    m_detailAuthor->setStyleSheet(QStringLiteral("color: #4ec9b0; font-size: 13px;"));
    detailLayout->addWidget(m_detailAuthor);

    m_detailDesc = new QLabel(this);
    m_detailDesc->setWordWrap(true);
    m_detailDesc->setStyleSheet(QStringLiteral("color: #cccccc; font-size: 13px;"));
    detailLayout->addWidget(m_detailDesc);

    m_detailHomepage = new QLabel(this);
    m_detailHomepage->setOpenExternalLinks(true);
    m_detailHomepage->setStyleSheet(QStringLiteral("color: #3794ff; font-size: 12px;"));
    detailLayout->addWidget(m_detailHomepage);

    m_actionButton = new QPushButton(this);
    m_actionButton->setVisible(false);
    detailLayout->addWidget(m_actionButton);

    detailLayout->addStretch();
    detailContainer->setStyleSheet(QStringLiteral("background: #252526;"));
    splitter->addWidget(detailContainer);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
}

// ── Data population ───────────────────────────────────────────────────────────

void PluginGalleryPanel::setPluginManager(PluginManager *manager)
{
    m_pluginManager = manager;
    populateInstalled();
}

void PluginGalleryPanel::refreshInstalled()
{
    populateInstalled();
}

void PluginGalleryPanel::populateInstalled()
{
    m_installedList->clear();
    if (!m_pluginManager) return;

    for (const auto &lp : m_pluginManager->loadedPlugins()) {
        const PluginInfo pi = lp.instance->info();
        const bool disabled = m_pluginManager->isPluginDisabled(pi.id);
        const QString label = disabled
            ? QStringLiteral("%1  v%2  (disabled)").arg(pi.name, pi.version)
            : QStringLiteral("%1  v%2").arg(pi.name, pi.version);
        auto *item = new QListWidgetItem(label, m_installedList);
        item->setData(Qt::UserRole, pi.id);
        item->setData(Qt::UserRole + 1, pi.name);
        item->setData(Qt::UserRole + 2, pi.version);
        item->setData(Qt::UserRole + 3, pi.author);
        item->setData(Qt::UserRole + 4, pi.description);
        item->setToolTip(pi.description);
        if (disabled)
            item->setForeground(QColor(0x66, 0x66, 0x66));
    }

    // Also list Lua script plugins
    for (const auto &lua : m_pluginManager->loadedLuaScripts()) {
        const QString label = QStringLiteral("%1  (Lua)").arg(lua.name);
        auto *item = new QListWidgetItem(label, m_installedList);
        item->setData(Qt::UserRole, lua.id);
        item->setData(Qt::UserRole + 1, lua.name);
        item->setData(Qt::UserRole + 2, lua.version);
        item->setData(Qt::UserRole + 3, lua.author);
        item->setData(Qt::UserRole + 4, lua.description);
        item->setToolTip(lua.description);
    }
}

void PluginGalleryPanel::loadRegistryFromFile(const QString &jsonPath)
{
    m_registryEntries.clear();

    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return;

    const QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        const QJsonObject obj = v.toObject();
        RegistryEntry e;
        e.id          = obj[QLatin1String("id")].toString();
        e.name        = obj[QLatin1String("name")].toString();
        e.version     = obj[QLatin1String("version")].toString();
        e.author      = obj[QLatin1String("author")].toString();
        e.description = obj[QLatin1String("description")].toString();
        e.homepage    = obj[QLatin1String("homepage")].toString();
        e.downloadUrl = obj[QLatin1String("downloadUrl")].toString();
        if (!e.id.isEmpty())
            m_registryEntries.append(e);
    }

    populateAvailable();
}

void PluginGalleryPanel::populateAvailable()
{
    m_availableList->clear();

    // Collect installed IDs for "already installed" check
    QSet<QString> installedIds;
    if (m_pluginManager) {
        for (const auto &lp : m_pluginManager->loadedPlugins())
            installedIds.insert(lp.instance->info().id);
    }

    for (const RegistryEntry &e : std::as_const(m_registryEntries)) {
        const bool alreadyInstalled = installedIds.contains(e.id);
        const QString label = alreadyInstalled
            ? QStringLiteral("%1  v%2  (installed)").arg(e.name, e.version)
            : QStringLiteral("%1  v%2").arg(e.name, e.version);
        auto *item = new QListWidgetItem(label, m_availableList);
        item->setData(Qt::UserRole, e.id);
        item->setData(Qt::UserRole + 1, e.name);
        item->setData(Qt::UserRole + 2, e.version);
        item->setData(Qt::UserRole + 3, e.author);
        item->setData(Qt::UserRole + 4, e.description);
        item->setData(Qt::UserRole + 5, e.homepage);
        item->setData(Qt::UserRole + 6, e.downloadUrl);
        item->setData(Qt::UserRole + 7, alreadyInstalled);
        item->setToolTip(e.description);
    }
}

// ── Item click handlers ───────────────────────────────────────────────────────

void PluginGalleryPanel::onInstalledItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    const QString pluginId = item->data(Qt::UserRole).toString();
    m_detailName->setText(item->data(Qt::UserRole + 1).toString());
    m_detailVersion->setText(tr("Version: %1").arg(item->data(Qt::UserRole + 2).toString()));
    m_detailAuthor->setText(tr("Author: %1").arg(item->data(Qt::UserRole + 3).toString()));
    m_detailDesc->setText(item->data(Qt::UserRole + 4).toString());
    m_detailHomepage->clear();

    const bool disabled = m_pluginManager && m_pluginManager->isPluginDisabled(pluginId);
    m_actionButton->setText(disabled ? tr("Enable") : tr("Disable"));
    m_actionButton->setEnabled(true);
    m_actionButton->setVisible(true);
    m_actionButton->disconnect();
    connect(m_actionButton, &QPushButton::clicked, this, [this, pluginId]() {
        if (!m_pluginManager) return;
        const bool wasDisabled = m_pluginManager->isPluginDisabled(pluginId);
        m_pluginManager->setPluginDisabled(pluginId, !wasDisabled);
        m_actionButton->setText(wasDisabled ? tr("Disable") : tr("Enable"));
        emit pluginToggled(pluginId, wasDisabled);
    });
}

void PluginGalleryPanel::onAvailableItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    const QString pluginId   = item->data(Qt::UserRole).toString();
    const bool    installed  = item->data(Qt::UserRole + 7).toBool();
    const QString homepage   = item->data(Qt::UserRole + 5).toString();
    const QString dlUrl      = item->data(Qt::UserRole + 6).toString();

    m_detailName->setText(item->data(Qt::UserRole + 1).toString());
    m_detailVersion->setText(tr("Version: %1").arg(item->data(Qt::UserRole + 2).toString()));
    m_detailAuthor->setText(tr("Author: %1").arg(item->data(Qt::UserRole + 3).toString()));
    m_detailDesc->setText(item->data(Qt::UserRole + 4).toString());

    if (!homepage.isEmpty()) {
        m_detailHomepage->setText(
            QStringLiteral("<a href=\"%1\">%1</a>").arg(homepage.toHtmlEscaped()));
    } else {
        m_detailHomepage->clear();
    }

    if (installed) {
        m_actionButton->setText(tr("Installed"));
        m_actionButton->setEnabled(false);
    } else {
        m_actionButton->setText(tr("Install"));
        m_actionButton->setEnabled(true);
        m_actionButton->disconnect();
        connect(m_actionButton, &QPushButton::clicked, this, [this, pluginId, dlUrl]() {
            emit installRequested(pluginId, dlUrl);
            m_actionButton->setText(tr("Installing..."));
            m_actionButton->setEnabled(false);
        });
    }
    m_actionButton->setVisible(true);
}

// ── Filter ────────────────────────────────────────────────────────────────────

void PluginGalleryPanel::onFilterTextChanged(const QString &text)
{
    const QString filter = text.trimmed().toLower();

    // Filter installed list
    for (int i = 0; i < m_installedList->count(); ++i) {
        auto *item = m_installedList->item(i);
        const QString name = item->data(Qt::UserRole + 1).toString().toLower();
        const QString desc = item->data(Qt::UserRole + 4).toString().toLower();
        item->setHidden(!filter.isEmpty() && !name.contains(filter) && !desc.contains(filter));
    }

    // Filter available list
    for (int i = 0; i < m_availableList->count(); ++i) {
        auto *item = m_availableList->item(i);
        const QString name = item->data(Qt::UserRole + 1).toString().toLower();
        const QString desc = item->data(Qt::UserRole + 4).toString().toLower();
        item->setHidden(!filter.isEmpty() && !name.contains(filter) && !desc.contains(filter));
    }
}

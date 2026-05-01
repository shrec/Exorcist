#include "plugingallerypanel.h"
#include "../pluginmanager.h"
#include "installpluginurldialog.h"
#include "pluginmarketplaceservice.h"

#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPluginLoader>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

// ── Styling ───────────────────────────────────────────────────────────────────

static const char *kPanelStyle = R"(
QListWidget {
    background: #1e1e1e;
    border: none;
    font-size: 13px;
    outline: 0;
}
QListWidget::item {
    padding: 0px;
    margin: 0px;
    border: none;
    background: transparent;
}
QListWidget::item:selected {
    background: transparent;
}
QListWidget::item:hover {
    background: transparent;
}
QLineEdit#PluginGallerySearch {
    background: #3c3c3c;
    border: 1px solid #3c3c3c;
    border-radius: 4px;
    color: #cccccc;
    padding: 6px 10px;
    font-size: 13px;
    margin: 6px 8px;
}
QLineEdit#PluginGallerySearch:focus {
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
QPushButton[pgClass="primary"] {
    background: #0e639c;
    color: #ffffff;
    border: none;
    border-radius: 3px;
    padding: 4px 12px;
    font-size: 12px;
    min-width: 60px;
}
QPushButton[pgClass="primary"]:hover {
    background: #1177bb;
}
QPushButton[pgClass="primary"]:disabled {
    background: #3c3c3c;
    color: #888888;
}
QPushButton[pgClass="secondary"] {
    background: #3a3d41;
    color: #cccccc;
    border: 1px solid #3a3d41;
    border-radius: 3px;
    padding: 4px 10px;
    font-size: 12px;
}
QPushButton[pgClass="secondary"]:hover {
    background: #45494e;
    color: #ffffff;
}
QFrame[pgClass="card"] {
    background: #252526;
    border: 1px solid #2d2d2d;
    border-radius: 4px;
}
QFrame[pgClass="card"]:hover {
    border-color: #007acc;
}
QFrame[pgClass="cardDisabled"] {
    background: #1e1e1e;
    border: 1px dashed #3a3d41;
    border-radius: 4px;
}
QLabel#PluginGalleryEmpty {
    color: #6a6a6a;
    font-size: 13px;
    padding: 24px;
}
QToolButton#PluginGalleryInstallUrl {
    background: #0e639c;
    color: #ffffff;
    border: none;
    border-radius: 3px;
    padding: 6px 12px;
    font-size: 12px;
    margin-right: 6px;
}
QToolButton#PluginGalleryInstallUrl:hover {
    background: #1177bb;
}
)";

namespace {

constexpr int kCardHeight = 84;

QIcon defaultPluginIcon()
{
    // Reuse a stock Qt icon as a generic puzzle-piece stand-in.
    if (auto *app = qApp)
        return app->style()->standardIcon(QStyle::SP_FileDialogDetailedView);
    return QIcon();
}

QString shortenPath(const QString &path)
{
    if (path.isEmpty())
        return path;
    QFileInfo fi(path);
    return fi.absolutePath();
}

} // namespace

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

    // Top toolbar row: search filter + Install-from-URL button
    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 8, 0);
    topRow->setSpacing(0);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setObjectName(QStringLiteral("PluginGallerySearch"));
    m_filterEdit->setPlaceholderText(tr("Search plugins..."));
    m_filterEdit->setClearButtonEnabled(true);
    topRow->addWidget(m_filterEdit, 1);

    m_installUrlBtn = new QToolButton(this);
    m_installUrlBtn->setObjectName(QStringLiteral("PluginGalleryInstallUrl"));
    m_installUrlBtn->setText(tr("Install from URL..."));
    m_installUrlBtn->setToolTip(
        tr("Install a plugin from a Git URL or .zip URL (Ctrl+Shift+U)"));
    m_installUrlBtn->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U));
    m_installUrlBtn->setCursor(Qt::PointingHandCursor);
    m_installUrlBtn->setEnabled(false);  // enabled by setMarketplaceService()
    topRow->addWidget(m_installUrlBtn);

    mainLayout->addLayout(topRow);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &PluginGalleryPanel::onFilterTextChanged);
    connect(m_installUrlBtn, &QToolButton::clicked,
            this, &PluginGalleryPanel::onInstallFromUrlClicked);

    // Splitter: left list | right detail
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter, 1);

    // Left: tabs for Installed / Available, each tab has its own list +
    // empty-state placeholder stacked together.
    auto *tabWidget = new QTabWidget(this);

    // ── Installed tab ─────────────────────────────────────────────────────
    auto *installedTab = new QWidget(tabWidget);
    auto *installedLayout = new QVBoxLayout(installedTab);
    installedLayout->setContentsMargins(0, 0, 0, 0);
    installedLayout->setSpacing(0);

    m_installedList = new QListWidget(installedTab);
    m_installedList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_installedList->setSpacing(4);
    m_installedList->setUniformItemSizes(false);
    m_installedList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    installedLayout->addWidget(m_installedList);

    m_installedEmpty = new QLabel(tr("No plugins installed."), installedTab);
    m_installedEmpty->setObjectName(QStringLiteral("PluginGalleryEmpty"));
    m_installedEmpty->setAlignment(Qt::AlignCenter);
    m_installedEmpty->setVisible(false);
    installedLayout->addWidget(m_installedEmpty);

    tabWidget->addTab(installedTab, tr("Installed"));

    // ── Available tab ─────────────────────────────────────────────────────
    auto *availableTab = new QWidget(tabWidget);
    auto *availableLayout = new QVBoxLayout(availableTab);
    availableLayout->setContentsMargins(0, 0, 0, 0);
    availableLayout->setSpacing(0);

    m_availableList = new QListWidget(availableTab);
    m_availableList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_availableList->setSpacing(4);
    m_availableList->setUniformItemSizes(false);
    m_availableList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    availableLayout->addWidget(m_availableList);

    m_availableEmpty = new QLabel(tr("No plugins available in registry."), availableTab);
    m_availableEmpty->setObjectName(QStringLiteral("PluginGalleryEmpty"));
    m_availableEmpty->setAlignment(Qt::AlignCenter);
    m_availableEmpty->setVisible(false);
    availableLayout->addWidget(m_availableEmpty);

    tabWidget->addTab(availableTab, tr("Available"));

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
    m_actionButton->setProperty("pgClass", QStringLiteral("primary"));
    m_actionButton->setVisible(false);
    detailLayout->addWidget(m_actionButton);

    detailLayout->addStretch();
    detailContainer->setStyleSheet(QStringLiteral("background: #252526;"));
    splitter->addWidget(detailContainer);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
}

// ── Data population ───────────────────────────────────────────────────────────

void PluginGalleryPanel::setPluginManager(PluginManager *manager)
{
    m_pluginManager = manager;
    populateInstalled();
}

void PluginGalleryPanel::setMarketplaceService(PluginMarketplaceService *service)
{
    m_marketplaceService = service;
    if (m_installUrlBtn)
        m_installUrlBtn->setEnabled(service != nullptr);

    if (service) {
        connect(service, &PluginMarketplaceService::installFinished, this,
                [this](bool ok, const QString &pluginId, const QString &) {
            if (!ok || pluginId.isEmpty())
                return;
            m_recentlyInstalled.insert(pluginId);
            m_recentlyInstalledAt.insert(
                pluginId, QDateTime::currentMSecsSinceEpoch());
            refreshInstalled();
            // Drop the badge after 60 seconds.
            QTimer::singleShot(60000, this, [this, pluginId]() {
                m_recentlyInstalled.remove(pluginId);
                m_recentlyInstalledAt.remove(pluginId);
                refreshInstalled();
            });
        }, Qt::UniqueConnection);
    }
}

void PluginGalleryPanel::onInstallFromUrlClicked()
{
    if (!m_marketplaceService) {
        // Defensive: button should be disabled in this case, but never
        // crash if a caller invokes the slot directly.
        return;
    }
    auto *dlg = new InstallPluginUrlDialog(m_marketplaceService, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
}

void PluginGalleryPanel::refreshInstalled()
{
    populateInstalled();
}

QWidget *PluginGalleryPanel::buildInstalledCard(const InstalledRow &row)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("PluginCard"));
    // Use a class-style selector via a property so QSS .PluginCard works.
    card->setProperty("pgClass", row.disabled
                      ? QStringLiteral("cardDisabled")
                      : QStringLiteral("card"));
    card->setFixedHeight(kCardHeight);
    card->setFrameShape(QFrame::StyledPanel);

    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    // Icon
    auto *iconLabel = new QLabel(card);
    iconLabel->setFixedSize(40, 40);
    iconLabel->setAlignment(Qt::AlignCenter);
    const QIcon icon = defaultPluginIcon();
    iconLabel->setPixmap(icon.pixmap(32, 32));
    if (row.disabled)
        iconLabel->setStyleSheet(QStringLiteral("opacity: 0.4;"));
    layout->addWidget(iconLabel);

    // Center text block
    auto *textCol = new QVBoxLayout;
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(6);

    QString nameText = row.name.isEmpty() ? row.id : row.name;
    if (row.isLua)
        nameText += QStringLiteral(" \xE2\x80\xA2 Lua"); // bullet + Lua tag
    if (row.disabled)
        nameText += tr("  (disabled)");
    if (m_recentlyInstalled.contains(row.id))
        nameText += tr("  (just installed)");

    auto *nameLabel = new QLabel(nameText, card);
    const QString nameColor = row.disabled ? QStringLiteral("#7a7a7a")
                                           : QStringLiteral("#ffffff");
    nameLabel->setStyleSheet(QStringLiteral(
        "font-size: 13px; font-weight: bold; color: %1;").arg(nameColor));
    titleRow->addWidget(nameLabel);

    if (!row.version.isEmpty()) {
        auto *verLabel = new QLabel(QStringLiteral("v%1").arg(row.version), card);
        verLabel->setStyleSheet(QStringLiteral(
            "color: #858585; font-size: 11px;"));
        titleRow->addWidget(verLabel);
    }
    titleRow->addStretch();
    textCol->addLayout(titleRow);

    if (!row.description.isEmpty()) {
        auto *descLabel = new QLabel(row.description, card);
        descLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 12px;")
            .arg(row.disabled ? QStringLiteral("#5a5a5a")
                              : QStringLiteral("#bbbbbb")));
        descLabel->setWordWrap(false);
        descLabel->setTextFormat(Qt::PlainText);
        descLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        textCol->addWidget(descLabel);
    }

    if (!row.author.isEmpty()) {
        auto *authorLabel = new QLabel(tr("by %1").arg(row.author), card);
        authorLabel->setStyleSheet(QStringLiteral(
            "color: #6a6a6a; font-size: 11px; font-style: italic;"));
        textCol->addWidget(authorLabel);
    }
    textCol->addStretch();
    layout->addLayout(textCol, 1);

    // Action buttons
    auto *btnCol = new QVBoxLayout;
    btnCol->setContentsMargins(0, 0, 0, 0);
    btnCol->setSpacing(4);
    btnCol->setAlignment(Qt::AlignTop);

    auto *toggleBtn = new QPushButton(card);
    toggleBtn->setProperty("pgClass", QStringLiteral("primary"));
    toggleBtn->setText(row.disabled ? tr("Enable") : tr("Disable"));
    toggleBtn->setCursor(Qt::PointingHandCursor);
    if (row.isLua) {
        // Lua scripts aren't toggleable through PluginManager::setPluginDisabled.
        toggleBtn->setEnabled(false);
        toggleBtn->setToolTip(tr("Lua scripts are managed from disk"));
    }
    const QString id = row.id;
    connect(toggleBtn, &QPushButton::clicked, this, [this, id, toggleBtn]() {
        if (!m_pluginManager) return;
        const bool wasDisabled = m_pluginManager->isPluginDisabled(id);
        m_pluginManager->setPluginDisabled(id, !wasDisabled);
        toggleBtn->setText(wasDisabled ? tr("Disable") : tr("Enable"));
        emit pluginToggled(id, wasDisabled);
        // Re-render so the card style reflects the new state.
        populateInstalled();
    });
    btnCol->addWidget(toggleBtn);

    auto *folderBtn = new QPushButton(tr("Open Folder"), card);
    folderBtn->setProperty("pgClass", QStringLiteral("secondary"));
    folderBtn->setCursor(Qt::PointingHandCursor);
    const QString filePath = row.filePath;
    if (filePath.isEmpty()) {
        folderBtn->setEnabled(false);
    } else {
        folderBtn->setToolTip(filePath);
    }
    connect(folderBtn, &QPushButton::clicked, this, [filePath]() {
        if (filePath.isEmpty()) return;
        const QString dir = shortenPath(filePath);
        if (!dir.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    btnCol->addWidget(folderBtn);

    layout->addLayout(btnCol);

    // Re-polish so :hover and class selectors apply.
    card->style()->unpolish(card);
    card->style()->polish(card);
    return card;
}

QWidget *PluginGalleryPanel::buildAvailableCard(const RegistryEntry &entry,
                                                bool alreadyInstalled)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("PluginCard"));
    card->setProperty("pgClass", QStringLiteral("card"));
    card->setFixedHeight(kCardHeight);
    card->setFrameShape(QFrame::StyledPanel);

    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    // Icon
    auto *iconLabel = new QLabel(card);
    iconLabel->setFixedSize(40, 40);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(defaultPluginIcon().pixmap(32, 32));
    layout->addWidget(iconLabel);

    // Center text block
    auto *textCol = new QVBoxLayout;
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(6);

    QString nameText = entry.name.isEmpty() ? entry.id : entry.name;
    auto *nameLabel = new QLabel(nameText, card);
    nameLabel->setStyleSheet(QStringLiteral(
        "font-size: 13px; font-weight: bold; color: #ffffff;"));
    titleRow->addWidget(nameLabel);

    if (!entry.version.isEmpty()) {
        auto *verLabel = new QLabel(QStringLiteral("v%1").arg(entry.version), card);
        verLabel->setStyleSheet(QStringLiteral(
            "color: #858585; font-size: 11px;"));
        titleRow->addWidget(verLabel);
    }
    titleRow->addStretch();
    textCol->addLayout(titleRow);

    if (!entry.description.isEmpty()) {
        auto *descLabel = new QLabel(entry.description, card);
        descLabel->setStyleSheet(QStringLiteral(
            "color: #bbbbbb; font-size: 12px;"));
        descLabel->setTextFormat(Qt::PlainText);
        descLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        textCol->addWidget(descLabel);
    }

    if (!entry.author.isEmpty()) {
        auto *authorLabel = new QLabel(tr("by %1").arg(entry.author), card);
        authorLabel->setStyleSheet(QStringLiteral(
            "color: #6a6a6a; font-size: 11px; font-style: italic;"));
        textCol->addWidget(authorLabel);
    }
    textCol->addStretch();
    layout->addLayout(textCol, 1);

    // Action button
    auto *btnCol = new QVBoxLayout;
    btnCol->setContentsMargins(0, 0, 0, 0);
    btnCol->setSpacing(4);
    btnCol->setAlignment(Qt::AlignTop);

    auto *primaryBtn = new QPushButton(card);
    primaryBtn->setProperty("pgClass", QStringLiteral("primary"));
    primaryBtn->setCursor(Qt::PointingHandCursor);
    if (alreadyInstalled) {
        primaryBtn->setText(tr("Installed"));
        primaryBtn->setEnabled(false);
    } else {
        primaryBtn->setText(tr("Install"));
        const QString id = entry.id;
        const QString url = entry.downloadUrl;
        connect(primaryBtn, &QPushButton::clicked, this,
                [this, id, url, primaryBtn]() {
            emit installRequested(id, url);
            primaryBtn->setText(tr("Installing..."));
            primaryBtn->setEnabled(false);
        });
    }
    btnCol->addWidget(primaryBtn);

    if (!entry.homepage.isEmpty()) {
        auto *homeBtn = new QPushButton(tr("Homepage"), card);
        homeBtn->setProperty("pgClass", QStringLiteral("secondary"));
        homeBtn->setCursor(Qt::PointingHandCursor);
        const QString url = entry.homepage;
        connect(homeBtn, &QPushButton::clicked, this, [url]() {
            if (!url.isEmpty())
                QDesktopServices::openUrl(QUrl(url));
        });
        btnCol->addWidget(homeBtn);
    }

    layout->addLayout(btnCol);

    card->style()->unpolish(card);
    card->style()->polish(card);
    return card;
}

void PluginGalleryPanel::populateInstalled()
{
    if (!m_installedList)
        return;
    m_installedList->clear();
    if (!m_pluginManager) {
        updateEmptyStates();
        return;
    }

    for (const auto &lp : m_pluginManager->loadedPlugins()) {
        if (!lp.instance)
            continue;
        const PluginInfo pi = lp.instance->info();
        InstalledRow row;
        row.id          = pi.id;
        row.name        = pi.name;
        row.version     = pi.version;
        row.author      = pi.author;
        row.description = pi.description;
        row.disabled    = m_pluginManager->isPluginDisabled(pi.id);
        row.isLua       = false;
        row.filePath    = lp.loader ? lp.loader->fileName() : QString();

        auto *item = new QListWidgetItem(m_installedList);
        item->setData(Qt::UserRole, row.id);
        item->setData(Qt::UserRole + 1, row.name);
        item->setData(Qt::UserRole + 2, row.version);
        item->setData(Qt::UserRole + 3, row.author);
        item->setData(Qt::UserRole + 4, row.description);
        item->setSizeHint(QSize(0, kCardHeight + 8));

        QWidget *card = buildInstalledCard(row);
        m_installedList->setItemWidget(item, card);
    }

    // Lua script plugins
    for (const auto &lua : m_pluginManager->loadedLuaScripts()) {
        InstalledRow row;
        row.id          = lua.id;
        row.name        = lua.name;
        row.version     = lua.version;
        row.author      = lua.author;
        row.description = lua.description;
        row.isLua       = true;
        row.disabled    = false;
        row.filePath    = lua.entryScript;

        auto *item = new QListWidgetItem(m_installedList);
        item->setData(Qt::UserRole, row.id);
        item->setData(Qt::UserRole + 1, row.name);
        item->setData(Qt::UserRole + 2, row.version);
        item->setData(Qt::UserRole + 3, row.author);
        item->setData(Qt::UserRole + 4, row.description);
        item->setSizeHint(QSize(0, kCardHeight + 8));

        QWidget *card = buildInstalledCard(row);
        m_installedList->setItemWidget(item, card);
    }

    updateEmptyStates();
    // Re-apply filter if user already typed something
    if (m_filterEdit && !m_filterEdit->text().isEmpty())
        onFilterTextChanged(m_filterEdit->text());
}

void PluginGalleryPanel::loadRegistryFromFile(const QString &jsonPath)
{
    m_registryEntries.clear();

    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        populateAvailable();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) {
        populateAvailable();
        return;
    }

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
    if (!m_availableList)
        return;
    m_availableList->clear();

    QSet<QString> installedIds;
    if (m_pluginManager) {
        for (const auto &lp : m_pluginManager->loadedPlugins()) {
            if (lp.instance)
                installedIds.insert(lp.instance->info().id);
        }
    }

    for (const RegistryEntry &e : std::as_const(m_registryEntries)) {
        const bool alreadyInstalled = installedIds.contains(e.id);

        auto *item = new QListWidgetItem(m_availableList);
        item->setData(Qt::UserRole, e.id);
        item->setData(Qt::UserRole + 1, e.name);
        item->setData(Qt::UserRole + 2, e.version);
        item->setData(Qt::UserRole + 3, e.author);
        item->setData(Qt::UserRole + 4, e.description);
        item->setData(Qt::UserRole + 5, e.homepage);
        item->setData(Qt::UserRole + 6, e.downloadUrl);
        item->setData(Qt::UserRole + 7, alreadyInstalled);
        item->setSizeHint(QSize(0, kCardHeight + 8));

        QWidget *card = buildAvailableCard(e, alreadyInstalled);
        m_availableList->setItemWidget(item, card);
    }

    updateEmptyStates();
    if (m_filterEdit && !m_filterEdit->text().isEmpty())
        onFilterTextChanged(m_filterEdit->text());
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
        populateInstalled();
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

    auto applyFilter = [&filter](QListWidget *list) {
        if (!list) return;
        for (int i = 0; i < list->count(); ++i) {
            auto *item = list->item(i);
            const QString name   = item->data(Qt::UserRole + 1).toString().toLower();
            const QString desc   = item->data(Qt::UserRole + 4).toString().toLower();
            const QString author = item->data(Qt::UserRole + 3).toString().toLower();
            const QString id     = item->data(Qt::UserRole).toString().toLower();
            const bool matches = filter.isEmpty()
                              || name.contains(filter)
                              || desc.contains(filter)
                              || author.contains(filter)
                              || id.contains(filter);
            item->setHidden(!matches);
        }
    };

    applyFilter(m_installedList);
    applyFilter(m_availableList);
    updateEmptyStates();
}

void PluginGalleryPanel::updateEmptyStates()
{
    auto isAllHidden = [](QListWidget *list) {
        if (!list) return true;
        if (list->count() == 0) return true;
        for (int i = 0; i < list->count(); ++i) {
            if (!list->item(i)->isHidden())
                return false;
        }
        return true;
    };

    if (m_installedEmpty && m_installedList) {
        const bool empty = m_installedList->count() == 0;
        const bool filteredOut = !empty && isAllHidden(m_installedList);
        m_installedEmpty->setVisible(empty || filteredOut);
        m_installedEmpty->setText(empty
            ? tr("No plugins installed.")
            : tr("No plugins match the current filter."));
    }
    if (m_availableEmpty && m_availableList) {
        const bool empty = m_availableList->count() == 0;
        const bool filteredOut = !empty && isAllHidden(m_availableList);
        m_availableEmpty->setVisible(empty || filteredOut);
        m_availableEmpty->setText(empty
            ? tr("No plugins available in registry.")
            : tr("No plugins match the current filter."));
    }
}

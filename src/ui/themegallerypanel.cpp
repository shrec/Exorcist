#include "themegallerypanel.h"

#include "../thememanager.h"

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
#include <QTabWidget>
#include <QVBoxLayout>

// ── Construction ────────────────────────────────────────────────────────────

ThemeGalleryPanel::ThemeGalleryPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void ThemeGalleryPanel::setThemeManager(ThemeManager *mgr)
{
    m_themeManager = mgr;
    populateInstalled();
}

// ── UI ─────────────────────────────────────────────────────────────────────

void ThemeGalleryPanel::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // Search filter
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Search themes..."));
    m_filter->setClearButtonEnabled(true);
    root->addWidget(m_filter);

    // Splitter: list on left, detail on right
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    root->addWidget(splitter, 1);

    // Tab widget with Installed / Available tabs
    auto *tabs = new QTabWidget(this);
    m_installedList = new QListWidget(this);
    m_availableList = new QListWidget(this);
    tabs->addTab(m_installedList, tr("Installed"));
    tabs->addTab(m_availableList, tr("Available"));
    splitter->addWidget(tabs);

    // Detail panel
    m_detailPanel = new QWidget(this);
    auto *detailLayout = new QVBoxLayout(m_detailPanel);

    m_detailName = new QLabel(this);
    m_detailName->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #dcdcdc;"));
    m_detailName->setWordWrap(true);

    m_detailVersion = new QLabel(this);
    m_detailVersion->setStyleSheet(QStringLiteral("color: #888;"));

    m_detailAuthor = new QLabel(this);
    m_detailAuthor->setStyleSheet(QStringLiteral("color: #4ec9b0;"));

    m_detailDesc = new QLabel(this);
    m_detailDesc->setWordWrap(true);
    m_detailDesc->setStyleSheet(QStringLiteral("color: #ccc;"));

    m_actionBtn = new QPushButton(this);
    m_actionBtn->setVisible(false);

    detailLayout->addWidget(m_detailName);
    detailLayout->addWidget(m_detailVersion);
    detailLayout->addWidget(m_detailAuthor);
    detailLayout->addSpacing(8);
    detailLayout->addWidget(m_detailDesc);
    detailLayout->addSpacing(12);
    detailLayout->addWidget(m_actionBtn);
    detailLayout->addStretch();

    m_detailPanel->setVisible(false);
    splitter->addWidget(m_detailPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);

    // Connections
    connect(m_installedList, &QListWidget::itemClicked,
            this, &ThemeGalleryPanel::onInstalledItemClicked);
    connect(m_availableList, &QListWidget::itemClicked,
            this, &ThemeGalleryPanel::onAvailableItemClicked);
    connect(m_filter, &QLineEdit::textChanged,
            this, &ThemeGalleryPanel::onFilterTextChanged);
}

// ── Installed themes ────────────────────────────────────────────────────────

void ThemeGalleryPanel::populateInstalled()
{
    m_installedList->clear();

    // Built-in Dark theme
    auto *darkItem = new QListWidgetItem(tr("Dark (built-in)"), m_installedList);
    darkItem->setData(Qt::UserRole, QStringLiteral("dark"));
    darkItem->setData(Qt::UserRole + 1, tr("Default dark theme"));

    // Built-in Light theme
    auto *lightItem = new QListWidgetItem(tr("Light (built-in)"), m_installedList);
    lightItem->setData(Qt::UserRole, QStringLiteral("light"));
    lightItem->setData(Qt::UserRole + 1, tr("Default light theme"));

    // Custom theme if loaded
    if (m_themeManager && !m_themeManager->themeName().isEmpty()) {
        auto *customItem = new QListWidgetItem(
            m_themeManager->themeName(), m_installedList);
        customItem->setData(Qt::UserRole, QStringLiteral("custom"));
        customItem->setData(Qt::UserRole + 1,
            tr("Custom theme by %1").arg(m_themeManager->themeAuthor()));
    }
}

void ThemeGalleryPanel::refreshInstalled()
{
    populateInstalled();
}

// ── Available themes registry ───────────────────────────────────────────────

void ThemeGalleryPanel::loadRegistryFromFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray())
        return;

    m_registry.clear();
    for (const auto &val : doc.array()) {
        const QJsonObject obj = val.toObject();
        RegistryEntry entry;
        entry.name        = obj.value(QLatin1String("name")).toString();
        entry.author      = obj.value(QLatin1String("author")).toString();
        entry.version     = obj.value(QLatin1String("version")).toString();
        entry.description = obj.value(QLatin1String("description")).toString();
        entry.file        = obj.value(QLatin1String("file")).toString();
        if (!entry.name.isEmpty())
            m_registry.append(entry);
    }
    populateAvailable();
}

void ThemeGalleryPanel::populateAvailable()
{
    m_availableList->clear();
    for (int i = 0; i < m_registry.size(); ++i) {
        const auto &e = m_registry[i];
        auto *item = new QListWidgetItem(e.name, m_availableList);
        item->setData(Qt::UserRole, i);
        item->setData(Qt::UserRole + 1, e.author);
        item->setData(Qt::UserRole + 2, e.version);
        item->setData(Qt::UserRole + 3, e.description);
        item->setData(Qt::UserRole + 4, e.file);
    }
}

// ── Detail panel ────────────────────────────────────────────────────────────

void ThemeGalleryPanel::showDetail(const QString &name, const QString &author,
                                   const QString &version, const QString &description,
                                   bool canApply)
{
    m_detailName->setText(name);
    m_detailVersion->setText(version.isEmpty() ? QString() : tr("v%1").arg(version));
    m_detailAuthor->setText(author.isEmpty() ? QString() : tr("by %1").arg(author));
    m_detailDesc->setText(description);
    m_actionBtn->setVisible(canApply);
    m_detailPanel->setVisible(true);
}

void ThemeGalleryPanel::onInstalledItemClicked(QListWidgetItem *item)
{
    const QString id = item->data(Qt::UserRole).toString();
    const QString desc = item->data(Qt::UserRole + 1).toString();
    showDetail(item->text(), {}, {}, desc, true);

    m_actionBtn->setText(tr("Apply"));
    m_actionBtn->setEnabled(true);
    m_actionBtn->disconnect();
    connect(m_actionBtn, &QPushButton::clicked, this, [this, id]() {
        if (!m_themeManager) return;
        if (id == QLatin1String("dark"))
            m_themeManager->setTheme(ThemeManager::Dark);
        else if (id == QLatin1String("light"))
            m_themeManager->setTheme(ThemeManager::Light);
        else
            m_themeManager->setTheme(ThemeManager::Custom);
        emit themeApplied(id);
    });
}

void ThemeGalleryPanel::onAvailableItemClicked(QListWidgetItem *item)
{
    const int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_registry.size()) return;

    const auto &e = m_registry[idx];
    showDetail(e.name, e.author, e.version, e.description, true);

    m_actionBtn->setText(tr("Install && Apply"));
    m_actionBtn->setEnabled(true);
    m_actionBtn->disconnect();
    connect(m_actionBtn, &QPushButton::clicked, this, [this, e]() {
        if (!m_themeManager) return;
        QString error;
        if (m_themeManager->loadCustomTheme(e.file, &error)) {
            m_themeManager->setTheme(ThemeManager::Custom);
            refreshInstalled();
            emit themeApplied(e.name);
            m_actionBtn->setText(tr("Applied"));
            m_actionBtn->setEnabled(false);
        }
    });
}

// ── Filter ──────────────────────────────────────────────────────────────────

void ThemeGalleryPanel::onFilterTextChanged(const QString &text)
{
    for (int i = 0; i < m_installedList->count(); ++i) {
        auto *item = m_installedList->item(i);
        const bool match = text.isEmpty()
            || item->text().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
    for (int i = 0; i < m_availableList->count(); ++i) {
        auto *item = m_availableList->item(i);
        const bool match = text.isEmpty()
            || item->text().contains(text, Qt::CaseInsensitive)
            || item->data(Qt::UserRole + 3).toString().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

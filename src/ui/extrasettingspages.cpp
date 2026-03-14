#include "extrasettingspages.h"
#include "../settings/languageprofile.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

// ═══════════════════════════════════════════════════════════════════════════════
// LanguageSettingsPage
// ═══════════════════════════════════════════════════════════════════════════════

LanguageSettingsPage::LanguageSettingsPage(LanguageProfileManager *mgr,
                                           QWidget *parent)
    : SettingsPage(parent)
    , m_mgr(mgr)
{
    auto *rootLayout = new QHBoxLayout(this);

    // ── Left: language list + buttons ─────────────────────────────────────
    auto *leftLayout = new QVBoxLayout();
    auto *listLabel  = new QLabel(tr("Languages:"), this);
    leftLayout->addWidget(listLabel);

    m_langList = new QListWidget(this);
    m_langList->setMaximumWidth(180);
    leftLayout->addWidget(m_langList);

    auto *btnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton(tr("Add..."), this);
    m_removeBtn = new QPushButton(tr("Remove"), this);
    m_removeBtn->setEnabled(false);
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_removeBtn);
    leftLayout->addLayout(btnLayout);

    rootLayout->addLayout(leftLayout);

    // ── Right: settings form ──────────────────────────────────────────────
    auto *rightGroup = new QGroupBox(tr("Language Settings"), this);
    auto *formLayout = new QFormLayout(rightGroup);

    m_enabled = new QCheckBox(tr("Profile enabled (activates language plugins)"), this);
    m_enabled->setToolTip(tr("When enabled, language-specific plugins for this language will load.\n"
                             "When disabled, plugins bound to this language stay dormant."));
    formLayout->addRow(QString(), m_enabled);

    formLayout->addRow(new QLabel(QString(), this)); // spacer

    m_tabSize = new QSpinBox(this);
    m_tabSize->setRange(1, 16);
    m_tabSize->setValue(4);
    m_tabSize->setToolTip(tr("Tab size for this language (overrides global)"));
    formLayout->addRow(tr("Tab Size:"), m_tabSize);

    m_useTabs = new QCheckBox(tr("Use tabs instead of spaces"), this);
    formLayout->addRow(QString(), m_useTabs);

    m_trimWhitespace = new QCheckBox(tr("Trim trailing whitespace on save"), this);
    formLayout->addRow(QString(), m_trimWhitespace);

    m_formatOnSave = new QCheckBox(tr("Format on save (via LSP)"), this);
    formLayout->addRow(QString(), m_formatOnSave);

    m_eolMode = new QComboBox(this);
    m_eolMode->addItem(tr("Default (platform)"), QString());
    m_eolMode->addItem(tr("LF (Unix/Mac)"),     QStringLiteral("lf"));
    m_eolMode->addItem(tr("CRLF (Windows)"),     QStringLiteral("crlf"));
    formLayout->addRow(tr("Line Ending:"), m_eolMode);

    rightGroup->setEnabled(false);
    rootLayout->addWidget(rightGroup, 1);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_langList, &QListWidget::currentRowChanged,
            this, &LanguageSettingsPage::onLanguageSelected);
    connect(m_addBtn, &QPushButton::clicked,
            this, &LanguageSettingsPage::addLanguage);
    connect(m_removeBtn, &QPushButton::clicked,
            this, &LanguageSettingsPage::removeLanguage);
}

void LanguageSettingsPage::load()
{
    m_langList->clear();
    const QStringList langs = m_mgr->configuredLanguages();
    for (const QString &lang : langs)
        m_langList->addItem(lang);
}

void LanguageSettingsPage::apply()
{
    syncFromWidgets();
    m_mgr->save();
}

void LanguageSettingsPage::onLanguageSelected(int row)
{
    syncFromWidgets(); // save previous

    if (row < 0) {
        m_currentLang.clear();
        m_removeBtn->setEnabled(false);
        if (auto *g = findChild<QGroupBox *>())
            g->setEnabled(false);
        return;
    }

    m_currentLang = m_langList->item(row)->text();
    m_removeBtn->setEnabled(true);
    if (auto *g = findChild<QGroupBox *>())
        g->setEnabled(true);

    const LanguageProfileData d = m_mgr->profile(m_currentLang);
    m_enabled->setChecked(d.enabled);
    m_tabSize->setValue(d.tabSize > 0 ? d.tabSize : 4);
    m_useTabs->setChecked(d.useTabs);
    m_trimWhitespace->setChecked(d.trimWhitespace);
    m_formatOnSave->setChecked(d.formatOnSave);

    const int idx = m_eolMode->findData(d.eolMode);
    m_eolMode->setCurrentIndex(idx >= 0 ? idx : 0);
}

void LanguageSettingsPage::addLanguage()
{
    static const QStringList suggestions = {
        QStringLiteral("cpp"), QStringLiteral("c"), QStringLiteral("python"),
        QStringLiteral("javascript"), QStringLiteral("typescript"),
        QStringLiteral("rust"), QStringLiteral("go"), QStringLiteral("java"),
        QStringLiteral("csharp"), QStringLiteral("ruby"), QStringLiteral("php"),
        QStringLiteral("lua"), QStringLiteral("json"), QStringLiteral("yaml"),
        QStringLiteral("html"), QStringLiteral("css"), QStringLiteral("sql"),
        QStringLiteral("markdown"), QStringLiteral("cmake"),
        QStringLiteral("dockerfile"), QStringLiteral("xml"),
        QStringLiteral("toml"), QStringLiteral("kotlin"), QStringLiteral("swift"),
        QStringLiteral("dart"), QStringLiteral("makefile")
    };

    bool ok = false;
    const QString lang = QInputDialog::getItem(
        this, tr("Add Language Profile"),
        tr("Language ID:"), suggestions, 0, true, &ok);

    if (!ok || lang.isEmpty()) return;
    if (m_mgr->configuredLanguages().contains(lang)) return;

    LanguageProfileData d;
    d.tabSize = 4;
    m_mgr->setProfile(lang, d);
    m_langList->addItem(lang);
    m_langList->setCurrentRow(m_langList->count() - 1);
}

void LanguageSettingsPage::removeLanguage()
{
    if (m_currentLang.isEmpty()) return;

    m_mgr->removeProfile(m_currentLang);
    const int row = m_langList->currentRow();
    if (row >= 0) {
        auto *item = m_langList->takeItem(row);
        delete item;
    }
    m_currentLang.clear();
}

void LanguageSettingsPage::syncFromWidgets()
{
    if (m_currentLang.isEmpty()) return;

    LanguageProfileData d;
    d.enabled        = m_enabled->isChecked();
    d.tabSize        = m_tabSize->value();
    d.useTabs        = m_useTabs->isChecked();
    d.useTabsSet     = true;
    d.trimWhitespace = m_trimWhitespace->isChecked();
    d.trimSet        = true;
    d.formatOnSave   = m_formatOnSave->isChecked();
    d.formatOnSaveSet = true;
    d.eolMode        = m_eolMode->currentData().toString();

    m_mgr->setProfile(m_currentLang, d);
}

// ═══════════════════════════════════════════════════════════════════════════════
// TerminalSettingsPage
// ═══════════════════════════════════════════════════════════════════════════════

TerminalSettingsPage::TerminalSettingsPage(QWidget *parent)
    : SettingsPage(parent)
{
    auto *layout = new QFormLayout(this);

    m_fontFamily = new QFontComboBox(this);
    m_fontFamily->setFontFilters(QFontComboBox::MonospacedFonts);
    layout->addRow(tr("Font:"), m_fontFamily);

    m_fontSize = new QSpinBox(this);
    m_fontSize->setRange(6, 48);
    layout->addRow(tr("Font Size:"), m_fontSize);

    m_scrollback = new QSpinBox(this);
    m_scrollback->setRange(500, 100000);
    m_scrollback->setSingleStep(1000);
    m_scrollback->setSuffix(tr(" lines"));
    layout->addRow(tr("Scrollback:"), m_scrollback);

    m_copyOnSelect = new QCheckBox(tr("Copy text on selection"), this);
    layout->addRow(QString(), m_copyOnSelect);
}

void TerminalSettingsPage::load()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(QStringLiteral("terminal"));
    m_fontFamily->setCurrentFont(QFont(
        s.value(QStringLiteral("fontFamily"), QStringLiteral("Consolas")).toString()));
    m_fontSize->setValue(s.value(QStringLiteral("fontSize"), 11).toInt());
    m_scrollback->setValue(s.value(QStringLiteral("scrollback"), 10000).toInt());
    m_copyOnSelect->setChecked(s.value(QStringLiteral("copyOnSelect"), false).toBool());
    s.endGroup();
}

void TerminalSettingsPage::apply()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(QStringLiteral("terminal"));
    s.setValue(QStringLiteral("fontFamily"), m_fontFamily->currentFont().family());
    s.setValue(QStringLiteral("fontSize"), m_fontSize->value());
    s.setValue(QStringLiteral("scrollback"), m_scrollback->value());
    s.setValue(QStringLiteral("copyOnSelect"), m_copyOnSelect->isChecked());
    s.endGroup();
}

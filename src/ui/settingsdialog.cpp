#include "settingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "../thememanager.h"

SettingsDialog::SettingsDialog(ThemeManager *themeMgr, QWidget *parent)
    : QDialog(parent)
    , m_themeMgr(themeMgr)
    , m_tabs(new QTabWidget(this))
{
    setWindowTitle(tr("Preferences"));
    setMinimumSize(480, 360);

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_tabs);

    auto *editorTab     = new QWidget;
    auto *appearanceTab = new QWidget;

    buildEditorTab(editorTab);
    buildAppearanceTab(appearanceTab);

    m_tabs->addTab(editorTab,     tr("Editor"));
    m_tabs->addTab(appearanceTab, tr("Appearance"));

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        applySettings();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadSettings();
}

void SettingsDialog::buildEditorTab(QWidget *tab)
{
    auto *lay = new QFormLayout(tab);

    // Font
    m_fontFamily = new QFontComboBox(tab);
    m_fontFamily->setFontFilters(QFontComboBox::MonospacedFonts);
    lay->addRow(tr("Font family:"), m_fontFamily);

    m_fontSize = new QSpinBox(tab);
    m_fontSize->setRange(6, 48);
    m_fontSize->setSuffix(tr(" pt"));
    lay->addRow(tr("Font size:"), m_fontSize);

    m_tabSize = new QSpinBox(tab);
    m_tabSize->setRange(1, 16);
    lay->addRow(tr("Tab size (spaces):"), m_tabSize);

    // Toggles
    m_wordWrap = new QCheckBox(tr("Enable word wrap"), tab);
    lay->addRow(m_wordWrap);

    m_showLineNumbers = new QCheckBox(tr("Show line numbers"), tab);
    lay->addRow(m_showLineNumbers);

    m_showMinimap = new QCheckBox(tr("Show minimap"), tab);
    lay->addRow(m_showMinimap);
}

void SettingsDialog::buildAppearanceTab(QWidget *tab)
{
    auto *lay = new QFormLayout(tab);

    m_themeCombo = new QComboBox(tab);
    m_themeCombo->addItem(tr("Dark"),  static_cast<int>(ThemeManager::Dark));
    m_themeCombo->addItem(tr("Light"), static_cast<int>(ThemeManager::Light));
    lay->addRow(tr("Theme:"), m_themeCombo);
}

void SettingsDialog::loadSettings()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(QStringLiteral("editor"));

    m_fontFamily->setCurrentFont(QFont(
        s.value(QStringLiteral("fontFamily"), QStringLiteral("Consolas")).toString()));
    m_fontSize->setValue(s.value(QStringLiteral("fontSize"), 11).toInt());
    m_tabSize->setValue(s.value(QStringLiteral("tabSize"), 4).toInt());
    m_wordWrap->setChecked(s.value(QStringLiteral("wordWrap"), false).toBool());
    m_showLineNumbers->setChecked(s.value(QStringLiteral("showLineNumbers"), true).toBool());
    m_showMinimap->setChecked(s.value(QStringLiteral("showMinimap"), false).toBool());

    s.endGroup();

    // Theme
    const int themeIdx = (m_themeMgr->currentTheme() == ThemeManager::Light) ? 1 : 0;
    m_themeCombo->setCurrentIndex(themeIdx);
}

void SettingsDialog::applySettings()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(QStringLiteral("editor"));

    s.setValue(QStringLiteral("fontFamily"),      m_fontFamily->currentFont().family());
    s.setValue(QStringLiteral("fontSize"),         m_fontSize->value());
    s.setValue(QStringLiteral("tabSize"),          m_tabSize->value());
    s.setValue(QStringLiteral("wordWrap"),         m_wordWrap->isChecked());
    s.setValue(QStringLiteral("showLineNumbers"),  m_showLineNumbers->isChecked());
    s.setValue(QStringLiteral("showMinimap"),       m_showMinimap->isChecked());

    s.endGroup();

    // Apply theme
    const auto newTheme = static_cast<ThemeManager::Theme>(
        m_themeCombo->currentData().toInt());
    m_themeMgr->setTheme(newTheme);

    emit settingsApplied();
}

QString SettingsDialog::fontFamily() const { return m_fontFamily->currentFont().family(); }
int     SettingsDialog::fontSize()   const { return m_fontSize->value(); }
int     SettingsDialog::tabSize()    const { return m_tabSize->value(); }
bool    SettingsDialog::wordWrap()   const { return m_wordWrap->isChecked(); }
bool    SettingsDialog::showMinimap()const { return m_showMinimap->isChecked(); }
bool    SettingsDialog::showLineNumbers() const { return m_showLineNumbers->isChecked(); }

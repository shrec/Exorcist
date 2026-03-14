#pragma once

#include "settingsdialog.h"

class QComboBox;
class QListWidget;
class QPushButton;
class QSpinBox;
class QCheckBox;

class LanguageProfileManager;

// ── LanguageSettingsPage ─────────────────────────────────────────────────────
// Settings page for per-language editor configuration: tab size, indent style,
// trim whitespace, format on save, EOL mode.

class LanguageSettingsPage : public SettingsPage
{
    Q_OBJECT
public:
    explicit LanguageSettingsPage(LanguageProfileManager *mgr,
                                 QWidget *parent = nullptr);
    QString title() const override { return tr("Languages"); }
    void load() override;
    void apply() override;

private:
    void onLanguageSelected(int row);
    void addLanguage();
    void removeLanguage();
    void syncFromWidgets();

    LanguageProfileManager *m_mgr = nullptr;
    QListWidget *m_langList       = nullptr;
    QSpinBox    *m_tabSize        = nullptr;
    QCheckBox   *m_useTabs        = nullptr;
    QCheckBox   *m_trimWhitespace = nullptr;
    QCheckBox   *m_formatOnSave   = nullptr;
    QComboBox   *m_eolMode        = nullptr;
    QPushButton *m_addBtn         = nullptr;
    QPushButton *m_removeBtn      = nullptr;

    QString m_currentLang;
};

// ── TerminalSettingsPage ─────────────────────────────────────────────────────
// Settings page for terminal emulator configuration.

class TerminalSettingsPage : public SettingsPage
{
    Q_OBJECT
public:
    explicit TerminalSettingsPage(QWidget *parent = nullptr);
    QString title() const override { return tr("Terminal"); }
    void load() override;
    void apply() override;

private:
    QFontComboBox *m_fontFamily   = nullptr;
    QSpinBox      *m_fontSize     = nullptr;
    QSpinBox      *m_scrollback   = nullptr;
    QCheckBox     *m_copyOnSelect = nullptr;
};

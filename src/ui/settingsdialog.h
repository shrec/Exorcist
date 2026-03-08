#pragma once

#include <QDialog>

class QComboBox;
class QFontComboBox;
class QSpinBox;
class QCheckBox;
class QTabWidget;
class ThemeManager;

// ── SettingsDialog ────────────────────────────────────────────────────────────
// IDE-wide preferences dialog: editor, appearance, keyboard shortcuts.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(ThemeManager *themeMgr, QWidget *parent = nullptr);

    // Current values
    QString fontFamily() const;
    int     fontSize() const;
    int     tabSize() const;
    bool    wordWrap() const;
    bool    showMinimap() const;
    bool    showLineNumbers() const;

signals:
    void settingsApplied();

private:
    void buildEditorTab(QWidget *tab);
    void buildAppearanceTab(QWidget *tab);
    void loadSettings();
    void applySettings();

    ThemeManager *m_themeMgr;
    QTabWidget   *m_tabs;

    // Editor settings
    QFontComboBox *m_fontFamily;
    QSpinBox      *m_fontSize;
    QSpinBox      *m_tabSize;
    QCheckBox     *m_wordWrap;
    QCheckBox     *m_showMinimap;
    QCheckBox     *m_showLineNumbers;

    // Appearance
    QComboBox     *m_themeCombo;
};

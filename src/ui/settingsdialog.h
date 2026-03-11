#pragma once

#include <QDialog>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;
class QStackedWidget;
class ThemeManager;

// ── SettingsPage ──────────────────────────────────────────────────────────────
// Abstract base for a single preferences section.
class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(QWidget *parent = nullptr) : QWidget(parent) {}

    virtual QString title() const = 0;          // sidebar label
    virtual void load() = 0;                    // read from QSettings
    virtual void apply() = 0;                   // write to QSettings
};

// ── EditorSettingsPage ────────────────────────────────────────────────────────
class EditorSettingsPage : public SettingsPage
{
    Q_OBJECT
public:
    explicit EditorSettingsPage(QWidget *parent = nullptr);
    QString title() const override { return tr("Editor"); }
    void load() override;
    void apply() override;

private:
    QFontComboBox *m_fontFamily = nullptr;
    QSpinBox      *m_fontSize   = nullptr;
    QSpinBox      *m_tabSize    = nullptr;
    QCheckBox     *m_wordWrap   = nullptr;
    QCheckBox     *m_showMinimap      = nullptr;
    QCheckBox     *m_showLineNumbers  = nullptr;
};

// ── AppearanceSettingsPage ────────────────────────────────────────────────────
class AppearanceSettingsPage : public SettingsPage
{
    Q_OBJECT
public:
    explicit AppearanceSettingsPage(ThemeManager *mgr, QWidget *parent = nullptr);
    QString title() const override { return tr("Appearance"); }
    void load() override;
    void apply() override;

private:
    ThemeManager *m_themeMgr = nullptr;
    QComboBox    *m_themeCombo = nullptr;
};

// ── IndexerSettingsPage ───────────────────────────────────────────────────────
class IndexerSettingsPage : public SettingsPage
{
    Q_OBJECT
public:
    explicit IndexerSettingsPage(QWidget *parent = nullptr);
    QString title() const override { return tr("Indexer"); }
    void load() override;
    void apply() override;

private:
    QPlainTextEdit *m_ignoreDirs  = nullptr;   // one per line
    QPlainTextEdit *m_ignoreGlobs = nullptr;   // one per line
    QPlainTextEdit *m_extensions  = nullptr;    // one per line
    QSpinBox       *m_maxFileSize = nullptr;    // KB
};

// ── SettingsDialog ────────────────────────────────────────────────────────────
// IDE-wide preferences dialog with sidebar navigation and pluggable pages.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(ThemeManager *themeMgr, QWidget *parent = nullptr);

    // Add a custom settings page (takes ownership).
    void addPage(SettingsPage *page);

signals:
    void settingsApplied();

private:
    void loadAll();
    void applyAll();

    QListWidget    *m_categoryList = nullptr;
    QStackedWidget *m_pageStack    = nullptr;
    QVector<SettingsPage *> m_pages;
};

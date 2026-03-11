#include "settingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "../thememanager.h"

// ── SettingsDialog ────────────────────────────────────────────────────────────

SettingsDialog::SettingsDialog(ThemeManager *themeMgr, QWidget *parent)
    : QDialog(parent)
    , m_categoryList(new QListWidget(this))
    , m_pageStack(new QStackedWidget(this))
{
    setWindowTitle(tr("Preferences"));
    setMinimumSize(640, 480);

    // Sidebar + content split
    m_categoryList->setFixedWidth(160);
    m_categoryList->setIconSize(QSize(0, 0));

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_categoryList);
    splitter->addWidget(m_pageStack);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    auto *root = new QVBoxLayout(this);
    root->addWidget(splitter, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        applyAll();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_categoryList, &QListWidget::currentRowChanged,
            m_pageStack, &QStackedWidget::setCurrentIndex);

    // Built-in pages
    addPage(new EditorSettingsPage(this));
    addPage(new AppearanceSettingsPage(themeMgr, this));
    addPage(new IndexerSettingsPage(this));

    m_categoryList->setCurrentRow(0);
    loadAll();
}

void SettingsDialog::addPage(SettingsPage *page)
{
    m_pages.append(page);
    m_categoryList->addItem(page->title());
    m_pageStack->addWidget(page);
}

void SettingsDialog::loadAll()
{
    for (auto *page : std::as_const(m_pages))
        page->load();
}

void SettingsDialog::applyAll()
{
    for (auto *page : std::as_const(m_pages))
        page->apply();
    emit settingsApplied();
}

// ── EditorSettingsPage ────────────────────────────────────────────────────────

EditorSettingsPage::EditorSettingsPage(QWidget *parent)
    : SettingsPage(parent)
{
    auto *lay = new QFormLayout(this);

    m_fontFamily = new QFontComboBox(this);
    m_fontFamily->setFontFilters(QFontComboBox::MonospacedFonts);
    lay->addRow(tr("Font family:"), m_fontFamily);

    m_fontSize = new QSpinBox(this);
    m_fontSize->setRange(6, 48);
    m_fontSize->setSuffix(tr(" pt"));
    lay->addRow(tr("Font size:"), m_fontSize);

    m_tabSize = new QSpinBox(this);
    m_tabSize->setRange(1, 16);
    lay->addRow(tr("Tab size (spaces):"), m_tabSize);

    m_wordWrap = new QCheckBox(tr("Enable word wrap"), this);
    lay->addRow(m_wordWrap);

    m_showLineNumbers = new QCheckBox(tr("Show line numbers"), this);
    lay->addRow(m_showLineNumbers);

    m_showMinimap = new QCheckBox(tr("Show minimap"), this);
    lay->addRow(m_showMinimap);
}

void EditorSettingsPage::load()
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
}

void EditorSettingsPage::apply()
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
}

// ── AppearanceSettingsPage ────────────────────────────────────────────────────

AppearanceSettingsPage::AppearanceSettingsPage(ThemeManager *mgr, QWidget *parent)
    : SettingsPage(parent)
    , m_themeMgr(mgr)
{
    auto *lay = new QFormLayout(this);

    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem(tr("Dark"),  static_cast<int>(ThemeManager::Dark));
    m_themeCombo->addItem(tr("Light"), static_cast<int>(ThemeManager::Light));
    lay->addRow(tr("Theme:"), m_themeCombo);
}

void AppearanceSettingsPage::load()
{
    const int themeIdx = (m_themeMgr->currentTheme() == ThemeManager::Light) ? 1 : 0;
    m_themeCombo->setCurrentIndex(themeIdx);
}

void AppearanceSettingsPage::apply()
{
    const auto newTheme = static_cast<ThemeManager::Theme>(
        m_themeCombo->currentData().toInt());
    m_themeMgr->setTheme(newTheme);
}

// ── IndexerSettingsPage ───────────────────────────────────────────────────────

static const QStringList kBuiltinIgnoreDirs = {
    QStringLiteral(".git"),        QStringLiteral(".cache"),
    QStringLiteral(".claude"),     QStringLiteral(".exorcist"),
    QStringLiteral(".next"),       QStringLiteral(".vs"),
    QStringLiteral(".vscode"),     QStringLiteral("__pycache__"),
    QStringLiteral("build"),       QStringLiteral("build-ci"),
    QStringLiteral("build-llvm"),  QStringLiteral("build-release"),
    QStringLiteral("CMakeFiles"),  QStringLiteral("dist"),
    QStringLiteral("node_modules"),QStringLiteral("out"),
    QStringLiteral("ReserchRepos"),QStringLiteral("vendor"),
};

static const QStringList kBuiltinExtensions = {
    // C/C++
    QStringLiteral("c"),    QStringLiteral("cpp"),  QStringLiteral("cc"),
    QStringLiteral("cxx"),  QStringLiteral("h"),    QStringLiteral("hpp"),
    QStringLiteral("hxx"),
    // C# / Java / Kotlin
    QStringLiteral("cs"),   QStringLiteral("java"), QStringLiteral("kt"),
    QStringLiteral("kts"),
    // Python
    QStringLiteral("py"),
    // JS/TS
    QStringLiteral("js"),   QStringLiteral("ts"),   QStringLiteral("jsx"),
    QStringLiteral("tsx"),  QStringLiteral("mjs"),  QStringLiteral("cjs"),
    // Rust / Go / Swift
    QStringLiteral("rs"),   QStringLiteral("go"),   QStringLiteral("swift"),
    // Ruby / PHP / Dart / Lua
    QStringLiteral("rb"),   QStringLiteral("php"),  QStringLiteral("dart"),
    QStringLiteral("lua"),
    // Shell
    QStringLiteral("sh"),   QStringLiteral("bash"), QStringLiteral("zsh"),
    QStringLiteral("ps1"),
    // Config / markup
    QStringLiteral("json"), QStringLiteral("yaml"), QStringLiteral("yml"),
    QStringLiteral("toml"), QStringLiteral("xml"),  QStringLiteral("html"),
    QStringLiteral("css"),  QStringLiteral("scss"), QStringLiteral("less"),
    QStringLiteral("sql"),  QStringLiteral("graphql"), QStringLiteral("proto"),
    // Build
    QStringLiteral("cmake"),QStringLiteral("pro"),  QStringLiteral("qbs"),
    // Docs
    QStringLiteral("md"),   QStringLiteral("txt"),  QStringLiteral("rst"),
};

IndexerSettingsPage::IndexerSettingsPage(QWidget *parent)
    : SettingsPage(parent)
{
    auto *root = new QVBoxLayout(this);

    // -- Ignored directories ---------------------------------------------------
    auto *dirGroup = new QGroupBox(tr("Ignored Directories"), this);
    auto *dirLay = new QVBoxLayout(dirGroup);
    dirLay->addWidget(new QLabel(tr("One directory name per line (e.g. node_modules):"), dirGroup));
    m_ignoreDirs = new QPlainTextEdit(dirGroup);
    m_ignoreDirs->setMaximumHeight(120);
    dirLay->addWidget(m_ignoreDirs);
    root->addWidget(dirGroup);

    // -- Ignore glob patterns --------------------------------------------------
    auto *globGroup = new QGroupBox(tr("Ignore Patterns (globs)"), this);
    auto *globLay = new QVBoxLayout(globGroup);
    globLay->addWidget(new QLabel(tr("One glob per line (e.g. *.log, build-*):"), globGroup));
    m_ignoreGlobs = new QPlainTextEdit(globGroup);
    m_ignoreGlobs->setMaximumHeight(80);
    globLay->addWidget(m_ignoreGlobs);
    root->addWidget(globGroup);

    // -- Indexable extensions --------------------------------------------------
    auto *extGroup = new QGroupBox(tr("Indexable File Extensions"), this);
    auto *extLay = new QVBoxLayout(extGroup);
    extLay->addWidget(new QLabel(tr("One extension per line without dot (e.g. cpp, py):"), extGroup));
    m_extensions = new QPlainTextEdit(extGroup);
    m_extensions->setMaximumHeight(120);
    extLay->addWidget(m_extensions);
    root->addWidget(extGroup);

    // -- Max file size ---------------------------------------------------------
    auto *sizeLay = new QFormLayout;
    m_maxFileSize = new QSpinBox(this);
    m_maxFileSize->setRange(32, 8192);
    m_maxFileSize->setSuffix(tr(" KB"));
    sizeLay->addRow(tr("Max file size:"), m_maxFileSize);
    root->addLayout(sizeLay);

    root->addStretch();
}

void IndexerSettingsPage::load()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(QStringLiteral("indexer"));

    // Ignored dirs — use builtin as default
    QStringList dirs = s.value(QStringLiteral("ignoreDirs"),
                               QStringList(kBuiltinIgnoreDirs)).toStringList();
    m_ignoreDirs->setPlainText(dirs.join(QLatin1Char('\n')));

    // Ignore globs
    QStringList globs = s.value(QStringLiteral("ignoreGlobs")).toStringList();
    m_ignoreGlobs->setPlainText(globs.join(QLatin1Char('\n')));

    // Extensions — use builtin as default
    QStringList exts = s.value(QStringLiteral("extensions"),
                                QStringList(kBuiltinExtensions)).toStringList();
    m_extensions->setPlainText(exts.join(QLatin1Char('\n')));

    // Max file size
    m_maxFileSize->setValue(s.value(QStringLiteral("maxFileSizeKB"), 512).toInt());

    s.endGroup();
}

void IndexerSettingsPage::apply()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(QStringLiteral("indexer"));

    // Parse lines — filter empty
    auto parseLines = [](QPlainTextEdit *edit) {
        QStringList result;
        const auto lines = edit->toPlainText().split(QLatin1Char('\n'));
        for (const auto &line : lines) {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty())
                result << trimmed;
        }
        return result;
    };

    s.setValue(QStringLiteral("ignoreDirs"),   parseLines(m_ignoreDirs));
    s.setValue(QStringLiteral("ignoreGlobs"),  parseLines(m_ignoreGlobs));
    s.setValue(QStringLiteral("extensions"),   parseLines(m_extensions));
    s.setValue(QStringLiteral("maxFileSizeKB"), m_maxFileSize->value());

    s.endGroup();
}

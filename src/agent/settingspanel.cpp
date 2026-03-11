#include "settingspanel.h"
#include "agent/chat/chatthemetokens.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
    , m_tabs(new QTabWidget(this))
{
    setStyleSheet(QStringLiteral(
        "SettingsPanel { background:%1; }"
        "QGroupBox { color:%2; border:1px solid %3; border-radius:4px;"
        "  margin-top:14px; padding-top:14px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }"
        "QCheckBox { color:%2; spacing:6px; }"
        "QLabel { color:%2; }"
        "QLineEdit { background:%4; color:%2; border:1px solid %3;"
        "  border-radius:3px; padding:3px 6px; }"
        "QLineEdit:focus { border-color:%5; }"
        "QSpinBox { background:%4; color:%2; border:1px solid %3;"
        "  border-radius:3px; padding:2px 6px; }"
        "QSpinBox:focus { border-color:%5; }"
        "QComboBox { background:%4; color:%2; border:1px solid %3;"
        "  border-radius:3px; padding:2px 6px; }"
        "QComboBox QAbstractItemView { background:%4; color:%2;"
        "  selection-background-color:%6; }"
        "QTabWidget::pane { border:1px solid %3; border-radius:4px; }"
        "QTabBar::tab { color:%7; padding:5px 12px; border:none;"
        "  border-bottom:2px solid transparent; }"
        "QTabBar::tab:selected { color:%2; border-bottom-color:%5; }"
        "QTabBar::tab:hover { color:%2; }")
        .arg(ChatTheme::pick(ChatTheme::PanelBg, ChatTheme::L_PanelBg),
             ChatTheme::pick(ChatTheme::FgPrimary, ChatTheme::L_FgPrimary),
             ChatTheme::pick(ChatTheme::Border, ChatTheme::L_Border),
             ChatTheme::pick(ChatTheme::InputBg, ChatTheme::L_InputBg),
             ChatTheme::AccentFg,
             ChatTheme::ListSelection,
             ChatTheme::pick(ChatTheme::FgDimmed, ChatTheme::L_FgDimmed)));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto *titleLabel = new QLabel(tr("<b>AI Settings</b>"), this);
    titleLabel->setStyleSheet(QStringLiteral("font-size:14px; padding:4px 0;"));
    root->addWidget(titleLabel);
    root->addWidget(m_tabs);

    auto *generalTab = new QWidget;
    auto *modelTab   = new QWidget;
    auto *toolsTab   = new QWidget;
    auto *contextTab = new QWidget;

    buildGeneralTab(generalTab);
    buildModelTab(modelTab);
    buildToolsTab(toolsTab);
    buildContextTab(contextTab);

    m_tabs->addTab(generalTab, tr("General"));
    m_tabs->addTab(modelTab,   tr("Model"));
    m_tabs->addTab(toolsTab,   tr("Tools"));
    m_tabs->addTab(contextTab, tr("Context"));

    loadSettings();
}

void SettingsPanel::buildGeneralTab(QWidget *tab)
{
    auto *lay = new QVBoxLayout(tab);

    auto *features = new QGroupBox(tr("Feature Toggles"), tab);
    auto *flay = new QVBoxLayout(features);

    m_enableInlineChat = new QCheckBox(tr("Enable inline chat (Ctrl+I)"), features);
    m_enableGhostText  = new QCheckBox(tr("Enable ghost text completions"), features);
    m_enableMemory     = new QCheckBox(tr("Enable memory system"), features);
    m_enableReview     = new QCheckBox(tr("Enable code review features"), features);

    flay->addWidget(m_enableInlineChat);
    flay->addWidget(m_enableGhostText);
    flay->addWidget(m_enableMemory);
    flay->addWidget(m_enableReview);
    lay->addWidget(features);

    // Language disable for completions
    auto *langGroup = new QGroupBox(tr("Completion Language Filter"), tab);
    auto *langLay = new QVBoxLayout(langGroup);
    auto *langLabel = new QLabel(tr("Disable completions for (comma-separated language IDs):"), langGroup);
    langLabel->setWordWrap(true);
    m_disabledLangs = new QLineEdit(langGroup);
    m_disabledLangs->setPlaceholderText(tr("e.g. markdown, plaintext, json"));
    langLay->addWidget(langLabel);
    langLay->addWidget(m_disabledLangs);
    lay->addWidget(langGroup);

    lay->addStretch();

    for (auto *cb : {m_enableInlineChat, m_enableGhostText, m_enableMemory, m_enableReview})
        connect(cb, &QCheckBox::toggled, this, [this]() { saveSettings(); emit settingsChanged(); });
    connect(m_disabledLangs, &QLineEdit::editingFinished, this, [this]() { saveSettings(); emit settingsChanged(); });
}

void SettingsPanel::buildModelTab(QWidget *tab)
{
    auto *lay = new QFormLayout(tab);

    m_maxSteps = new QSpinBox(tab);
    m_maxSteps->setRange(1, 100);
    m_maxSteps->setValue(20);
    lay->addRow(tr("Max tool steps per turn:"), m_maxSteps);

    m_maxTokens = new QSpinBox(tab);
    m_maxTokens->setRange(256, 200000);
    m_maxTokens->setSingleStep(1000);
    m_maxTokens->setValue(16384);
    lay->addRow(tr("Max response tokens:"), m_maxTokens);

    m_reasoningEffort = new QComboBox(tab);
    m_reasoningEffort->addItems({tr("Low"), tr("Medium"), tr("High")});
    m_reasoningEffort->setCurrentIndex(2);
    lay->addRow(tr("Reasoning effort:"), m_reasoningEffort);

    lay->addRow(new QLabel(tr("<b>Inline Completion</b>"), tab));

    m_completionModel = new QLineEdit(tab);
    m_completionModel->setPlaceholderText(tr("Leave empty to use the chat model"));
    lay->addRow(tr("Completion model:"), m_completionModel);

    lay->addRow(new QLabel(tr("<b>Custom Endpoint (BYOK)</b>"), tab));

    m_customEndpoint = new QLineEdit(tab);
    m_customEndpoint->setPlaceholderText(tr("https://api.openai.com/v1/chat/completions"));
    lay->addRow(tr("Endpoint URL:"), m_customEndpoint);

    m_customApiKey = new QLineEdit(tab);
    m_customApiKey->setEchoMode(QLineEdit::Password);
    m_customApiKey->setPlaceholderText(tr("sk-..."));
    lay->addRow(tr("API Key:"), m_customApiKey);

    lay->addRow(new QLabel(tr("<b>Web Search (SearXNG)</b>"), tab));

    m_searxngUrl = new QLineEdit(tab);
    m_searxngUrl->setPlaceholderText(tr("http://localhost:8080"));
    lay->addRow(tr("SearXNG URL:"), m_searxngUrl);

    connect(m_maxSteps, &QSpinBox::valueChanged, this, [this]() { saveSettings(); emit settingsChanged(); });
    connect(m_maxTokens, &QSpinBox::valueChanged, this, [this]() { saveSettings(); emit settingsChanged(); });
    connect(m_reasoningEffort, &QComboBox::currentIndexChanged, this, [this]() { saveSettings(); emit settingsChanged(); });
    connect(m_customEndpoint, &QLineEdit::editingFinished, this, [this]() { saveSettings(); emit settingsChanged(); });
    connect(m_customApiKey, &QLineEdit::editingFinished, this, [this]() { saveSettings(); emit settingsChanged(); });
    connect(m_completionModel, &QLineEdit::editingFinished, this, [this]() { saveSettings(); emit settingsChanged(); });
    connect(m_searxngUrl, &QLineEdit::editingFinished, this, [this]() { saveSettings(); emit settingsChanged(); });
}

void SettingsPanel::buildToolsTab(QWidget *tab)
{
    auto *lay = new QVBoxLayout(tab);
    lay->addWidget(new QLabel(
        tr("Enable or disable individual tools available to the agent.\n"
           "Disabled tools will not be offered to the AI model."), tab));

    m_toolsContainer = new QWidget(tab);
    m_toolsContainer->setLayout(new QVBoxLayout);
    lay->addWidget(m_toolsContainer);
    lay->addStretch();
}

void SettingsPanel::buildContextTab(QWidget *tab)
{
    auto *lay = new QVBoxLayout(tab);

    auto *group = new QGroupBox(tr("Auto-include in Context"), tab);
    auto *glay = new QVBoxLayout(group);

    m_includeOpenFiles   = new QCheckBox(tr("Open files list"), group);
    m_includeTerminal    = new QCheckBox(tr("Terminal output (last 50 lines)"), group);
    m_includeDiagnostics = new QCheckBox(tr("Build diagnostics / errors"), group);
    m_includeGitDiff     = new QCheckBox(tr("Git diff (staged + unstaged)"), group);

    glay->addWidget(m_includeOpenFiles);
    glay->addWidget(m_includeTerminal);
    glay->addWidget(m_includeDiagnostics);
    glay->addWidget(m_includeGitDiff);
    lay->addWidget(group);

    auto *tokenGroup = new QGroupBox(tr("Token Limits"), tab);
    auto *tlay = new QFormLayout(tokenGroup);
    m_contextTokenLimit = new QSpinBox(tokenGroup);
    m_contextTokenLimit->setRange(1000, 200000);
    m_contextTokenLimit->setSingleStep(1000);
    m_contextTokenLimit->setValue(100000);
    tlay->addRow(tr("Context token limit:"), m_contextTokenLimit);
    lay->addWidget(tokenGroup);

    lay->addStretch();

    for (auto *cb : {m_includeOpenFiles, m_includeTerminal, m_includeDiagnostics, m_includeGitDiff})
        connect(cb, &QCheckBox::toggled, this, [this]() { saveSettings(); emit settingsChanged(); });
    connect(m_contextTokenLimit, &QSpinBox::valueChanged, this, [this]() { saveSettings(); emit settingsChanged(); });
}

void SettingsPanel::loadSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("AI"));

    m_enableInlineChat->setChecked(s.value(QStringLiteral("enableInlineChat"), true).toBool());
    m_enableGhostText->setChecked(s.value(QStringLiteral("enableGhostText"), true).toBool());
    m_enableMemory->setChecked(s.value(QStringLiteral("enableMemory"), true).toBool());
    m_enableReview->setChecked(s.value(QStringLiteral("enableReview"), true).toBool());
    m_disabledLangs->setText(s.value(QStringLiteral("disabledCompletionLangs")).toString());

    m_maxSteps->setValue(s.value(QStringLiteral("maxSteps"), 20).toInt());
    m_maxTokens->setValue(s.value(QStringLiteral("maxTokens"), 16384).toInt());
    m_reasoningEffort->setCurrentIndex(s.value(QStringLiteral("reasoningEffort"), 2).toInt());
    m_customEndpoint->setText(s.value(QStringLiteral("customEndpoint")).toString());
    m_customApiKey->setText(s.value(QStringLiteral("customApiKey")).toString());
    m_completionModel->setText(s.value(QStringLiteral("completionModel")).toString());
    m_searxngUrl->setText(s.value(QStringLiteral("searxngUrl")).toString());

    m_includeOpenFiles->setChecked(s.value(QStringLiteral("includeOpenFiles"), true).toBool());
    m_includeTerminal->setChecked(s.value(QStringLiteral("includeTerminal"), true).toBool());
    m_includeDiagnostics->setChecked(s.value(QStringLiteral("includeDiagnostics"), true).toBool());
    m_includeGitDiff->setChecked(s.value(QStringLiteral("includeGitDiff"), true).toBool());
    m_contextTokenLimit->setValue(s.value(QStringLiteral("contextTokenLimit"), 100000).toInt());

    s.endGroup();
}

void SettingsPanel::saveSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("AI"));

    s.setValue(QStringLiteral("enableInlineChat"), m_enableInlineChat->isChecked());
    s.setValue(QStringLiteral("enableGhostText"), m_enableGhostText->isChecked());
    s.setValue(QStringLiteral("enableMemory"), m_enableMemory->isChecked());
    s.setValue(QStringLiteral("enableReview"), m_enableReview->isChecked());
    s.setValue(QStringLiteral("disabledCompletionLangs"), m_disabledLangs->text());

    s.setValue(QStringLiteral("maxSteps"), m_maxSteps->value());
    s.setValue(QStringLiteral("maxTokens"), m_maxTokens->value());
    s.setValue(QStringLiteral("reasoningEffort"), m_reasoningEffort->currentIndex());
    s.setValue(QStringLiteral("customEndpoint"), m_customEndpoint->text());
    s.setValue(QStringLiteral("customApiKey"), m_customApiKey->text());
    s.setValue(QStringLiteral("completionModel"), m_completionModel->text());
    s.setValue(QStringLiteral("searxngUrl"), m_searxngUrl->text());

    s.setValue(QStringLiteral("includeOpenFiles"), m_includeOpenFiles->isChecked());
    s.setValue(QStringLiteral("includeTerminal"), m_includeTerminal->isChecked());
    s.setValue(QStringLiteral("includeDiagnostics"), m_includeDiagnostics->isChecked());
    s.setValue(QStringLiteral("includeGitDiff"), m_includeGitDiff->isChecked());
    s.setValue(QStringLiteral("contextTokenLimit"), m_contextTokenLimit->value());
    s.setValue(QStringLiteral("disabledTools"), disabledTools());

    s.endGroup();
}

int SettingsPanel::contextTokenLimit() const
{
    return m_contextTokenLimit->value();
}

int SettingsPanel::maxSteps() const
{
    return m_maxSteps->value();
}

QString SettingsPanel::reasoningEffortString() const
{
    switch (m_reasoningEffort->currentIndex()) {
    case 0: return QStringLiteral("low");
    case 1: return QStringLiteral("medium");
    case 2: return QStringLiteral("high");
    default: return QStringLiteral("high");
    }
}

QStringList SettingsPanel::disabledCompletionLanguages() const
{
    const QString text = m_disabledLangs->text().trimmed();
    if (text.isEmpty()) return {};
    QStringList langs;
    for (const QString &s : text.split(QLatin1Char(',')))
        langs << s.trimmed().toLower();
    return langs;
}

QString SettingsPanel::customEndpoint() const
{
    return m_customEndpoint->text().trimmed();
}

QString SettingsPanel::customApiKey() const
{
    return m_customApiKey->text().trimmed();
}

QString SettingsPanel::completionModel() const
{
    return m_completionModel->text().trimmed();
}

void SettingsPanel::setToolNames(const QStringList &names)
{
    // Clear existing checkboxes
    qDeleteAll(m_toolChecks);
    m_toolChecks.clear();

    if (!m_toolsContainer) return;

    QSettings s;
    s.beginGroup(QStringLiteral("AI"));
    const QStringList disabled = s.value(QStringLiteral("disabledTools")).toStringList();
    s.endGroup();

    auto *lay = qobject_cast<QVBoxLayout *>(m_toolsContainer->layout());
    if (!lay) return;

    for (const QString &name : names) {
        auto *cb = new QCheckBox(name, m_toolsContainer);
        cb->setChecked(!disabled.contains(name));
        connect(cb, &QCheckBox::toggled, this, [this]() { saveSettings(); emit settingsChanged(); });
        lay->addWidget(cb);
        m_toolChecks[name] = cb;
    }
}

QStringList SettingsPanel::disabledTools() const
{
    QStringList disabled;
    for (auto it = m_toolChecks.begin(); it != m_toolChecks.end(); ++it) {
        if (!it.value()->isChecked())
            disabled << it.key();
    }
    return disabled;
}

#include "settingspanel.h"
#include "agent/chat/chatthemetokens.h"
#include "settings/scopedsettings.h"
#include "ui/thememanager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
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

    buildScopeBar();
    if (m_scopeBar)
        root->addWidget(m_scopeBar);

    root->addWidget(m_tabs);

    auto *generalTab    = new QWidget;
    auto *modelTab      = new QWidget;
    auto *toolsTab      = new QWidget;
    auto *contextTab    = new QWidget;
    auto *appearanceTab = new QWidget;

    buildGeneralTab(generalTab);
    buildModelTab(modelTab);
    buildToolsTab(toolsTab);
    buildContextTab(contextTab);
    buildAppearanceTab(appearanceTab);

    m_tabs->addTab(generalTab,    tr("General"));
    m_tabs->addTab(appearanceTab, tr("Appearance & Editor"));
    m_tabs->addTab(modelTab,      tr("Model"));
    m_tabs->addTab(toolsTab,      tr("Tools"));
    m_tabs->addTab(contextTab,    tr("Context"));

    loadSettings();

    // React to scope changes (workspace open/close, external file edits).
    connect(&ScopedSettings::instance(), &ScopedSettings::valueChanged, this,
            [this](const QString &) {
                refreshWorkspaceState();
                m_loading = true;
                loadSettings();
                m_loading = false;
                refreshAllBadges();
            });
    refreshWorkspaceState();
    refreshAllBadges();
}

// ── Scope bar ────────────────────────────────────────────────────────────────

void SettingsPanel::buildScopeBar()
{
    m_scopeBar = new QWidget(this);
    auto *lay = new QHBoxLayout(m_scopeBar);
    lay->setContentsMargins(2, 2, 2, 6);
    lay->setSpacing(8);

    auto *lbl = new QLabel(tr("Scope:"), m_scopeBar);
    m_scopeUser      = new QRadioButton(tr("User Settings"), m_scopeBar);
    m_scopeWorkspace = new QRadioButton(tr("Workspace Settings"), m_scopeBar);
    m_scopeUser->setChecked(true);
    m_scopeUser->setToolTip(tr("Edit values that persist globally for all workspaces."));
    m_scopeWorkspace->setToolTip(
        tr("Edit values that override user settings for this workspace only."));

    m_scopeStatus = new QLabel(m_scopeBar);
    m_scopeStatus->setStyleSheet(QStringLiteral("color:%1; font-style:italic;")
                                     .arg(ChatTheme::pick(ChatTheme::FgDimmed,
                                                           ChatTheme::L_FgDimmed)));

    lay->addWidget(lbl);
    lay->addWidget(m_scopeUser);
    lay->addWidget(m_scopeWorkspace);
    lay->addStretch();
    lay->addWidget(m_scopeStatus);

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    m_statusTimer->setInterval(3000);
    connect(m_statusTimer, &QTimer::timeout, this, [this]() {
        if (m_scopeStatus) m_scopeStatus->clear();
    });

    connect(m_scopeUser,      &QRadioButton::toggled, this,
            [this](bool on) { if (on) onScopeChanged(); });
    connect(m_scopeWorkspace, &QRadioButton::toggled, this,
            [this](bool on) { if (on) onScopeChanged(); });
}

void SettingsPanel::onScopeChanged()
{
    refreshAllBadges();
}

void SettingsPanel::refreshWorkspaceState()
{
    if (!m_scopeWorkspace) return;
    const bool ws = ScopedSettings::instance().hasWorkspace();
    m_scopeWorkspace->setEnabled(ws);
    if (!ws) {
        m_scopeWorkspace->setToolTip(
            tr("Open a workspace to enable workspace settings."));
        if (m_scopeWorkspace->isChecked() && m_scopeUser)
            m_scopeUser->setChecked(true);
    } else {
        m_scopeWorkspace->setToolTip(
            tr("Edit values that override user settings for this workspace only."));
    }
}

void SettingsPanel::showStatus(const QString &msg)
{
    if (!m_scopeStatus) return;
    m_scopeStatus->setText(msg);
    if (m_statusTimer) m_statusTimer->start();
}

// ── Tracked controls (per-key badge + reset) ─────────────────────────────────

void SettingsPanel::trackControl(const QString &key, QWidget *editor,
                                 EditorKind kind, QLabel *labelToBadge)
{
    Tracked t;
    t.key    = key;
    t.editor = editor;

    // Inject badge + reset button beside the label (when provided).
    if (labelToBadge) {
        // Wrap label text + badge + reset into a horizontal container.
        // We can't easily replace the widget in its parent layout, so we use
        // a child layout on the label itself when possible. Simpler: append
        // badge text by repurposing the label, and place a reset button via
        // a sibling widget. Here we adjust the label to host children.
        if (!labelToBadge->layout()) {
            auto *hl = new QHBoxLayout(labelToBadge);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(6);
            // Re-add label text via a child QLabel so we keep tr() semantics.
            const QString original = labelToBadge->text();
            labelToBadge->setText({});
            auto *txt = new QLabel(original, labelToBadge);
            hl->addWidget(txt);
            t.badge = new QLabel(labelToBadge);
            t.badge->setStyleSheet(QStringLiteral(
                "padding:1px 5px; border-radius:6px;"
                "font-size:9px; font-weight:bold;"));
            hl->addWidget(t.badge);
            t.reset = new QPushButton(QStringLiteral("↩"), labelToBadge);
            t.reset->setToolTip(tr("Reset this key to user-scope value"));
            t.reset->setFlat(true);
            t.reset->setCursor(Qt::PointingHandCursor);
            t.reset->setFixedSize(18, 18);
            t.reset->setStyleSheet(QStringLiteral(
                "QPushButton { color:%1; border:none; padding:0; }"
                "QPushButton:hover { color:%2; }")
                    .arg(ChatTheme::pick(ChatTheme::FgDimmed, ChatTheme::L_FgDimmed),
                         ChatTheme::AccentFg));
            t.reset->hide();
            hl->addWidget(t.reset);
            hl->addStretch();

            connect(t.reset.data(), &QPushButton::clicked, this, [this, key]() {
                ScopedSettings::instance().removeWorkspaceOverride(key);
                showStatus(tr("Workspace override removed"));
            });
        }
    }

    m_tracked.append(t);
    m_editorIdx.insert(editor, m_tracked.size() - 1);
    m_kindByKey.insert(key, kind);
}

void SettingsPanel::applyValueToEditor(QWidget *editor, EditorKind kind, const QVariant &v)
{
    if (!editor) return;
    QSignalBlocker block(editor);
    switch (kind) {
    case CheckBoxK: if (auto *cb = qobject_cast<QCheckBox*>(editor)) cb->setChecked(v.toBool()); break;
    case SpinBoxK:  if (auto *sb = qobject_cast<QSpinBox*>(editor))  sb->setValue(v.toInt()); break;
    case ComboBoxK: if (auto *cb = qobject_cast<QComboBox*>(editor)) cb->setCurrentIndex(v.toInt()); break;
    case LineEditK: if (auto *le = qobject_cast<QLineEdit*>(editor)) le->setText(v.toString()); break;
    }
}

QVariant SettingsPanel::editorValue(QWidget *editor, EditorKind kind) const
{
    if (!editor) return {};
    switch (kind) {
    case CheckBoxK: if (auto *cb = qobject_cast<QCheckBox*>(editor)) return cb->isChecked(); break;
    case SpinBoxK:  if (auto *sb = qobject_cast<QSpinBox*>(editor))  return sb->value(); break;
    case ComboBoxK: if (auto *cb = qobject_cast<QComboBox*>(editor)) return cb->currentIndex(); break;
    case LineEditK: if (auto *le = qobject_cast<QLineEdit*>(editor)) return le->text(); break;
    }
    return {};
}

void SettingsPanel::writeFromEditor(const QString &key, QWidget *editor, EditorKind kind)
{
    if (m_loading) return;
    const QVariant v = editorValue(editor, kind);
    const auto scope = (m_scopeWorkspace && m_scopeWorkspace->isChecked()
                        && ScopedSettings::instance().hasWorkspace())
                       ? ScopedSettings::Workspace
                       : ScopedSettings::User;
    ScopedSettings::instance().setValue(key, v, scope);
    // Also keep legacy QSettings in sync for the user scope so existing readers see it.
    if (scope == ScopedSettings::User) {
        QSettings s;
        s.setValue(key, v);
    }
    showStatus(scope == ScopedSettings::Workspace
               ? tr("Saved to workspace settings")
               : tr("Saved to user settings"));
    refreshAllBadges();
}

void SettingsPanel::updateBadge(const Tracked &t)
{
    if (!t.badge) return;
    const bool overridden = ScopedSettings::instance().hasOverride(t.key);
    if (overridden) {
        t.badge->setText(tr("WORKSPACE"));
        t.badge->setStyleSheet(QStringLiteral(
            "padding:1px 5px; border-radius:6px; font-size:9px; font-weight:bold;"
            "color:white; background:%1;").arg(ChatTheme::AccentFg));
    } else {
        t.badge->setText(tr("USER"));
        t.badge->setStyleSheet(QStringLiteral(
            "padding:1px 5px; border-radius:6px; font-size:9px; font-weight:bold;"
            "color:%1; background:%2;")
                .arg(ChatTheme::pick(ChatTheme::FgDimmed, ChatTheme::L_FgDimmed),
                     ChatTheme::pick(ChatTheme::Border,   ChatTheme::L_Border)));
    }
    if (t.reset)
        t.reset->setVisible(overridden);

    // Italic blue when overridden — visual feedback per UX rules.
    if (t.editor) {
        const QString styleKey = QStringLiteral("__overrideStyled");
        if (overridden) {
            t.editor->setStyleSheet(
                t.editor->property(styleKey.toUtf8().constData()).toString()
                + QStringLiteral("font-style:italic; color:%1;").arg(ChatTheme::AccentFg));
        } else {
            // Restore: simplest is to clear inline style; surrounding QSS still applies.
            t.editor->setStyleSheet({});
        }
    }
}

void SettingsPanel::refreshAllBadges()
{
    for (const auto &t : m_tracked)
        updateBadge(t);
}

QWidget *SettingsPanel::wrapCheckBoxWithBadge(const QString &key, QCheckBox *cb)
{
    auto *wrap = new QWidget(cb->parentWidget());
    auto *hl   = new QHBoxLayout(wrap);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);

    // Re-parent checkbox into wrapper.
    cb->setParent(wrap);
    hl->addWidget(cb);

    auto *badge = new QLabel(wrap);
    badge->setStyleSheet(QStringLiteral(
        "padding:1px 5px; border-radius:6px; font-size:9px; font-weight:bold;"));
    hl->addWidget(badge);

    auto *reset = new QPushButton(QStringLiteral("↩"), wrap);
    reset->setToolTip(tr("Reset this key to user-scope value"));
    reset->setFlat(true);
    reset->setCursor(Qt::PointingHandCursor);
    reset->setFixedSize(18, 18);
    reset->setStyleSheet(QStringLiteral(
        "QPushButton { color:%1; border:none; padding:0; }"
        "QPushButton:hover { color:%2; }")
            .arg(ChatTheme::pick(ChatTheme::FgDimmed, ChatTheme::L_FgDimmed),
                 ChatTheme::AccentFg));
    reset->hide();
    hl->addWidget(reset);
    hl->addStretch();

    connect(reset, &QPushButton::clicked, this, [this, key]() {
        ScopedSettings::instance().removeWorkspaceOverride(key);
        showStatus(tr("Workspace override removed"));
    });

    Tracked t;
    t.key    = key;
    t.editor = cb;
    t.badge  = badge;
    t.reset  = reset;
    m_tracked.append(t);
    m_editorIdx.insert(cb, m_tracked.size() - 1);
    m_kindByKey.insert(key, CheckBoxK);
    return wrap;
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

    flay->addWidget(wrapCheckBoxWithBadge(QStringLiteral("AI/enableInlineChat"), m_enableInlineChat));
    flay->addWidget(wrapCheckBoxWithBadge(QStringLiteral("AI/enableGhostText"),  m_enableGhostText));
    flay->addWidget(wrapCheckBoxWithBadge(QStringLiteral("AI/enableMemory"),     m_enableMemory));
    flay->addWidget(wrapCheckBoxWithBadge(QStringLiteral("AI/enableReview"),     m_enableReview));
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

    auto wireCheck = [this](QCheckBox *cb, const QString &key) {
        connect(cb, &QCheckBox::toggled, this, [this, cb, key]() {
            writeFromEditor(key, cb, CheckBoxK);
            emit settingsChanged();
        });
    };
    wireCheck(m_enableInlineChat, QStringLiteral("AI/enableInlineChat"));
    wireCheck(m_enableGhostText,  QStringLiteral("AI/enableGhostText"));
    wireCheck(m_enableMemory,     QStringLiteral("AI/enableMemory"));
    wireCheck(m_enableReview,     QStringLiteral("AI/enableReview"));
    connect(m_disabledLangs, &QLineEdit::editingFinished, this, [this]() {
        writeFromEditor(QStringLiteral("AI/disabledCompletionLangs"), m_disabledLangs, LineEditK);
        emit settingsChanged();
    });
    // m_disabledLangs lives below langLabel — track it without label badge for now.
    m_kindByKey.insert(QStringLiteral("AI/disabledCompletionLangs"), LineEditK);
    Tracked tle; tle.key = QStringLiteral("AI/disabledCompletionLangs"); tle.editor = m_disabledLangs;
    m_tracked.append(tle);
    m_editorIdx.insert(m_disabledLangs, m_tracked.size() - 1);
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

    auto labelOf = [lay](QWidget *w) {
        return qobject_cast<QLabel*>(lay->labelForField(w));
    };
    trackControl(QStringLiteral("AI/maxSteps"),         m_maxSteps,         SpinBoxK,  labelOf(m_maxSteps));
    trackControl(QStringLiteral("AI/maxTokens"),        m_maxTokens,        SpinBoxK,  labelOf(m_maxTokens));
    trackControl(QStringLiteral("AI/reasoningEffort"),  m_reasoningEffort,  ComboBoxK, labelOf(m_reasoningEffort));
    trackControl(QStringLiteral("AI/completionModel"),  m_completionModel,  LineEditK, labelOf(m_completionModel));
    trackControl(QStringLiteral("AI/customEndpoint"),   m_customEndpoint,   LineEditK, labelOf(m_customEndpoint));
    trackControl(QStringLiteral("AI/customApiKey"),     m_customApiKey,     LineEditK, labelOf(m_customApiKey));
    trackControl(QStringLiteral("AI/searxngUrl"),       m_searxngUrl,       LineEditK, labelOf(m_searxngUrl));

    connect(m_maxSteps,        &QSpinBox::valueChanged,         this, [this]() { writeFromEditor(QStringLiteral("AI/maxSteps"),        m_maxSteps,        SpinBoxK);  emit settingsChanged(); });
    connect(m_maxTokens,       &QSpinBox::valueChanged,         this, [this]() { writeFromEditor(QStringLiteral("AI/maxTokens"),       m_maxTokens,       SpinBoxK);  emit settingsChanged(); });
    connect(m_reasoningEffort, &QComboBox::currentIndexChanged, this, [this]() { writeFromEditor(QStringLiteral("AI/reasoningEffort"), m_reasoningEffort, ComboBoxK); emit settingsChanged(); });
    connect(m_customEndpoint,  &QLineEdit::editingFinished,     this, [this]() { writeFromEditor(QStringLiteral("AI/customEndpoint"),  m_customEndpoint,  LineEditK); emit settingsChanged(); });
    connect(m_customApiKey,    &QLineEdit::editingFinished,     this, [this]() { writeFromEditor(QStringLiteral("AI/customApiKey"),    m_customApiKey,    LineEditK); emit settingsChanged(); });
    connect(m_completionModel, &QLineEdit::editingFinished,     this, [this]() { writeFromEditor(QStringLiteral("AI/completionModel"), m_completionModel, LineEditK); emit settingsChanged(); });
    connect(m_searxngUrl,      &QLineEdit::editingFinished,     this, [this]() { writeFromEditor(QStringLiteral("AI/searxngUrl"),      m_searxngUrl,      LineEditK); emit settingsChanged(); });
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

    glay->addWidget(wrapCheckBoxWithBadge(QStringLiteral("AI/includeOpenFiles"),   m_includeOpenFiles));
    glay->addWidget(wrapCheckBoxWithBadge(QStringLiteral("AI/includeTerminal"),    m_includeTerminal));
    glay->addWidget(wrapCheckBoxWithBadge(QStringLiteral("AI/includeDiagnostics"), m_includeDiagnostics));
    glay->addWidget(wrapCheckBoxWithBadge(QStringLiteral("AI/includeGitDiff"),     m_includeGitDiff));
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

    auto labelOf = [tlay](QWidget *w) { return qobject_cast<QLabel*>(tlay->labelForField(w)); };
    trackControl(QStringLiteral("AI/contextTokenLimit"), m_contextTokenLimit, SpinBoxK, labelOf(m_contextTokenLimit));

    auto wireCheck = [this](QCheckBox *cb, const QString &key) {
        connect(cb, &QCheckBox::toggled, this, [this, cb, key]() {
            writeFromEditor(key, cb, CheckBoxK);
            emit settingsChanged();
        });
    };
    wireCheck(m_includeOpenFiles,   QStringLiteral("AI/includeOpenFiles"));
    wireCheck(m_includeTerminal,    QStringLiteral("AI/includeTerminal"));
    wireCheck(m_includeDiagnostics, QStringLiteral("AI/includeDiagnostics"));
    wireCheck(m_includeGitDiff,     QStringLiteral("AI/includeGitDiff"));
    connect(m_contextTokenLimit, &QSpinBox::valueChanged, this, [this]() {
        writeFromEditor(QStringLiteral("AI/contextTokenLimit"), m_contextTokenLimit, SpinBoxK);
        emit settingsChanged();
    });
}

void SettingsPanel::buildAppearanceTab(QWidget *tab)
{
    auto *lay = new QVBoxLayout(tab);

    // Appearance group
    auto *appearance = new QGroupBox(tr("Appearance"), tab);
    auto *alay = new QVBoxLayout(appearance);

    m_darkTheme = new QCheckBox(tr("Dark theme"), appearance);
    alay->addWidget(wrapCheckBoxWithBadge(QStringLiteral("theme/dark"), m_darkTheme));
    lay->addWidget(appearance);

    // Auto-save group
    auto *autosave = new QGroupBox(tr("Auto-save"), tab);
    auto *aslay = new QFormLayout(autosave);

    m_autoSaveEnabled = new QCheckBox(tr("Enable auto-save"), autosave);
    aslay->addRow(wrapCheckBoxWithBadge(QStringLiteral("autosave/enabled"), m_autoSaveEnabled));

    m_autoSaveInterval = new QSpinBox(autosave);
    m_autoSaveInterval->setRange(5, 600);
    m_autoSaveInterval->setSingleStep(5);
    m_autoSaveInterval->setSuffix(tr(" s"));
    m_autoSaveInterval->setValue(30);
    aslay->addRow(tr("Interval:"), m_autoSaveInterval);
    trackControl(QStringLiteral("autosave/interval"), m_autoSaveInterval, SpinBoxK,
                 qobject_cast<QLabel*>(aslay->labelForField(m_autoSaveInterval)));

    auto *hint = new QLabel(
        tr("<i>Note: auto-save changes apply on next IDE restart.</i>"), autosave);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:%1;")
                            .arg(ChatTheme::pick(ChatTheme::FgDimmed,
                                                  ChatTheme::L_FgDimmed)));
    aslay->addRow(hint);

    lay->addWidget(autosave);
    lay->addStretch();

    // Theme: apply live via ThemeManager free helper.
    connect(m_darkTheme, &QCheckBox::toggled, this, [this](bool checked) {
        Exorcist::setDarkTheme(checked);
        writeFromEditor(QStringLiteral("theme/dark"), m_darkTheme, CheckBoxK);
        emit settingsChanged();
    });

    // Auto-save: persist; AutoSaveManager picks up on next construction.
    connect(m_autoSaveEnabled, &QCheckBox::toggled, this,
            [this](bool checked) {
                if (m_autoSaveInterval)
                    m_autoSaveInterval->setEnabled(checked);
                writeFromEditor(QStringLiteral("autosave/enabled"), m_autoSaveEnabled, CheckBoxK);
                emit settingsChanged();
            });
    connect(m_autoSaveInterval, &QSpinBox::valueChanged, this, [this]() {
        writeFromEditor(QStringLiteral("autosave/interval"), m_autoSaveInterval, SpinBoxK);
        emit settingsChanged();
    });
}

void SettingsPanel::loadSettings()
{
    auto &S = ScopedSettings::instance();
    auto setCheck = [](QCheckBox *cb, bool v) {
        if (!cb) return;
        QSignalBlocker b(cb);
        cb->setChecked(v);
    };
    auto setText = [](QLineEdit *le, const QString &v) {
        if (!le) return;
        QSignalBlocker b(le);
        le->setText(v);
    };
    auto setInt = [](QSpinBox *sb, int v) {
        if (!sb) return;
        QSignalBlocker b(sb);
        sb->setValue(v);
    };
    auto setIdx = [](QComboBox *cb, int v) {
        if (!cb) return;
        QSignalBlocker b(cb);
        cb->setCurrentIndex(v);
    };

    setCheck(m_enableInlineChat, S.value(QStringLiteral("AI/enableInlineChat"), true).toBool());
    setCheck(m_enableGhostText,  S.value(QStringLiteral("AI/enableGhostText"),  true).toBool());
    setCheck(m_enableMemory,     S.value(QStringLiteral("AI/enableMemory"),     true).toBool());
    setCheck(m_enableReview,     S.value(QStringLiteral("AI/enableReview"),     true).toBool());
    setText(m_disabledLangs,     S.value(QStringLiteral("AI/disabledCompletionLangs")).toString());

    setInt(m_maxSteps,           S.value(QStringLiteral("AI/maxSteps"), 20).toInt());
    setInt(m_maxTokens,          S.value(QStringLiteral("AI/maxTokens"), 16384).toInt());
    setIdx(m_reasoningEffort,    S.value(QStringLiteral("AI/reasoningEffort"), 2).toInt());
    setText(m_customEndpoint,    S.value(QStringLiteral("AI/customEndpoint")).toString());
    setText(m_customApiKey,      S.value(QStringLiteral("AI/customApiKey")).toString());
    setText(m_completionModel,   S.value(QStringLiteral("AI/completionModel")).toString());
    setText(m_searxngUrl,        S.value(QStringLiteral("AI/searxngUrl")).toString());

    setCheck(m_includeOpenFiles,   S.value(QStringLiteral("AI/includeOpenFiles"),   true).toBool());
    setCheck(m_includeTerminal,    S.value(QStringLiteral("AI/includeTerminal"),    true).toBool());
    setCheck(m_includeDiagnostics, S.value(QStringLiteral("AI/includeDiagnostics"), true).toBool());
    setCheck(m_includeGitDiff,     S.value(QStringLiteral("AI/includeGitDiff"),     true).toBool());
    setInt(m_contextTokenLimit,    S.value(QStringLiteral("AI/contextTokenLimit"), 100000).toInt());

    // Appearance & Editor — top-level keys
    setCheck(m_darkTheme,        S.value(QStringLiteral("theme/dark"),        true).toBool());
    setCheck(m_autoSaveEnabled,  S.value(QStringLiteral("autosave/enabled"),  true).toBool());
    setInt(m_autoSaveInterval,   qBound(5, S.value(QStringLiteral("autosave/interval"), 30).toInt(), 600));
    if (m_autoSaveInterval && m_autoSaveEnabled)
        m_autoSaveInterval->setEnabled(m_autoSaveEnabled->isChecked());
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

    // Appearance & Editor — top-level keys (shared with ThemeManager / AutoSaveManager)
    if (m_darkTheme)
        s.setValue(QStringLiteral("theme/dark"), m_darkTheme->isChecked());
    if (m_autoSaveEnabled)
        s.setValue(QStringLiteral("autosave/enabled"),
                   m_autoSaveEnabled->isChecked());
    if (m_autoSaveInterval)
        s.setValue(QStringLiteral("autosave/interval"),
                   m_autoSaveInterval->value());
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

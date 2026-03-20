#include "buildtoolbar.h"
#include "cmakeintegration.h"
#include "toolchainmanager.h"
#include "debug/idebugadapter.h"

#include <QComboBox>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

// ── VS2022 toolbar color tokens ────────────────────────────────────────────────
namespace VST {
    constexpr auto BgCombo      = "#3f3f46";   // VS2022 toolbar combo bg
    constexpr auto TxtCombo     = "#c8c8c8";   // combo text
    constexpr auto BtnGreen     = "#388a34";   // VS launch green (bg)
    constexpr auto BtnGreenHov  = "#4cac47";   // launch green hover
    constexpr auto BtnGreenPrs  = "#2c6e29";   // launch green pressed
    constexpr auto BtnRed       = "#c72e24";   // stop red
    constexpr auto BtnRedHov    = "#e03730";   // stop red hover
    constexpr auto BtnNorm      = "transparent";
    constexpr auto BtnNormHov   = "#3e3e40";
    constexpr auto TxtMuted     = "#848484";
    constexpr auto TxtWhite     = "#ffffff";
    constexpr auto Border       = "#3f3f46";
    constexpr auto Separator    = "#555558";
}

// ── Internal helpers ──────────────────────────────────────────────────────────

static QString comboStyle()
{
    return QStringLiteral(
        "QComboBox {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 2px;"
        "  padding: 1px 6px 1px 6px;"
        "  min-height: 20px;"
        "  font-size: 12px;"
        "  selection-background-color: #094771;"
        "}"
        "QComboBox:hover { border-color: #007acc; }"
        "QComboBox::drop-down {"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: right center;"
        "  width: 16px;"
        "  border-left: 1px solid %3;"
        "}"
        "QComboBox::down-arrow { image: none; width: 0; }"
        "QComboBox QAbstractItemView {"
        "  background: #2d2d30;"
        "  color: #c8c8c8;"
        "  border: 1px solid #007acc;"
        "  selection-background-color: #094771;"
        "  selection-color: #ffffff;"
        "  outline: 0;"
        "}"
    ).arg(VST::BgCombo, VST::TxtCombo, VST::Border);
}

static QFrame *makeSeparator(QWidget *parent)
{
    auto *sep = new QFrame(parent);
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(1);
    sep->setFixedHeight(18);
    sep->setStyleSheet(QStringLiteral("background: %1;").arg(VST::Separator));
    return sep;
}

// ── BuildToolbar ──────────────────────────────────────────────────────────────

BuildToolbar::BuildToolbar(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void BuildToolbar::setupUi()
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);

    // ── Configuration combo (Debug / Release / preset) ─────────────────────
    m_configCombo = new QComboBox(this);
    m_configCombo->setFixedWidth(108);
    m_configCombo->setToolTip(tr("Build Configuration (Debug / Release)"));
    m_configCombo->setStyleSheet(comboStyle());
    layout->addWidget(m_configCombo);

    connect(m_configCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (m_cmake)
            m_cmake->setActiveBuildConfig(idx);
        emit configChanged(idx);
    });

    // ── Target / startup combo ─────────────────────────────────────────────
    m_targetCombo = new QComboBox(this);
    m_targetCombo->setFixedWidth(160);
    m_targetCombo->setToolTip(tr("Startup Target (launch target)"));
    m_targetCombo->setStyleSheet(comboStyle());
    layout->addWidget(m_targetCombo);

    layout->addWidget(makeSeparator(this));
    layout->addSpacing(2);

    // ── Configure button (wrench) ──────────────────────────────────────────
    m_configureBtn = new QToolButton(this);
    m_configureBtn->setText(QStringLiteral("\u2699 Configure"));
    m_configureBtn->setToolTip(tr("CMake Configure (Ctrl+Shift+C)"));
    m_configureBtn->setAutoRaise(true);
    m_configureBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  color: #c8c8c8; background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 2px;"
        "  padding: 2px 8px;"
        "  font-size: 12px;"
        "  min-height: 20px;"
        "}"
        "QToolButton:hover { background: %3; border-color: #007acc; }"
        "QToolButton:pressed { background: #0d6ea8; }"
        "QToolButton:disabled { color: #555; border-color: #3a3a3a; }"
    ).arg(VST::BtnNorm, VST::Border, VST::BtnNormHov));
    connect(m_configureBtn, &QToolButton::clicked, this, &BuildToolbar::configureRequested);
    layout->addWidget(m_configureBtn);

    // ── Build button ──────────────────────────────────────────────────────
    m_buildBtn = new QToolButton(this);
    m_buildBtn->setText(QStringLiteral("\U0001F528 Build"));
    m_buildBtn->setToolTip(tr("Build Solution  Ctrl+Shift+B"));
    m_buildBtn->setAutoRaise(true);
    m_buildBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  color: #c8c8c8; background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 2px;"
        "  padding: 2px 10px;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  min-height: 20px;"
        "}"
        "QToolButton:hover { background: %3; border-color: #007acc; }"
        "QToolButton:pressed { background: #0d6ea8; }"
        "QToolButton:disabled { color: #555; border-color: #3a3a3a; }"
    ).arg(VST::BtnNorm, VST::Border, VST::BtnNormHov));
    connect(m_buildBtn, &QToolButton::clicked, this, &BuildToolbar::buildRequested);
    layout->addWidget(m_buildBtn);

    layout->addWidget(makeSeparator(this));
    layout->addSpacing(2);

    // ── Run button (▶ without debugger) — VS teal-green ───────────────────
    m_runBtn = new QToolButton(this);
    m_runBtn->setText(QStringLiteral("\u25B6 Run"));
    m_runBtn->setToolTip(tr("Start Without Debugging  Ctrl+F5"));
    m_runBtn->setAutoRaise(true);
    m_runBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  color: #4ec9b0;"          // VS2022 teal
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 2px;"
        "  padding: 2px 10px;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  min-height: 20px;"
        "}"
        "QToolButton:hover { color: #5ee3ca; background: #1e3a35; border-color: #4ec9b0; }"
        "QToolButton:pressed { background: #0e2822; }"
        "QToolButton:disabled { color: #3a6a62; border-color: #3a3a3a; }"
    ).arg(VST::BtnNorm, VST::Border));
    connect(m_runBtn, &QToolButton::clicked, this, [this]() {
        emit runRequested(selectedTarget());
    });
    layout->addWidget(m_runBtn);

    // ── Debug button (▶▶ with debugger) — VS launch green ────────────────
    m_debugBtn = new QToolButton(this);
    m_debugBtn->setToolTip(tr("Start Debugging  F5"));
    m_debugBtn->setAutoRaise(true);
    m_debugBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  color: %1;"
        "  background: %2;"
        "  border: 1px solid transparent;"
        "  border-radius: 3px;"
        "  padding: 3px 14px;"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "  min-height: 22px;"
        "}"
        "QToolButton:hover { background: %3; }"
        "QToolButton:pressed { background: %4; }"
        "QToolButton:disabled {"
        "  color: #3a6a3a; background: #1e2e1e; border-color: #2a3a2a;"
        "}"
    ).arg(VST::TxtWhite, VST::BtnGreen, VST::BtnGreenHov, VST::BtnGreenPrs));
    connect(m_debugBtn, &QToolButton::clicked, this, [this]() {
        emit debugRequested(selectedTarget());
    });
    layout->addWidget(m_debugBtn);

    // ── Stop button (■) — VS red ───────────────────────────────────────────
    m_stopBtn = new QToolButton(this);
    m_stopBtn->setText(QStringLiteral("\u25A0"));
    m_stopBtn->setToolTip(tr("Stop  Shift+F5"));
    m_stopBtn->setEnabled(false);
    m_stopBtn->setAutoRaise(true);
    m_stopBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  color: %1;"
        "  background: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 2px;"
        "  padding: 2px 8px;"
        "  font-size: 14px;"
        "  font-weight: 700;"
        "  min-height: 20px;"
        "}"
        "QToolButton:hover { background: %4; border-color: %1; }"
        "QToolButton:pressed { background: #8a1010; }"
        "QToolButton:disabled { color: #555; border-color: #3a3a3a; }"
    ).arg(VST::BtnRed, VST::BtnNorm, VST::Border, "#3e1010"));
    connect(m_stopBtn, &QToolButton::clicked, this, &BuildToolbar::stopRequested);
    layout->addWidget(m_stopBtn);

    layout->addWidget(makeSeparator(this));
    layout->addSpacing(2);

    // ── Clean button ───────────────────────────────────────────────────────
    m_cleanBtn = new QToolButton(this);
    m_cleanBtn->setText(tr("Clean"));
    m_cleanBtn->setToolTip(tr("Clean Build Output"));
    m_cleanBtn->setAutoRaise(true);
    m_cleanBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  color: %1; background: %2;"
        "  border: 1px solid %3; border-radius: 2px;"
        "  padding: 2px 8px; font-size: 12px; min-height: 20px;"
        "}"
        "QToolButton:hover { background: %4; border-color: #007acc; }"
        "QToolButton:pressed { background: #0d6ea8; }"
        "QToolButton:disabled { color: #555; border-color: #3a3a3a; }"
    ).arg(VST::TxtCombo, VST::BtnNorm, VST::Border, VST::BtnNormHov));
    connect(m_cleanBtn, &QToolButton::clicked, this, &BuildToolbar::cleanRequested);
    layout->addWidget(m_cleanBtn);

    // ── Status label ───────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("Ready"), this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; padding: 0 10px; font-size: 11px; background: transparent; }")
        .arg(VST::TxtMuted));
    layout->addWidget(m_statusLabel);

    layout->addStretch();

    // Update debug button label once target combo has items
    connect(m_targetCombo, &QComboBox::currentTextChanged, this, [this](const QString &t) {
        const QString label = t.isEmpty()
            ? QStringLiteral("\u25B6\u25B6 Debug")
            : QStringLiteral("\u25B6\u25B6 %1").arg(t);
        m_debugBtn->setText(label);
    });
    m_debugBtn->setText(QStringLiteral("\u25B6\u25B6 Debug"));
}

// ── Public API ────────────────────────────────────────────────────────────────

void BuildToolbar::setCMakeIntegration(CMakeIntegration *cmake)
{
    m_cmake = cmake;
    if (!m_cmake) return;

    connect(m_cmake, &CMakeIntegration::configsChanged, this, &BuildToolbar::refresh);
    connect(m_cmake, &CMakeIntegration::buildFinished, this, [this](bool success, int) {
        updateButtonStates(false, false);
        m_statusLabel->setText(success ? tr("Build succeeded") : tr("Build failed"));
        m_statusLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; padding: 0 10px; font-size: 11px; background: transparent; }")
            .arg(success ? "#4ec9b0" : "#f14c4c"));
        if (success) {
            const QStringList targets = m_cmake->discoverTargets();
            m_targetCombo->clear();
            for (const auto &t : targets)
                m_targetCombo->addItem(QFileInfo(t).fileName(), t);
        }
    });
    // Show "Building…" on first output line (no dedicated buildStarted signal)
    connect(m_cmake, &CMakeIntegration::buildOutput, this, [this](const QString &, bool) {
        if (m_buildBtn->isEnabled()) {   // first output → transition to building
            updateButtonStates(true, false);
            m_statusLabel->setText(tr("Building\u2026"));
            m_statusLabel->setStyleSheet(QStringLiteral(
                "QLabel { color: #d7ba7d; padding: 0 10px; font-size: 11px; background: transparent; }"));
        }
    });
    connect(m_cmake, &CMakeIntegration::configureFinished, this, [this](bool success, const QString &) {
        updateButtonStates(false, false);
        m_statusLabel->setText(success ? tr("Configure done") : tr("Configure failed"));
        m_statusLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; padding: 0 10px; font-size: 11px; background: transparent; }")
            .arg(success ? "#4ec9b0" : "#f14c4c"));
    });
}

void BuildToolbar::setToolchainManager(ToolchainManager *mgr)
{
    m_toolchainMgr = mgr;
}

void BuildToolbar::setDebugAdapter(IDebugAdapter *adapter)
{
    m_debugAdapter = adapter;
    if (!m_debugAdapter) return;

    connect(m_debugAdapter, &IDebugAdapter::started, this, [this]() {
        updateButtonStates(false, true);
        m_statusLabel->setText(tr("Debugging\u2026"));
        m_statusLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #4fc1ff; padding: 0 10px; font-size: 11px; background: transparent; }"));
    });
    connect(m_debugAdapter, &IDebugAdapter::terminated, this, [this]() {
        updateButtonStates(false, false);
        m_statusLabel->setText(tr("Ready"));
        m_statusLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #848484; padding: 0 10px; font-size: 11px; background: transparent; }"));
    });
}

void BuildToolbar::refresh()
{
    m_configCombo->blockSignals(true);
    m_configCombo->clear();
    if (m_cmake) {
        const auto configs = m_cmake->buildConfigs();
        for (const auto &cfg : configs)
            m_configCombo->addItem(cfg.name);
        if (m_cmake->activeBuildConfigIndex() < m_configCombo->count())
            m_configCombo->setCurrentIndex(m_cmake->activeBuildConfigIndex());

        m_targetCombo->clear();
        const QStringList targets = m_cmake->discoverTargets();
        for (const auto &t : targets)
            m_targetCombo->addItem(QFileInfo(t).fileName(), t);
    }
    m_configCombo->blockSignals(false);
}

int BuildToolbar::selectedConfigIndex() const
{
    return m_configCombo->currentIndex();
}

QString BuildToolbar::selectedTarget() const
{
    return m_targetCombo->currentData().toString();
}

void BuildToolbar::updateButtonStates(bool building, bool debugging)
{
    m_configureBtn->setEnabled(!building && !debugging);
    m_buildBtn->setEnabled(!building && !debugging);
    m_runBtn->setEnabled(!building && !debugging);
    m_debugBtn->setEnabled(!building && !debugging);
    m_stopBtn->setEnabled(building || debugging);
    m_cleanBtn->setEnabled(!building && !debugging);
    m_configCombo->setEnabled(!building && !debugging);
    m_targetCombo->setEnabled(!building && !debugging);
}

#include "buildtoolbar.h"
#include "cmakeintegration.h"
#include "toolchainmanager.h"
#include "debug/idebugadapter.h"

#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

BuildToolbar::BuildToolbar(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void BuildToolbar::setupUi()
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(2);

    // ── Configuration combo (Debug/Release/preset) ────────────────────────
    m_configCombo = new QComboBox(this);
    m_configCombo->setMinimumWidth(120);
    m_configCombo->setToolTip(tr("Build Configuration"));
    layout->addWidget(m_configCombo);

    connect(m_configCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (m_cmake)
            m_cmake->setActiveBuildConfig(idx);
        emit configChanged(idx);
    });

    // ── Target combo (executables) ────────────────────────────────────────
    m_targetCombo = new QComboBox(this);
    m_targetCombo->setMinimumWidth(160);
    m_targetCombo->setToolTip(tr("Launch Target"));
    layout->addWidget(m_targetCombo);

    layout->addSpacing(8);

    // ── Configure button ──────────────────────────────────────────────────
    m_configureBtn = new QToolButton(this);
    m_configureBtn->setText(tr("Configure"));
    m_configureBtn->setToolTip(tr("CMake Configure"));
    connect(m_configureBtn, &QToolButton::clicked, this, &BuildToolbar::configureRequested);
    layout->addWidget(m_configureBtn);

    // ── Build button ──────────────────────────────────────────────────────
    m_buildBtn = new QToolButton(this);
    m_buildBtn->setText(tr("\u25B6 Build"));
    m_buildBtn->setToolTip(tr("Build (Ctrl+Shift+B)"));
    m_buildBtn->setStyleSheet(QStringLiteral(
        "QToolButton { padding: 2px 8px; font-weight: bold; }"));
    connect(m_buildBtn, &QToolButton::clicked, this, &BuildToolbar::buildRequested);
    layout->addWidget(m_buildBtn);

    // ── Run button (no debug) ─────────────────────────────────────────────
    m_runBtn = new QToolButton(this);
    m_runBtn->setText(tr("\u25B6"));
    m_runBtn->setToolTip(tr("Run Without Debugging (Ctrl+F5)"));
    m_runBtn->setStyleSheet(QStringLiteral(
        "QToolButton { color: #4ec9b0; font-size: 14px; padding: 2px 6px; }"));
    connect(m_runBtn, &QToolButton::clicked, this, [this]() {
        emit runRequested(selectedTarget());
    });
    layout->addWidget(m_runBtn);

    // ── Debug button ──────────────────────────────────────────────────────
    m_debugBtn = new QToolButton(this);
    m_debugBtn->setText(tr("\u25B6 Debug"));
    m_debugBtn->setToolTip(tr("Start Debugging (F5)"));
    m_debugBtn->setStyleSheet(QStringLiteral(
        "QToolButton { color: #4fc1ff; padding: 2px 8px; font-weight: bold; }"));
    connect(m_debugBtn, &QToolButton::clicked, this, [this]() {
        emit debugRequested(selectedTarget());
    });
    layout->addWidget(m_debugBtn);

    // ── Stop button ───────────────────────────────────────────────────────
    m_stopBtn = new QToolButton(this);
    m_stopBtn->setText(tr("\u25A0"));
    m_stopBtn->setToolTip(tr("Stop (Shift+F5)"));
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet(QStringLiteral(
        "QToolButton { color: #f14c4c; font-size: 14px; padding: 2px 6px; }"));
    connect(m_stopBtn, &QToolButton::clicked, this, &BuildToolbar::stopRequested);
    layout->addWidget(m_stopBtn);

    // ── Clean button ──────────────────────────────────────────────────────
    m_cleanBtn = new QToolButton(this);
    m_cleanBtn->setText(tr("Clean"));
    m_cleanBtn->setToolTip(tr("Clean Build"));
    connect(m_cleanBtn, &QToolButton::clicked, this, &BuildToolbar::cleanRequested);
    layout->addWidget(m_cleanBtn);

    layout->addSpacing(8);

    // ── Status label ──────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("Ready"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; padding: 0 8px;"));
    layout->addWidget(m_statusLabel);

    layout->addStretch();
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
        // Refresh targets after build
        if (success) {
            const QStringList targets = m_cmake->discoverTargets();
            m_targetCombo->clear();
            for (const auto &t : targets)
                m_targetCombo->addItem(QFileInfo(t).fileName(), t);
        }
    });
    connect(m_cmake, &CMakeIntegration::configureFinished, this, [this](bool success, const QString &) {
        updateButtonStates(false, false);
        m_statusLabel->setText(success ? tr("Configure done") : tr("Configure failed"));
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
        m_statusLabel->setText(tr("Debugging..."));
    });
    connect(m_debugAdapter, &IDebugAdapter::terminated, this, [this]() {
        updateButtonStates(false, false);
        m_statusLabel->setText(tr("Ready"));
    });
}

void BuildToolbar::refresh()
{
    // Block signals while repopulating to prevent spurious currentIndexChanged(0)
    // which would reset the saved active config to the first item.
    m_configCombo->blockSignals(true);
    m_configCombo->clear();
    if (m_cmake) {
        const auto configs = m_cmake->buildConfigs();
        for (const auto &cfg : configs)
            m_configCombo->addItem(cfg.name);

        if (m_cmake->activeBuildConfigIndex() < m_configCombo->count())
            m_configCombo->setCurrentIndex(m_cmake->activeBuildConfigIndex());

        // Populate target combo
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

    if (building)
        m_statusLabel->setText(tr("Building..."));
}

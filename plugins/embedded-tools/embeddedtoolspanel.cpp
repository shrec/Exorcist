#include "embeddedtoolspanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <memory>

EmbeddedToolsPanel::EmbeddedToolsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto layout = std::make_unique<QVBoxLayout>(this);

    auto titleLabel = std::make_unique<QLabel>(tr("Embedded workspace actions"), this);
    titleLabel->setObjectName(QStringLiteral("embeddedToolsTitleLabel"));
    layout->addWidget(titleLabel.get());

    auto summaryLabel = std::make_unique<QLabel>(this);
    m_summaryLabel = summaryLabel.get();
    m_summaryLabel->setObjectName(QStringLiteral("embeddedToolsSummaryLabel"));
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    auto flashHintLabel = std::make_unique<QLabel>(this);
    m_flashHintLabel = flashHintLabel.get();
    m_flashHintLabel->setObjectName(QStringLiteral("embeddedToolsFlashHintLabel"));
    m_flashHintLabel->setWordWrap(true);
    layout->addWidget(m_flashHintLabel);

    auto monitorHintLabel = std::make_unique<QLabel>(this);
    m_monitorHintLabel = monitorHintLabel.get();
    m_monitorHintLabel->setObjectName(QStringLiteral("embeddedToolsMonitorHintLabel"));
    m_monitorHintLabel->setWordWrap(true);
    layout->addWidget(m_monitorHintLabel);

    auto debugHintLabel = std::make_unique<QLabel>(this);
    m_debugHintLabel = debugHintLabel.get();
    m_debugHintLabel->setObjectName(QStringLiteral("embeddedToolsDebugHintLabel"));
    m_debugHintLabel->setWordWrap(true);
    layout->addWidget(m_debugHintLabel);

    auto overrideTitleLabel = std::make_unique<QLabel>(tr("Custom commands (optional)"), this);
    overrideTitleLabel->setObjectName(QStringLiteral("embeddedToolsOverridesTitleLabel"));
    layout->addWidget(overrideTitleLabel.get());

    auto flashCommandEdit = std::make_unique<QLineEdit>(this);
    m_flashCommandEdit = flashCommandEdit.get();
    m_flashCommandEdit->setObjectName(QStringLiteral("embeddedToolsFlashCommandEdit"));
    m_flashCommandEdit->setPlaceholderText(
        tr("Leave empty to use the inferred flash command"));
    layout->addWidget(m_flashCommandEdit);

    auto monitorCommandEdit = std::make_unique<QLineEdit>(this);
    m_monitorCommandEdit = monitorCommandEdit.get();
    m_monitorCommandEdit->setObjectName(QStringLiteral("embeddedToolsMonitorCommandEdit"));
    m_monitorCommandEdit->setPlaceholderText(
        tr("Leave empty to use the inferred monitor command"));
    layout->addWidget(m_monitorCommandEdit);

    auto debugCommandEdit = std::make_unique<QLineEdit>(this);
    m_debugCommandEdit = debugCommandEdit.get();
    m_debugCommandEdit->setObjectName(QStringLiteral("embeddedToolsDebugCommandEdit"));
    m_debugCommandEdit->setPlaceholderText(
        tr("Leave empty to use the inferred debug command"));
    layout->addWidget(m_debugCommandEdit);

    auto buttonRow = std::make_unique<QHBoxLayout>();
    auto configureButton = std::make_unique<QPushButton>(tr("Configure"), this);
    auto flashButton = std::make_unique<QPushButton>(tr("Flash / Upload"), this);
    auto monitorButton = std::make_unique<QPushButton>(tr("Open Monitor"), this);
    auto debugButton = std::make_unique<QPushButton>(tr("Start Debug Route"), this);
    auto resetOverridesButton = std::make_unique<QPushButton>(tr("Use Inferred"), this);
    buttonRow->addWidget(configureButton.get());
    buttonRow->addWidget(flashButton.get());
    buttonRow->addWidget(monitorButton.get());
    buttonRow->addWidget(debugButton.get());
    buttonRow->addWidget(resetOverridesButton.get());
    buttonRow->addStretch();
    layout->addLayout(buttonRow.get());
    layout->addStretch();

    connect(configureButton.get(), &QPushButton::clicked,
            this, &EmbeddedToolsPanel::configureRequested);
    connect(flashButton.get(), &QPushButton::clicked,
            this, &EmbeddedToolsPanel::flashRequested);
    connect(monitorButton.get(), &QPushButton::clicked,
            this, &EmbeddedToolsPanel::monitorRequested);
        connect(debugButton.get(), &QPushButton::clicked,
            this, &EmbeddedToolsPanel::debugRequested);
    connect(m_flashCommandEdit, &QLineEdit::textChanged,
            this, &EmbeddedToolsPanel::flashCommandOverrideChanged);
    connect(m_monitorCommandEdit, &QLineEdit::textChanged,
            this, &EmbeddedToolsPanel::monitorCommandOverrideChanged);
        connect(m_debugCommandEdit, &QLineEdit::textChanged,
            this, &EmbeddedToolsPanel::debugCommandOverrideChanged);
    connect(resetOverridesButton.get(), &QPushButton::clicked,
            this, &EmbeddedToolsPanel::resetOverridesRequested);

    titleLabel.release();
    summaryLabel.release();
    flashHintLabel.release();
    monitorHintLabel.release();
    debugHintLabel.release();
    overrideTitleLabel.release();
    flashCommandEdit.release();
    monitorCommandEdit.release();
    debugCommandEdit.release();
    configureButton.release();
    flashButton.release();
    monitorButton.release();
    debugButton.release();
    resetOverridesButton.release();
    buttonRow.release();
    layout.release();

    setWorkspaceSummary(tr("No embedded toolchain markers detected yet."));
    setRecommendedCommands(QString(), QString());
    setDebugRoute(QString(), QString());
    setCommandOverrides(QString(), QString(), QString());
}

void EmbeddedToolsPanel::setWorkspaceSummary(const QString &summary)
{
    m_summaryLabel->setText(summary);
}

void EmbeddedToolsPanel::setRecommendedCommands(const QString &flashCommand,
                                                const QString &monitorCommand)
{
    m_flashHintLabel->setText(flashCommand.isEmpty()
        ? tr("Flash command: no uploader command inferred for this workspace.")
        : tr("Flash command: %1").arg(flashCommand));

    m_monitorHintLabel->setText(monitorCommand.isEmpty()
        ? tr("Monitor command: no serial monitor command inferred for this workspace.")
        : tr("Monitor command: %1").arg(monitorCommand));
}

void EmbeddedToolsPanel::setDebugRoute(const QString &debugSummary,
                                       const QString &debugCommand)
{
    if (debugSummary.isEmpty() && debugCommand.isEmpty()) {
        m_debugHintLabel->setText(tr("Debug route: no debug workflow inferred for this workspace yet."));
        return;
    }

    if (debugCommand.isEmpty()) {
        m_debugHintLabel->setText(tr("Debug route: %1").arg(debugSummary));
        return;
    }

    m_debugHintLabel->setText(tr("Debug route: %1\nDebug command: %2")
        .arg(debugSummary, debugCommand));
}

void EmbeddedToolsPanel::setCommandOverrides(const QString &flashCommand,
                                             const QString &monitorCommand,
                                             const QString &debugCommand)
{
    const QSignalBlocker flashBlocker(m_flashCommandEdit);
    const QSignalBlocker monitorBlocker(m_monitorCommandEdit);
    const QSignalBlocker debugBlocker(m_debugCommandEdit);
    m_flashCommandEdit->setText(flashCommand);
    m_monitorCommandEdit->setText(monitorCommand);
    m_debugCommandEdit->setText(debugCommand);
}

QString EmbeddedToolsPanel::flashCommandOverride() const
{
    return m_flashCommandEdit ? m_flashCommandEdit->text().trimmed() : QString();
}

QString EmbeddedToolsPanel::monitorCommandOverride() const
{
    return m_monitorCommandEdit ? m_monitorCommandEdit->text().trimmed() : QString();
}

QString EmbeddedToolsPanel::debugCommandOverride() const
{
    return m_debugCommandEdit ? m_debugCommandEdit->text().trimmed() : QString();
}
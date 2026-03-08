#pragma once

#include <QObject>
#include <QString>

class QWidget;

// ── IAgentSettingsPageProvider ────────────────────────────────────────────────
//
// Optional interface that an IPlugin can implement to contribute a
// provider-specific settings/configuration widget to the IDE settings dialog.
//
// Usage:
//   auto *settingsPage = qobject_cast<IAgentSettingsPageProvider *>(plugin);
//   if (settingsPage)
//       settingsDialog->addPage(settingsPage->settingsPageTitle(),
//                               settingsPage->createSettingsPage(parent));

class IAgentSettingsPageProvider
{
public:
    virtual ~IAgentSettingsPageProvider() = default;

    /// Human-readable title shown in the settings tab/tree (e.g. "Copilot").
    virtual QString settingsPageTitle() const = 0;

    /// Create the configuration widget.  Caller takes ownership (via parent).
    virtual QWidget *createSettingsPage(QWidget *parent) = 0;
};

#define EXORCIST_AGENT_SETTINGS_PAGE_IID "org.exorcist.IAgentSettingsPageProvider/1.0"
Q_DECLARE_INTERFACE(IAgentSettingsPageProvider, EXORCIST_AGENT_SETTINGS_PAGE_IID)

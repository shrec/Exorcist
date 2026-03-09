#pragma once

#include <QString>

// ── Notification Service ─────────────────────────────────────────────────────
//
// Stable SDK interface for showing notifications to the user.
// Maps to status bar messages, toast notifications, and message boxes.

class INotificationService
{
public:
    virtual ~INotificationService() = default;

    /// Show an informational message (non-blocking toast / status bar).
    virtual void info(const QString &text) = 0;

    /// Show a warning message.
    virtual void warning(const QString &text) = 0;

    /// Show an error message.
    virtual void error(const QString &text) = 0;

    /// Show a message in the status bar (auto-clears after timeout ms, 0 = persistent).
    virtual void statusMessage(const QString &text, int timeoutMs = 5000) = 0;
};

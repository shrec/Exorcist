#pragma once

#include <QObject>
#include <QString>

// ── Output Service ───────────────────────────────────────────────────────────
//
// Stable SDK interface for writing to the IDE output panel.
// Any plugin (build, test, lint, language server, etc.) can resolve this
// service and write output lines without depending on the concrete
// OutputPanel class or knowing which dock hosts it.
//
// Channels allow different producers to coexist in the same panel.
// The panel implementation decides how to display channels (e.g. a combo
// box, tabs, or a single merged stream).
//
// Registered in ServiceRegistry as "outputService".

class IOutputService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// Append a single line to the given channel.
    /// @param channel  Logical channel name (e.g. "Build", "Test", "Lint").
    /// @param text     The text to append.
    /// @param isError  If true, the line is styled as an error.
    virtual void appendLine(const QString &channel,
                            const QString &text,
                            bool isError = false) = 0;

    /// Append a line to the default (active) channel.
    virtual void appendLine(const QString &text, bool isError = false) = 0;

    /// Clear the contents of the given channel.
    virtual void clearChannel(const QString &channel) = 0;

    /// Clear the active channel.
    virtual void clear() = 0;

    /// Show the output panel and optionally switch to a specific channel.
    /// If @p channel is empty, the panel is shown with the current channel.
    virtual void show(const QString &channel = {}) = 0;

    /// The currently active channel name.
    virtual QString activeChannel() const = 0;

signals:
    /// Emitted after a line is appended to any channel.
    void lineAppended(const QString &channel, const QString &text, bool isError);

    /// Emitted after a channel is cleared.
    void channelCleared(const QString &channel);
};

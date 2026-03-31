#pragma once

#include <QIcon>
#include <QSize>
#include <QString>

/// Centralized icon provider for Exorcist.
///
/// Loads SVG icons from Qt resources, tints them to match the current
/// dark/light theme, and caches the result. Icons are the VS Code
/// Codicon aesthetic: simple, recognizable, 16×16.
///
/// Usage:
///   auto icon = ThemeIcons::icon("play");        // normal icon
///   auto icon = ThemeIcons::icon("stop", "#c72e24"); // colored icon
class ThemeIcons
{
public:
    /// Return a tinted QIcon for the named icon.
    /// @param name  Icon name without extension (e.g. "play", "build")
    /// @param color Hex color to tint the SVG's currentColor (default: theme foreground)
    static QIcon icon(const QString &name,
                      const QString &color = QString());

    /// Return icon at an explicit pixel size (default 16).
    static QIcon icon(const QString &name, const QSize &size,
                      const QString &color = QString());

    /// Invalidate the icon cache (call after theme change).
    static void clearCache();

    /// Default foreground color for the current theme.
    static QString defaultColor();

private:
    ThemeIcons() = delete;
};

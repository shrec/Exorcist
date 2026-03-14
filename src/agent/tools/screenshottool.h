#pragma once

#include "../itool.h"

#include <QPixmap>

#include <functional>

// ── ScreenshotTool ───────────────────────────────────────────────────────────
// Captures a screenshot of the main window or a specific widget.
// Returns the saved file path so a vision-capable model can analyze it.

class ScreenshotTool : public ITool
{
public:
    // widgetGrabber(target) → QPixmap of that target (empty if not found)
    //   target is "window", "editor", or a panel name like "debug", "search", "agent"
    using WidgetGrabber = std::function<QPixmap(const QString &target)>;

    explicit ScreenshotTool(WidgetGrabber grabber);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    WidgetGrabber m_grabber;
};

#include "screenshottool.h"

#include <QDateTime>
#include <QStandardPaths>

ScreenshotTool::ScreenshotTool(WidgetGrabber grabber)
    : m_grabber(std::move(grabber)) {}

ToolSpec ScreenshotTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("screenshot");
    s.description = QStringLiteral(
        "Capture a screenshot of the application window or a specific "
        "panel. Useful for verifying UI layout, checking visual "
        "correctness, and diagnosing display issues. "
        "Returns the file path to the saved PNG image.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("target"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("What to capture: 'window' (full app), "
                                "'editor' (active editor), or a panel "
                                "name like 'debug', 'search', 'agent', "
                                "'terminal', 'git'. Default: 'window'.")}
            }},
            {QStringLiteral("savePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Optional file path to save the PNG. "
                                "If omitted, saves to a temp directory.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{}}
    };
    return s;
}

ToolExecResult ScreenshotTool::invoke(const QJsonObject &args)
{
    const QString target = args[QLatin1String("target")].toString(
        QStringLiteral("window"));
    QString savePath = args[QLatin1String("savePath")].toString();

    const QPixmap pixmap = m_grabber(target);
    if (pixmap.isNull())
        return {false, {}, {},
                QStringLiteral("Could not capture '%1'. "
                               "Widget not found or not visible.").arg(target)};

    // Determine save path
    if (savePath.isEmpty()) {
        const QString tmpDir = QStandardPaths::writableLocation(
            QStandardPaths::TempLocation);
        savePath = tmpDir + QStringLiteral("/exorcist_screenshot_%1.png")
                       .arg(QDateTime::currentMSecsSinceEpoch());
    }

    if (!pixmap.save(savePath, "PNG"))
        return {false, {}, {},
                QStringLiteral("Failed to save screenshot to %1").arg(savePath)};

    const QString text = QStringLiteral(
        "Screenshot captured: %1\nSize: %2x%3 pixels\nTarget: %4")
        .arg(savePath)
        .arg(pixmap.width())
        .arg(pixmap.height())
        .arg(target);

    QJsonObject data;
    data[QLatin1String("path")]   = savePath;
    data[QLatin1String("width")]  = pixmap.width();
    data[QLatin1String("height")] = pixmap.height();
    data[QLatin1String("target")] = target;

    return {true, data, text, {}};
}

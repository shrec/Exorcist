#pragma once

#include <QWidget>

class QTextBrowser;
class QToolButton;
class QVBoxLayout;

/// Debug panel showing raw AI request/response JSON logs, tool call traces,
/// and context inspector.
class RequestLogPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RequestLogPanel(QWidget *parent = nullptr);

    /// Log a request being sent to the model.
    void logRequest(const QString &requestId, const QString &model,
                    int messageCount, int toolCount);

    /// Log a response chunk or finished event.
    void logResponse(const QString &requestId, const QString &text);

    /// Log a tool call invocation.
    void logToolCall(const QString &toolName, const QJsonObject &args, bool ok,
                     const QString &resultPreview);

    /// Log an error.
    void logError(const QString &requestId, const QString &error);

    /// Log raw context that was sent.
    void logContext(const QString &section, const QString &content);

public slots:
    void clear();

private:
    void appendLog(const QString &css, const QString &label, const QString &body);

    QTextBrowser *m_browser;
    QToolButton  *m_clearBtn;
    int           m_entryCount = 0;
};

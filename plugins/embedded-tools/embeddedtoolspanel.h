#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;

class EmbeddedToolsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EmbeddedToolsPanel(QWidget *parent = nullptr);

    void setWorkspaceSummary(const QString &summary);
    void setRecommendedCommands(const QString &flashCommand,
                                const QString &monitorCommand);
    void setDebugRoute(const QString &debugSummary,
                       const QString &debugCommand);
    void setCommandOverrides(const QString &flashCommand,
                             const QString &monitorCommand,
                             const QString &debugCommand);
    QString flashCommandOverride() const;
    QString monitorCommandOverride() const;
    QString debugCommandOverride() const;

signals:
    void flashRequested();
    void monitorRequested();
    void debugRequested();
    void configureRequested();
    void flashCommandOverrideChanged(const QString &command);
    void monitorCommandOverrideChanged(const QString &command);
    void debugCommandOverrideChanged(const QString &command);
    void resetOverridesRequested();

private:
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_flashHintLabel = nullptr;
    QLabel *m_monitorHintLabel = nullptr;
    QLabel *m_debugHintLabel = nullptr;
    QLineEdit *m_flashCommandEdit = nullptr;
    QLineEdit *m_monitorCommandEdit = nullptr;
    QLineEdit *m_debugCommandEdit = nullptr;
};
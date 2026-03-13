#pragma once

#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

class QJsonArray;
class QJsonObject;
class QJsonValue;
class QTimer;

#include "chatcontentpart.h"
#include "chatthemetokens.h"

class ChatToolInvocationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatToolInvocationWidget(QWidget *parent = nullptr);

    void setToolData(const ChatContentPart &part);
    void updateState(const ChatContentPart &part);

signals:
    void confirmed(const QString &callId, int approval);

private:
    static QString formatToolInput(const QString &toolName, const QString &text);
    static QString formatToolOutput(const QString &toolName, const QString &text);
    static QString formatJsonObject(const QJsonObject &obj, int depth, int maxLen);
    static QString formatJsonArray(const QJsonArray &arr, int depth, int maxLen);
    static QString formatJsonValue(const QJsonValue &val, int depth, int maxLen);
    static QString formatTerminalOutput(const QString &text);
    static QString keyValueLine(const QString &key, const QString &value);
    static QString diffLine(const QString &text, bool isInsert);
    static QString limitText(const QString &text, int max);
    static QString escapeAndLimit(const QString &text, int maxChars);
    static QString iconForTool(const QString &toolName);
    static QString spinnerFrame();

    bool isTerminalTool() const;
    void startSpinner();

    QWidget     *m_card         = nullptr;
    QLabel      *m_iconLabel    = nullptr;
    QLabel      *m_titleLabel   = nullptr;
    QLabel      *m_stateBadge   = nullptr;
    QLabel      *m_descLabel    = nullptr;
    QToolButton *m_ioToggle     = nullptr;
    QWidget     *m_ioPanel      = nullptr;
    QLabel      *m_inputLabel   = nullptr;
    QLabel      *m_outputLabel  = nullptr;
    QWidget     *m_confirmRow   = nullptr;
    QToolButton *m_allowBtn     = nullptr;
    QToolButton *m_allowMenuBtn = nullptr;
    QToolButton *m_denyBtn      = nullptr;
    QString      m_callId;
    QString      m_toolName;
    QTimer      *m_spinTimer    = nullptr;
    int          m_spinIndex    = 0;
};

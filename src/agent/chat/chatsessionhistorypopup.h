#pragma once

#include <QDateTime>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "chatthemetokens.h"

class ChatSessionHistoryPopup : public QWidget
{
    Q_OBJECT

public:
    struct SessionEntry
    {
        QString   id;
        QString   title;
        QDateTime timestamp;
        int       turnCount = 0;
    };

    explicit ChatSessionHistoryPopup(QWidget *parent = nullptr);

    void setSessions(const QList<SessionEntry> &sessions);
    void showAt(const QPoint &globalPos);

signals:
    void sessionSelected(const QString &sessionId);
    void sessionDeleted(const QString &sessionId);
    void sessionRenamed(const QString &sessionId, const QString &newTitle);
    void newSessionRequested();

private:
    void rebuildList();
    void filterList();

    QLineEdit            *m_searchBox = nullptr;
    QListWidget          *m_list      = nullptr;
    QList<SessionEntry>   m_sessions;
};

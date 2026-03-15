#pragma once

#include <QWidget>

#include "chatcontentpart.h"
#include "chatthemetokens.h"

class QVBoxLayout;

class ChatFollowupsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatFollowupsWidget(QWidget *parent = nullptr);

    void setFollowups(const QVector<ChatContentPart::FollowupItem> &items);

signals:
    void followupClicked(const QString &message);

private:
    static QString chipStyle();

    QVBoxLayout *m_layout = nullptr;
};

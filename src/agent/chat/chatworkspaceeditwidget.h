#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "chatcontentpart.h"
#include "chatthemetokens.h"

class ChatWorkspaceEditWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWorkspaceEditWidget(QWidget *parent = nullptr);

    void setEditedFiles(const QVector<ChatContentPart::EditedFile> &files);

signals:
    void fileClicked(int fileIndex, const QString &path);
    void keepFile(int fileIndex, const QString &path);
    void undoFile(int fileIndex, const QString &path);
    void keepAll();
    void undoAll();

private:
    QWidget *createFileRow(const ChatContentPart::EditedFile &file, int index);

    static QString buttonStyle(const QString &bg, const QString &fg, const QString &hover);
    static QString smallButtonStyle();
    static QString formatDiffHtml(const QString &diffText);

    QWidget     *m_card           = nullptr;
    QVBoxLayout *m_cardLayout     = nullptr;
    QLabel      *m_headerLabel    = nullptr;
    QToolButton *m_keepAllBtn     = nullptr;
    QToolButton *m_undoAllBtn     = nullptr;
    QWidget     *m_fileListWidget = nullptr;
    QVBoxLayout *m_fileListLayout = nullptr;
};

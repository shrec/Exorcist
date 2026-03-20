#pragma once

#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "chatcontentpart.h"
#include "chatthemetokens.h"

class ChainLineWidget : public QWidget
{
public:
    explicit ChainLineWidget(const QString &icon, QWidget *parent = nullptr);

    void setShowLine(bool top, bool bottom);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_icon;
    bool    m_topLine    = false;
    bool    m_bottomLine = false;
};

class ChatThinkingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatThinkingWidget(QWidget *parent = nullptr);

    void setThinkingText(const QString &text, bool streaming = false);
    void appendDelta(const QString &delta);
    void finalize();
    void setCollapsed(bool collapsed);

    bool isCollapsed() const { return !m_headerBtn->isChecked(); }
    QString rawText() const { return m_rawText; }
    bool isStreaming() const { return m_streaming; }

    struct ToolItem {
        QWidget         *row         = nullptr;
        ChainLineWidget *chain       = nullptr;
        QLabel          *title       = nullptr;
        QLabel          *state       = nullptr;
        QToolButton     *ioToggle    = nullptr;
        QWidget         *ioPanel     = nullptr;
        QLabel          *inputLabel  = nullptr;
        QLabel          *outputLabel = nullptr;
        QWidget         *confirmRow  = nullptr;
        QToolButton     *allowBtn    = nullptr;
        QToolButton     *alwaysAllowBtn = nullptr;
        QToolButton     *denyBtn     = nullptr;
        QString          callId;
        QString          toolName;
        ChatContentPart::ToolState toolState = ChatContentPart::ToolState::Queued;
    };

    ToolItem *addToolItem(const ChatContentPart &part);
    void updateToolState(const QString &callId, const ChatContentPart &part);
    ToolItem *findToolItem(const QString &callId) const;
    int toolCount() const { return m_toolCount; }

signals:
    void toolConfirmed(const QString &callId, int approval);

private:
    void updateBoxStyle(bool streaming);
    void updateHeaderStyle(bool expanded);
    void updateHeaderText(bool streaming, bool hasToolResults);
    void updateChainLines();
    static QString toolIcon(const QString &name);

    QWidget         *m_box                   = nullptr;
    QToolButton     *m_headerBtn             = nullptr;
    QWidget         *m_contentArea           = nullptr;
    QWidget         *m_thinkingTextContainer = nullptr;
    ChainLineWidget *m_thinkingChainLine     = nullptr;
    QLabel          *m_contentLabel           = nullptr;
    QWidget         *m_toolsContainer        = nullptr;
    QVBoxLayout     *m_toolsLayout           = nullptr;
    QWidget         *m_spinnerRow            = nullptr;
    ChainLineWidget *m_spinnerChainLine      = nullptr;
    QLabel          *m_spinnerLabel           = nullptr;

    QList<ToolItem *> m_toolItems;
    QString           m_rawText;
    bool              m_streaming        = false;
    int               m_toolCount        = 0;
    int               m_activeToolCount  = 0; // tools in Queued or Streaming state
};

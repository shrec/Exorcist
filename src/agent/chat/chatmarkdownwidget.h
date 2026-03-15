#pragma once

#include <QTextBrowser>
#include <QWidget>

#include "chatthemetokens.h"
#include "../../ui/markdownrenderer.h"

class ChatMarkdownWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatMarkdownWidget(QWidget *parent = nullptr);

    void setMarkdown(const QString &md, bool streaming = false);
    void appendDelta(const QString &delta);
    void finalize();

    QString rawMarkdown() const { return m_rawMarkdown; }
    const QList<MarkdownRenderer::CodeBlock> &codeBlocks() const { return m_lastBlocks; }

signals:
    void anchorClicked(const QUrl &url);
    void insertCodeRequested(const QString &code);

private slots:
    void handleAnchorClick(const QUrl &url);

private:
    static QString defaultCss();

    QTextBrowser *m_browser = nullptr;
    QString m_rawMarkdown;
    QList<MarkdownRenderer::CodeBlock> m_lastBlocks;
};

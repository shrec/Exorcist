#pragma once

#include <QWidget>

class QPlainTextEdit;
class QSplitter;
class QLabel;

class DiffViewerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DiffViewerPanel(QWidget *parent = nullptr);

    void showDiff(const QString &filePath,
                  const QString &oldText, const QString &newText);
    void clear();

private:
    void highlightDiffLines();

    QLabel          *m_titleLabel  = nullptr;
    QPlainTextEdit  *m_leftEditor  = nullptr;
    QPlainTextEdit  *m_rightEditor = nullptr;
    QSplitter       *m_splitter    = nullptr;
    QString          m_filePath;
};

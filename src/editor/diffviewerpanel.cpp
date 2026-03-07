#include "diffviewerpanel.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QTextBlock>

DiffViewerPanel::DiffViewerPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet(
        QStringLiteral("padding:4px 8px; font-weight:bold;"));
    layout->addWidget(m_titleLabel);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_leftEditor = new QPlainTextEdit(this);
    m_leftEditor->setReadOnly(true);
    m_leftEditor->setLineWrapMode(QPlainTextEdit::NoWrap);

    m_rightEditor = new QPlainTextEdit(this);
    m_rightEditor->setReadOnly(true);
    m_rightEditor->setLineWrapMode(QPlainTextEdit::NoWrap);

    m_splitter->addWidget(m_leftEditor);
    m_splitter->addWidget(m_rightEditor);
    m_splitter->setSizes({500, 500});

    layout->addWidget(m_splitter);

    // Sync scrolling
    connect(m_leftEditor->verticalScrollBar(), &QAbstractSlider::valueChanged,
            m_rightEditor->verticalScrollBar(), &QAbstractSlider::setValue);
    connect(m_rightEditor->verticalScrollBar(), &QAbstractSlider::valueChanged,
            m_leftEditor->verticalScrollBar(), &QAbstractSlider::setValue);
    connect(m_leftEditor->horizontalScrollBar(), &QAbstractSlider::valueChanged,
            m_rightEditor->horizontalScrollBar(), &QAbstractSlider::setValue);
    connect(m_rightEditor->horizontalScrollBar(), &QAbstractSlider::valueChanged,
            m_leftEditor->horizontalScrollBar(), &QAbstractSlider::setValue);
}

void DiffViewerPanel::showDiff(const QString &filePath,
                               const QString &oldText,
                               const QString &newText)
{
    m_filePath = filePath;
    m_titleLabel->setText(tr("Diff: %1").arg(filePath));
    m_leftEditor->setPlainText(oldText);
    m_rightEditor->setPlainText(newText);
    highlightDiffLines();
}

void DiffViewerPanel::clear()
{
    m_filePath.clear();
    m_titleLabel->clear();
    m_leftEditor->clear();
    m_rightEditor->clear();
}

void DiffViewerPanel::highlightDiffLines()
{
    // Simple line-by-line comparison — highlight differing lines
    const QStringList oldLines = m_leftEditor->toPlainText().split('\n');
    const QStringList newLines = m_rightEditor->toPlainText().split('\n');

    const QColor removedBg(80, 30, 30);
    const QColor addedBg(30, 80, 30);

    // Highlight left (removed lines)
    {
        QList<QTextEdit::ExtraSelection> sels;
        QTextBlock block = m_leftEditor->document()->begin();
        int i = 0;
        while (block.isValid()) {
            const QString line = block.text();
            bool differs = (i >= newLines.size()) || (line != newLines.at(i));
            if (differs) {
                QTextEdit::ExtraSelection sel;
                sel.format.setBackground(removedBg);
                sel.format.setProperty(QTextFormat::FullWidthSelection, true);
                sel.cursor = QTextCursor(block);
                sels.append(sel);
            }
            block = block.next();
            ++i;
        }
        m_leftEditor->setExtraSelections(sels);
    }

    // Highlight right (added lines)
    {
        QList<QTextEdit::ExtraSelection> sels;
        QTextBlock block = m_rightEditor->document()->begin();
        int i = 0;
        while (block.isValid()) {
            const QString line = block.text();
            bool differs = (i >= oldLines.size()) || (line != oldLines.at(i));
            if (differs) {
                QTextEdit::ExtraSelection sel;
                sel.format.setBackground(addedBg);
                sel.format.setProperty(QTextFormat::FullWidthSelection, true);
                sel.cursor = QTextCursor(block);
                sels.append(sel);
            }
            block = block.next();
            ++i;
        }
        m_rightEditor->setExtraSelections(sels);
    }
}

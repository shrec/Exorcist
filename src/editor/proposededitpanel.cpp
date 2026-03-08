#include "proposededitpanel.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBlock>
#include <QVBoxLayout>

#include <QFileInfo>

ProposedEditPanel::ProposedEditPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(8, 4, 8, 4);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #ccc; font-size: 12px; }"));
    toolbar->addWidget(m_summaryLabel, 1);

    m_acceptAllBtn = new QPushButton(tr("✓ Accept All"), this);
    m_acceptAllBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2ea04370; color: #4ec9b0; border: 1px solid #2ea043; "
        "border-radius: 3px; padding: 4px 12px; font-size: 12px; }"
        "QPushButton:hover { background: #2ea04390; }"));
    toolbar->addWidget(m_acceptAllBtn);

    m_rejectAllBtn = new QPushButton(tr("✕ Reject All"), this);
    m_rejectAllBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #f8514970; color: #f85149; border: 1px solid #f85149; "
        "border-radius: 3px; padding: 4px 12px; font-size: 12px; }"
        "QPushButton:hover { background: #f8514990; }"));
    toolbar->addWidget(m_rejectAllBtn);

    vbox->addLayout(toolbar);

    // Tab widget for multiple files
    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: none; }"
        "QTabBar::tab { background: #2d2d2d; color: #ccc; padding: 4px 12px; "
        "border: 1px solid #3e3e42; border-bottom: none; }"
        "QTabBar::tab:selected { background: #1e1e1e; color: #fff; }"));
    vbox->addWidget(m_tabs);

    // Signals
    connect(m_acceptAllBtn, &QPushButton::clicked, this, [this]() {
        for (const ProposedEdit &edit : m_edits)
            emit editAccepted(edit.filePath, edit.proposedText);
        emit allAccepted();
        clear();
    });
    connect(m_rejectAllBtn, &QPushButton::clicked, this, [this]() {
        for (const ProposedEdit &edit : m_edits)
            emit editRejected(edit.filePath);
        emit allRejected();
        clear();
    });
}

void ProposedEditPanel::setEdits(const QList<ProposedEdit> &edits)
{
    clear();
    m_edits = edits;
    m_summaryLabel->setText(
        tr("Proposed changes to %1 file%2")
            .arg(edits.size())
            .arg(edits.size() == 1 ? "" : "s"));

    for (int i = 0; i < edits.size(); ++i)
        addFileDiff(edits.at(i), i);
}

void ProposedEditPanel::clear()
{
    while (m_tabs->count() > 0) {
        QWidget *w = m_tabs->widget(0);
        m_tabs->removeTab(0);
        w->deleteLater();
    }
    m_edits.clear();
    m_summaryLabel->clear();
}

void ProposedEditPanel::addFileDiff(const ProposedEdit &edit, int index)
{
    auto *page = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    // Side-by-side editors
    auto *splitter = new QSplitter(Qt::Horizontal, page);

    auto makeEditor = [](const QString &text, const QString &header) {
        auto *container = new QWidget;
        auto *vl = new QVBoxLayout(container);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);

        auto *lbl = new QLabel(header);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { background: #252526; color: #888; padding: 2px 8px; "
            "font-size: 11px; border-bottom: 1px solid #3e3e42; }"));
        vl->addWidget(lbl);

        auto *editor = new QPlainTextEdit;
        editor->setReadOnly(true);
        editor->setPlainText(text);
        editor->setFont(QFont(QStringLiteral("Consolas"), 9));
        editor->setStyleSheet(QStringLiteral(
            "QPlainTextEdit { background: #1e1e1e; color: #d4d4d4; "
            "border: none; selection-background-color: #264f78; }"));
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        vl->addWidget(editor);
        return container;
    };

    auto *leftContainer = makeEditor(edit.originalText, tr("Original"));
    auto *rightContainer = makeEditor(edit.proposedText, tr("Proposed"));
    splitter->addWidget(leftContainer);
    splitter->addWidget(rightContainer);
    splitter->setSizes({500, 500});

    // Sync scrolling
    auto *leftEditor = leftContainer->findChild<QPlainTextEdit *>();
    auto *rightEditor = rightContainer->findChild<QPlainTextEdit *>();
    if (leftEditor && rightEditor) {
        connect(leftEditor->verticalScrollBar(), &QScrollBar::valueChanged,
                rightEditor->verticalScrollBar(), &QScrollBar::setValue);
        connect(rightEditor->verticalScrollBar(), &QScrollBar::valueChanged,
                leftEditor->verticalScrollBar(), &QScrollBar::setValue);
        highlightDiffLines(leftEditor, rightEditor);
    }

    pageLayout->addWidget(splitter);

    // Per-file accept/reject buttons
    auto *btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(8, 4, 8, 4);
    btnRow->addStretch();

    auto *acceptBtn = new QPushButton(tr("✓ Accept"), page);
    acceptBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2ea04370; color: #4ec9b0; border: 1px solid #2ea043; "
        "border-radius: 3px; padding: 3px 10px; font-size: 11px; }"
        "QPushButton:hover { background: #2ea04390; }"));
    connect(acceptBtn, &QPushButton::clicked, this, [this, index]() {
        if (index < m_edits.size()) {
            emit editAccepted(m_edits.at(index).filePath,
                              m_edits.at(index).proposedText);
            m_tabs->removeTab(m_tabs->indexOf(
                m_tabs->widget(index < m_tabs->count() ? index : 0)));
        }
    });
    btnRow->addWidget(acceptBtn);

    auto *rejectBtn = new QPushButton(tr("✕ Reject"), page);
    rejectBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #f8514970; color: #f85149; border: 1px solid #f85149; "
        "border-radius: 3px; padding: 3px 10px; font-size: 11px; }"
        "QPushButton:hover { background: #f8514990; }"));
    connect(rejectBtn, &QPushButton::clicked, this, [this, index]() {
        if (index < m_edits.size()) {
            emit editRejected(m_edits.at(index).filePath);
            m_tabs->removeTab(m_tabs->indexOf(
                m_tabs->widget(index < m_tabs->count() ? index : 0)));
        }
    });
    btnRow->addWidget(rejectBtn);

    pageLayout->addLayout(btnRow);

    const QString tabTitle = QFileInfo(edit.filePath).fileName();
    m_tabs->addTab(page, tabTitle);
}

void ProposedEditPanel::highlightDiffLines(QPlainTextEdit *left,
                                           QPlainTextEdit *right)
{
    const QStringList oldLines = left->toPlainText().split(QLatin1Char('\n'));
    const QStringList newLines = right->toPlainText().split(QLatin1Char('\n'));

    // Simple line-by-line diff highlighting (LCS would be better, but
    // this is sufficient for review).

    auto highlightEditor = [](QPlainTextEdit *editor, const QSet<int> &lines,
                              const QColor &color) {
        QList<QTextEdit::ExtraSelection> sels;
        for (int lineNo : lines) {
            QTextBlock block = editor->document()->findBlockByNumber(lineNo);
            if (!block.isValid()) continue;
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(color);
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            sel.cursor = QTextCursor(block);
            sels.append(sel);
        }
        editor->setExtraSelections(sels);
    };

    // Mark lines that differ
    QSet<int> removedLines, addedLines;
    const int maxLines = qMax(oldLines.size(), newLines.size());
    for (int i = 0; i < maxLines; ++i) {
        const QString oldLine = i < oldLines.size() ? oldLines[i] : QString();
        const QString newLine = i < newLines.size() ? newLines[i] : QString();
        if (oldLine != newLine) {
            if (i < oldLines.size()) removedLines.insert(i);
            if (i < newLines.size()) addedLines.insert(i);
        }
    }

    highlightEditor(left, removedLines, QColor(255, 60, 60, 30));   // red tint
    highlightEditor(right, addedLines, QColor(60, 255, 60, 30));    // green tint
}

#include "diffexplorerpanel.h"
#include "gitservice.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QVBoxLayout>

DiffExplorerPanel::DiffExplorerPanel(GitService *git, QWidget *parent)
    : QWidget(parent)
    , m_git(git)
{
    auto *mainLayout = std::make_unique<QVBoxLayout>().release();
    setLayout(mainLayout);

    // ── Revision picker toolbar ─────────────────────────────────────────
    auto *toolRow = std::make_unique<QHBoxLayout>().release();
    m_fromCombo = new QComboBox(this);
    m_fromCombo->setEditable(true);
    m_fromCombo->setPlaceholderText(tr("From (branch/commit)"));
    m_fromCombo->setMinimumWidth(140);

    m_toCombo = new QComboBox(this);
    m_toCombo->setEditable(true);
    m_toCombo->setPlaceholderText(tr("To (branch/commit)"));
    m_toCombo->setMinimumWidth(140);

    m_compareBtn = new QPushButton(tr("Compare"), this);
    m_summaryLabel = new QLabel(this);

    toolRow->addWidget(new QLabel(tr("From:"), this));
    toolRow->addWidget(m_fromCombo, 1);
    toolRow->addWidget(new QLabel(tr("To:"), this));
    toolRow->addWidget(m_toCombo, 1);
    toolRow->addWidget(m_compareBtn);
    toolRow->addWidget(m_summaryLabel);
    toolRow->addStretch();
    mainLayout->addLayout(toolRow);

    // ── Main splitter: file list | diff view ────────────────────────────
    auto *mainSplit = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(mainSplit, 1);

    // File list on the left
    auto *leftWidget = new QWidget(this);
    auto *leftLayout = std::make_unique<QVBoxLayout>().release();
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftWidget->setLayout(leftLayout);
    m_fileList = new QListWidget(this);
    leftLayout->addWidget(m_fileList);
    mainSplit->addWidget(leftWidget);

    // Diff view on the right
    auto *rightWidget = new QWidget(this);
    auto *rightLayout = std::make_unique<QVBoxLayout>().release();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightWidget->setLayout(rightLayout);

    m_diffTitle = new QLabel(this);
    m_diffTitle->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px;"));
    rightLayout->addWidget(m_diffTitle);

    auto *diffSplit = new QSplitter(Qt::Horizontal, this);

    m_leftPane = new QPlainTextEdit(this);
    m_leftPane->setReadOnly(true);
    m_leftPane->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont monoFont(QStringLiteral("Consolas"), 10);
    monoFont.setStyleHint(QFont::Monospace);
    m_leftPane->setFont(monoFont);

    m_rightPane = new QPlainTextEdit(this);
    m_rightPane->setReadOnly(true);
    m_rightPane->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_rightPane->setFont(monoFont);

    diffSplit->addWidget(m_leftPane);
    diffSplit->addWidget(m_rightPane);
    diffSplit->setSizes({400, 400});
    rightLayout->addWidget(diffSplit, 1);

    mainSplit->addWidget(rightWidget);
    mainSplit->setSizes({200, 600});

    // ── Synchronized scrolling ──────────────────────────────────────────
    connect(m_leftPane->verticalScrollBar(), &QScrollBar::valueChanged,
            m_rightPane->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_rightPane->verticalScrollBar(), &QScrollBar::valueChanged,
            m_leftPane->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_leftPane->horizontalScrollBar(), &QScrollBar::valueChanged,
            m_rightPane->horizontalScrollBar(), &QScrollBar::setValue);
    connect(m_rightPane->horizontalScrollBar(), &QScrollBar::valueChanged,
            m_leftPane->horizontalScrollBar(), &QScrollBar::setValue);

    // ── Connections ─────────────────────────────────────────────────────
    connect(m_compareBtn, &QPushButton::clicked, this, &DiffExplorerPanel::onCompareClicked);
    connect(m_fileList, &QListWidget::currentRowChanged, this, &DiffExplorerPanel::onFileSelected);

    // Refresh branch list when git status refreshes
    connect(m_git, &GitService::statusRefreshed, this, &DiffExplorerPanel::refreshBranches);
    refreshBranches();
}

void DiffExplorerPanel::compare(const QString &rev1, const QString &rev2)
{
    m_rev1 = rev1;
    m_rev2 = rev2;

    m_fromCombo->setCurrentText(rev1);
    m_toCombo->setCurrentText(rev2);

    const auto changed = m_git->changedFilesBetween(rev1, rev2);
    m_fileList->clear();
    m_changedPaths.clear();

    for (const auto &cf : changed) {
        QString label;
        switch (cf.status.toLatin1()) {
        case 'A': label = QStringLiteral("[+] "); break;
        case 'D': label = QStringLiteral("[-] "); break;
        case 'M': label = QStringLiteral("[~] "); break;
        case 'R': label = QStringLiteral("[R] "); break;
        default:  label = QStringLiteral("[%1] ").arg(cf.status); break;
        }
        m_fileList->addItem(label + cf.path);
        m_changedPaths.append(cf.path);
    }

    m_summaryLabel->setText(tr("%1 files changed").arg(changed.size()));

    // Show first file diff if available
    if (!m_changedPaths.isEmpty()) {
        m_fileList->setCurrentRow(0);
        showFileDiff(m_changedPaths.first());
    } else {
        m_leftPane->clear();
        m_rightPane->clear();
        m_diffTitle->clear();
    }
}

void DiffExplorerPanel::refreshBranches()
{
    const QString currentFrom = m_fromCombo->currentText();
    const QString currentTo = m_toCombo->currentText();

    m_fromCombo->clear();
    m_toCombo->clear();

    const QStringList branches = m_git->localBranches();
    m_fromCombo->addItems(branches);
    m_toCombo->addItems(branches);

    // Add HEAD as a convenience option
    m_fromCombo->addItem(QStringLiteral("HEAD"));
    m_toCombo->addItem(QStringLiteral("HEAD"));

    // Restore previous selections
    if (!currentFrom.isEmpty())
        m_fromCombo->setCurrentText(currentFrom);
    if (!currentTo.isEmpty())
        m_toCombo->setCurrentText(currentTo);
}

void DiffExplorerPanel::onCompareClicked()
{
    const QString from = m_fromCombo->currentText().trimmed();
    const QString to = m_toCombo->currentText().trimmed();
    if (from.isEmpty() || to.isEmpty())
        return;
    compare(from, to);
}

void DiffExplorerPanel::onFileSelected(int row)
{
    if (row < 0 || row >= m_changedPaths.size())
        return;
    showFileDiff(m_changedPaths.at(row));
}

void DiffExplorerPanel::showFileDiff(const QString &relPath)
{
    m_diffTitle->setText(tr("%1: %2 → %3").arg(relPath, m_rev1, m_rev2));

    const QString gitRoot = m_git->workingDirectory();
    const QString absPath = gitRoot + QLatin1Char('/') + relPath;

    const QString leftContent = m_git->showAtRevision(absPath, m_rev1);
    const QString rightContent = m_git->showAtRevision(absPath, m_rev2);

    m_leftPane->setPlainText(leftContent);
    m_rightPane->setPlainText(rightContent);

    highlightDifferences();
}

void DiffExplorerPanel::highlightDifferences()
{
    // Line-by-line comparison for highlighting
    const QStringList leftLines = m_leftPane->toPlainText().split(QLatin1Char('\n'));
    const QStringList rightLines = m_rightPane->toPlainText().split(QLatin1Char('\n'));

    auto highlight = [](QPlainTextEdit *edit, const QStringList &lines,
                        const QStringList &otherLines, const QColor &color) {
        QList<QTextEdit::ExtraSelection> selections;
        const int count = qMax(lines.size(), otherLines.size());
        for (int i = 0; i < count && i < lines.size(); ++i) {
            const bool differs = (i >= otherLines.size() || lines[i] != otherLines[i]);
            if (differs) {
                QTextEdit::ExtraSelection sel;
                sel.format.setBackground(color);
                sel.format.setProperty(QTextFormat::FullWidthSelection, true);
                sel.cursor = QTextCursor(edit->document()->findBlockByNumber(i));
                selections.append(sel);
            }
        }
        edit->setExtraSelections(selections);
    };

    highlight(m_leftPane, leftLines, rightLines, QColor(80, 30, 30));   // removed (red)
    highlight(m_rightPane, rightLines, leftLines, QColor(30, 80, 30));  // added (green)
}

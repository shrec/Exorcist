#include "searchpanel.h"

#include <QCheckBox>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "searchmatch.h"
#include "searchquery.h"
#include "searchservice.h"

SearchPanel::SearchPanel(SearchService *service, QWidget *parent)
    : QWidget(parent),
      m_rootPath(QDir::currentPath()),
      m_service(service),
      m_input(new QLineEdit(this)),
      m_includeFilter(new QLineEdit(this)),
      m_excludeFilter(new QLineEdit(this)),
      m_regex(new QCheckBox(tr(".*"), this)),
      m_case(new QCheckBox(tr("Aa"), this)),
      m_word(new QCheckBox(tr("\\b"), this)),
      m_searchButton(new QPushButton(tr("Search"), this)),
      m_results(new QTreeWidget(this)),
      m_statusLabel(new QLabel(this))
{
    // ── Search input row ──────────────────────────────────────────────────
    m_input->setPlaceholderText(tr("Search pattern..."));
    m_input->setClearButtonEnabled(true);

    auto *optRow = new QHBoxLayout();
    optRow->setContentsMargins(0, 0, 0, 0);
    optRow->addWidget(m_regex);
    optRow->addWidget(m_case);
    optRow->addWidget(m_word);
    optRow->addStretch();
    optRow->addWidget(m_searchButton);

    // ── File filter rows ──────────────────────────────────────────────────
    m_includeFilter->setPlaceholderText(tr("Include files (e.g. *.cpp;*.h)"));
    m_includeFilter->setClearButtonEnabled(true);
    m_excludeFilter->setPlaceholderText(tr("Exclude files (e.g. build;*.o)"));
    m_excludeFilter->setClearButtonEnabled(true);

    // Tooltips
    m_regex->setToolTip(tr("Use regular expressions"));
    m_case->setToolTip(tr("Match case"));
    m_word->setToolTip(tr("Match whole word"));
    m_includeFilter->setToolTip(tr("Semicolon-separated file patterns to include"));
    m_excludeFilter->setToolTip(tr("Semicolon-separated file/folder patterns to exclude"));

    // ── Results tree ──────────────────────────────────────────────────────
    m_results->setHeaderLabels({tr("File"), tr("Line"), tr("Text")});
    m_results->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_results->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_results->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_results->setRootIsDecorated(true);
    m_results->setUniformRowHeights(true);
    m_results->setAlternatingRowColors(true);

    // ── Status bar ────────────────────────────────────────────────────────
    m_statusLabel->setText(tr("Ready"));

    // ── Layout ────────────────────────────────────────────────────────────
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(m_input);
    layout->addLayout(optRow);
    layout->addWidget(m_includeFilter);
    layout->addWidget(m_excludeFilter);
    layout->addWidget(m_results, 1);
    layout->addWidget(m_statusLabel);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_searchButton, &QPushButton::clicked, this, &SearchPanel::startSearch);
    connect(m_input, &QLineEdit::returnPressed, this, &SearchPanel::startSearch);

    connect(m_service, &SearchService::matchFound, this, [this](const SearchMatch &match) {
        addMatch(match.filePath, match.line, match.column, match.preview);
    });
    connect(m_service, &SearchService::searchFinished, this, [this]() {
        m_searchButton->setEnabled(true);
        QString status = tr("Matching lines: %1  Matching files: %2")
                             .arg(m_matchCount).arg(m_fileCount);
        if (m_matchCount >= 5000)
            status += tr("  (results capped at 5000)");
        m_statusLabel->setText(status);
    });

    // Click / double-click / Enter on a result → navigate
    connect(m_results, &QTreeWidget::itemActivated,
            this, &SearchPanel::onItemActivated);
    connect(m_results, &QTreeWidget::itemClicked,
            this, &SearchPanel::onItemActivated);
}

void SearchPanel::activateSearch()
{
    m_input->setFocus();
    m_input->selectAll();
}

void SearchPanel::startSearch()
{
    const QString pattern = m_input->text().trimmed();
    if (pattern.isEmpty())
        return;

    m_searchButton->setEnabled(false);
    clearResults();

    SearchQuery query;
    query.pattern = pattern;
    query.mode = m_regex->isChecked() ? SearchMode::Regex : SearchMode::Literal;
    query.caseSensitive = m_case->isChecked();
    query.wholeWord = m_word->isChecked();

    m_statusLabel->setText(tr("Searching..."));

    m_service->cancel();
    m_service->startSearch(m_rootPath, query);
}

void SearchPanel::clearResults()
{
    m_results->clear();
    m_matchCount = 0;
    m_fileCount  = 0;
}

void SearchPanel::setRootPath(const QString &path)
{
    m_rootPath = path;
}

void SearchPanel::addMatch(const QString &filePath, int line, int column,
                           const QString &preview)
{
    ++m_matchCount;

    // Show relative path for readability
    const QDir root(m_rootPath);
    const QString relPath = root.relativeFilePath(filePath);

    // Find or create the file group item
    QTreeWidgetItem *parentItem = nullptr;
    for (int i = 0; i < m_results->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_results->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == filePath) {
            parentItem = item;
            break;
        }
    }

    if (!parentItem) {
        ++m_fileCount;
        parentItem = new QTreeWidgetItem(m_results);
        parentItem->setText(0, relPath);
        parentItem->setData(0, Qt::UserRole, filePath);  // absolute path
        QFont bold = parentItem->font(0);
        bold.setBold(true);
        parentItem->setFont(0, bold);
        parentItem->setFirstColumnSpanned(true);
        parentItem->setExpanded(true);
    }

    // Update match count in file header
    const int childCount = parentItem->childCount() + 1;
    parentItem->setText(0, QStringLiteral("%1 (%2)").arg(relPath).arg(childCount));

    // Add the match line
    auto *child = new QTreeWidgetItem(parentItem);
    child->setText(1, QString::number(line));
    child->setText(2, preview.trimmed());
    child->setData(0, Qt::UserRole, filePath);          // absolute path
    child->setData(0, Qt::UserRole + 1, line);          // line number
    child->setData(0, Qt::UserRole + 2, column);        // column number

    // Live status update
    if (m_matchCount % 50 == 0) {
        m_statusLabel->setText(tr("Searching... %1 matches in %2 files")
                                   .arg(m_matchCount).arg(m_fileCount));
    }
}

void SearchPanel::onItemActivated(QTreeWidgetItem *item, int /*column*/)
{
    if (!item)
        return;

    const QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty())
        return;

    // If it's a file header (top-level), open the file at line 1
    // If it's a match (child), open at the specific line
    const int line = item->data(0, Qt::UserRole + 1).toInt();
    const int col  = item->data(0, Qt::UserRole + 2).toInt();
    emit resultActivated(path, qMax(line, 1), col);
}

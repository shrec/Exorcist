#include "searchpanel.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QDir>
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
      m_regex(new QCheckBox(tr("Regex"), this)),
      m_case(new QCheckBox(tr("Case"), this)),
      m_word(new QCheckBox(tr("Word"), this)),
      m_searchButton(new QPushButton(tr("Search"), this)),
      m_results(new QTreeWidget(this))
{
    auto *row = new QHBoxLayout();
    row->addWidget(m_input, 1);
    row->addWidget(m_regex);
    row->addWidget(m_case);
    row->addWidget(m_word);
    row->addWidget(m_searchButton);

    m_results->setHeaderLabels({tr("File"), tr("Line"), tr("Text")});
    m_results->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_results->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_results->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(row);
    layout->addWidget(m_results);
    setLayout(layout);

    connect(m_searchButton, &QPushButton::clicked, this, &SearchPanel::startSearch);
    connect(m_input, &QLineEdit::returnPressed, this, &SearchPanel::startSearch);
    connect(m_service, &SearchService::matchFound, this, [this](const SearchMatch &match) {
        addMatch(match.filePath, match.line, match.column, match.preview);
    });
    connect(m_service, &SearchService::searchFinished, this, [this]() {
        m_searchButton->setEnabled(true);
    });
}

void SearchPanel::startSearch()
{
    const QString pattern = m_input->text().trimmed();
    if (pattern.isEmpty()) {
        return;
    }

    m_searchButton->setEnabled(false);
    clearResults();

    SearchQuery query;
    query.pattern = pattern;
    query.mode = m_regex->isChecked() ? SearchMode::Regex : SearchMode::Literal;
    query.caseSensitive = m_case->isChecked();
    query.wholeWord = m_word->isChecked();

    m_service->cancel();
    m_service->startSearch(m_rootPath, query);
}

void SearchPanel::clearResults()
{
    m_results->clear();
}

void SearchPanel::setRootPath(const QString &path)
{
    m_rootPath = path;
}

void SearchPanel::addMatch(const QString &filePath, int line, int column, const QString &preview)
{
    QTreeWidgetItem *parentItem = nullptr;
    for (int i = 0; i < m_results->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_results->topLevelItem(i);
        if (item->text(0) == filePath) {
            parentItem = item;
            break;
        }
    }

    if (!parentItem) {
        parentItem = new QTreeWidgetItem(m_results, {filePath});
        parentItem->setFirstColumnSpanned(true);
        m_results->addTopLevelItem(parentItem);
    }

    const QString lineText = QString("%1:%2").arg(line).arg(column);
    auto *child = new QTreeWidgetItem(parentItem, {QString(), lineText, preview});
    parentItem->addChild(child);
}

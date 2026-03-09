#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class SearchService;

class SearchPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SearchPanel(SearchService *service, QWidget *parent = nullptr);
    void setRootPath(const QString &path);

    // Focus the search input (called when Ctrl+Shift+F opens the panel)
    void activateSearch();

signals:
    // Emitted when the user clicks a search result
    void resultActivated(const QString &filePath, int line, int column);

private:
    void startSearch();
    void clearResults();
    void addMatch(const QString &filePath, int line, int column, const QString &preview);
    void onItemActivated(QTreeWidgetItem *item, int column);

    QString m_rootPath;
    SearchService *m_service;
    QLineEdit *m_input;
    QLineEdit *m_includeFilter;
    QLineEdit *m_excludeFilter;
    QCheckBox *m_regex;
    QCheckBox *m_case;
    QCheckBox *m_word;
    QPushButton *m_searchButton;
    QTreeWidget *m_results;
    QLabel *m_statusLabel;
    int m_matchCount = 0;
    int m_fileCount  = 0;
};

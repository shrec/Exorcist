#pragma once

#include <QWidget>

class QCheckBox;
class QLineEdit;
class QPushButton;
class QTreeWidget;

class SearchService;

class SearchPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SearchPanel(SearchService *service, QWidget *parent = nullptr);
    void setRootPath(const QString &path);

private:
    void startSearch();
    void clearResults();
    void addMatch(const QString &filePath, int line, int column, const QString &preview);

    QString m_rootPath;
    SearchService *m_service;
    QLineEdit *m_input;
    QCheckBox *m_regex;
    QCheckBox *m_case;
    QCheckBox *m_word;
    QPushButton *m_searchButton;
    QTreeWidget *m_results;
};

#pragma once

#include <QWidget>

class QTreeWidget;
class QTextEdit;
class QToolButton;
class QTreeWidgetItem;

/// Simple panel to browse and edit AI memory files stored in
/// AppDataLocation/memories/.
class MemoryBrowserPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryBrowserPanel(const QString &basePath, QWidget *parent = nullptr);

    void refresh();

private slots:
    void onItemClicked(QTreeWidgetItem *item);
    void saveCurrentFile();
    void deleteCurrentFile();
    void createNewFile();

private:
    void populateTree(const QString &dirPath, QTreeWidgetItem *parentItem);

    QString       m_basePath;
    QTreeWidget  *m_tree;
    QTextEdit    *m_editor;
    QToolButton  *m_saveBtn;
    QToolButton  *m_deleteBtn;
    QToolButton  *m_newBtn;
    QToolButton  *m_refreshBtn;
    QString       m_currentFilePath;
};

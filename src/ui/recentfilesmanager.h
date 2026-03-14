#pragma once

#include <QObject>
#include <QStringList>

class QMenu;

class RecentFilesManager : public QObject
{
    Q_OBJECT
public:
    explicit RecentFilesManager(QObject *parent = nullptr);

    void addFile(const QString &path);
    QStringList recentFiles() const;
    void clear();

    void attachMenu(QMenu *menu);

    static constexpr int MaxRecentFiles = 15;

signals:
    void fileSelected(const QString &path);

private:
    void save() const;
    void load();
    void rebuildMenu();

    QStringList m_files;
    QMenu *m_menu = nullptr;
};

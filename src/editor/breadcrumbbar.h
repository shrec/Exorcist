#pragma once

#include <QWidget>

class QHBoxLayout;

class BreadcrumbBar : public QWidget
{
    Q_OBJECT
public:
    explicit BreadcrumbBar(QWidget *parent = nullptr);

    void setFilePath(const QString &absPath, const QString &rootPath);
    void clear();

signals:
    void segmentClicked(const QString &dirPath);

private:
    QHBoxLayout *m_layout = nullptr;
};

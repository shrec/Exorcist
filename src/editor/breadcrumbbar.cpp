#include "breadcrumbbar.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

BreadcrumbBar::BreadcrumbBar(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(6, 2, 6, 2);
    m_layout->setSpacing(0);
    m_layout->addStretch();

    setFixedHeight(24);
    setStyleSheet(QStringLiteral(
        "BreadcrumbBar { background: rgba(255,255,255,8); }"
        "QPushButton { border:none; padding:0 4px; color:#cccccc; font-size:12px; }"
        "QPushButton:hover { color:#ffffff; text-decoration:underline; }"
        "QLabel { color:#666666; font-size:12px; padding:0 2px; }"));
}

void BreadcrumbBar::setFilePath(const QString &absPath, const QString &rootPath)
{
    clear();

    QString rel = absPath;
    if (!rootPath.isEmpty())
        rel = QDir(rootPath).relativeFilePath(absPath);

    const QStringList parts = rel.split('/', Qt::SkipEmptyParts);

    // Build cumulative paths for each segment
    QString cumulative = rootPath.isEmpty() ? QString() : rootPath;

    for (int i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            auto *sep = new QLabel(QStringLiteral("\u203A"), this);  // ›
            m_layout->insertWidget(m_layout->count() - 1, sep);
        }

        if (!cumulative.isEmpty())
            cumulative += '/';
        cumulative += parts[i];

        auto *btn = new QPushButton(parts[i], this);
        btn->setCursor(Qt::PointingHandCursor);
        const QString dirPath = cumulative;
        connect(btn, &QPushButton::clicked, this, [this, dirPath]() {
            emit segmentClicked(dirPath);
        });
        m_layout->insertWidget(m_layout->count() - 1, btn);
    }
}

void BreadcrumbBar::clear()
{
    while (m_layout->count() > 1) {  // keep the stretch
        auto *item = m_layout->takeAt(0);
        if (item->widget())
            delete item->widget();
        delete item;
    }
}

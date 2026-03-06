#include "referencespanel.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QUrl>
#include <QVBoxLayout>

ReferencesPanel::ReferencesPanel(QWidget *parent)
    : QWidget(parent),
      m_header(new QLabel(tr("References"), this)),
      m_list(new QListWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);
    layout->addWidget(m_header);
    layout->addWidget(m_list, 1);

    connect(m_list, &QListWidget::itemActivated,
            this, [this](QListWidgetItem *item) {
        const int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_refs.size())
            emit referenceActivated(m_refs[idx].path,
                                    m_refs[idx].line,
                                    m_refs[idx].col);
    });
}

void ReferencesPanel::showReferences(const QString &symbol,
                                     const QJsonArray &locations)
{
    m_refs.clear();
    m_list->clear();
    m_header->setText(tr("%1 reference(s) to \"%2\"")
                          .arg(locations.size()).arg(symbol));

    for (const QJsonValue &v : locations) {
        const QJsonObject loc   = v.toObject();
        const QString uri       = loc["uri"].toString();
        const QString filePath  = QUrl(uri).toLocalFile();
        if (filePath.isEmpty()) continue;

        const QJsonObject start = loc["range"].toObject()["start"].toObject();
        const int line = start["line"].toInt();
        const int col  = start["character"].toInt();

        const int idx = m_refs.size();
        m_refs.append({filePath, line, col});

        const QString display =
            QStringLiteral("%1:%2:%3")
                .arg(QFileInfo(filePath).fileName())
                .arg(line + 1)
                .arg(col + 1);

        auto *item = new QListWidgetItem(display, m_list);
        item->setData(Qt::UserRole, idx);
        item->setToolTip(filePath);
    }
}

void ReferencesPanel::clear()
{
    m_refs.clear();
    m_list->clear();
    m_header->setText(tr("References"));
}

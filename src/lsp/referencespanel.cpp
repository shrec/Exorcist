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
    m_header->setStyleSheet(QStringLiteral(
        "QLabel { color: #858585; font-size: 11px; padding: 4px 4px 2px 4px; }"));

    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { background: #1e1e1e; color: #d4d4d4; border: none; font-size: 12px; }"
        "QListWidget::item { padding: 3px 6px; border: none; }"
        "QListWidget::item:hover { background: #2a2d2e; }"
        "QListWidget::item:selected { background: #094771; color: #ffffff; }"
        "QScrollBar:vertical { background: #1e1e1e; width: 10px; border: none; }"
        "QScrollBar::handle:vertical { background: #424242; min-height: 20px; border-radius: 5px; }"
        "QScrollBar::handle:vertical:hover { background: #686868; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
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

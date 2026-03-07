#pragma once

#include <QAction>
#include <QObject>
#include <QPushButton>
#include <QStringList>
#include <QListWidget>
#include <QVBoxLayout>
#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// PromptFilePicker — UI for selecting .prompt.md files from the slash menu.
//
// Displays discovered prompt files with their descriptions. When a prompt
// is selected, emits promptSelected with the resolved body text.
// ─────────────────────────────────────────────────────────────────────────────

struct PromptFileEntry {
    QString name;        // display name from YAML
    QString description; // description from YAML
    QString filePath;    // absolute path
    QString body;        // resolved body content
    QString mode;        // ask, edit, agent
    QStringList tools;   // required tools
};

class PromptFilePicker : public QWidget
{
    Q_OBJECT

public:
    explicit PromptFilePicker(QWidget *parent = nullptr) : QWidget(parent)
    {
        setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
        setMinimumWidth(350);
        setMaximumHeight(400);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);

        m_list = new QListWidget(this);
        m_list->setStyleSheet(QStringLiteral(
            "QListWidget { border: 1px solid palette(mid); border-radius: 4px; }"
            "QListWidget::item { padding: 6px; }"
            "QListWidget::item:hover { background: palette(highlight); color: palette(highlighted-text); }"));
        layout->addWidget(m_list);

        connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
            const int idx = m_list->row(item);
            if (idx >= 0 && idx < m_entries.size()) {
                emit promptSelected(m_entries[idx]);
                hide();
            }
        });
    }

    void setEntries(const QList<PromptFileEntry> &entries)
    {
        m_entries = entries;
        m_list->clear();
        for (const auto &e : entries) {
            const QString text = e.description.isEmpty()
                ? e.name
                : QStringLiteral("%1 — %2").arg(e.name, e.description);
            auto *item = new QListWidgetItem(text);
            item->setToolTip(e.filePath);
            m_list->addItem(item);
        }
    }

    void showBelow(QWidget *anchor)
    {
        if (!anchor) return;
        const QPoint pos = anchor->mapToGlobal(QPoint(0, -height()));
        move(pos);
        show();
        m_list->setFocus();
    }

signals:
    void promptSelected(const PromptFileEntry &entry);

private:
    QListWidget *m_list;
    QList<PromptFileEntry> m_entries;
};

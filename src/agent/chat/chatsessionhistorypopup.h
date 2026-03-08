#pragma once

#include <QDateTime>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "chatthemetokens.h"

// ── ChatSessionHistoryPopup ──────────────────────────────────────────────────
//
// A popup widget showing previous chat sessions with search, rename, delete.
// Mirrors VS Code's chat session history panel.

class ChatSessionHistoryPopup : public QWidget
{
    Q_OBJECT

public:
    struct SessionEntry
    {
        QString   id;
        QString   title;
        QDateTime timestamp;
        int       turnCount = 0;
    };

    explicit ChatSessionHistoryPopup(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_DeleteOnClose, false);
        setFixedWidth(320);
        setMaximumHeight(400);
        setStyleSheet(
            QStringLiteral("ChatSessionHistoryPopup {"
                          "  background:%1; border:1px solid %2; border-radius:6px;"
                          "}")
                .arg(ChatTheme::InputBg, ChatTheme::Border));

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(4);

        // Title
        auto *title = new QLabel(tr("Chat History"), this);
        title->setStyleSheet(
            QStringLiteral("color:%1; font-weight:600; font-size:13px;")
                .arg(ChatTheme::FgPrimary));
        layout->addWidget(title);

        // Search box
        m_searchBox = new QLineEdit(this);
        m_searchBox->setPlaceholderText(tr("Search sessions\u2026"));
        m_searchBox->setClearButtonEnabled(true);
        m_searchBox->setStyleSheet(
            QStringLiteral("QLineEdit {"
                          "  background:%1; color:%2; border:1px solid %3;"
                          "  border-radius:4px; padding:4px 8px; font-size:12px;"
                          "}"
                          "QLineEdit:focus { border-color:%4; }")
                .arg(ChatTheme::PanelBg, ChatTheme::FgPrimary,
                     ChatTheme::Border, ChatTheme::AccentFg));
        connect(m_searchBox, &QLineEdit::textChanged,
                this, &ChatSessionHistoryPopup::filterList);
        layout->addWidget(m_searchBox);

        // Session list
        m_list = new QListWidget(this);
        m_list->setStyleSheet(
            QStringLiteral("QListWidget {"
                          "  background:transparent; border:none;"
                          "}"
                          "QListWidget::item {"
                          "  color:%1; padding:6px 8px; border-radius:4px;"
                          "}"
                          "QListWidget::item:hover { background:%2; }"
                          "QListWidget::item:selected { background:%3; color:%4; }")
                .arg(ChatTheme::FgPrimary, ChatTheme::InputBg,
                     ChatTheme::AccentFg, ChatTheme::ButtonFg));
        connect(m_list, &QListWidget::itemClicked,
                this, [this](QListWidgetItem *item) {
            const QString id = item->data(Qt::UserRole).toString();
            emit sessionSelected(id);
            hide();
        });

        m_list->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_list, &QWidget::customContextMenuRequested,
                this, [this](const QPoint &pos) {
            auto *item = m_list->itemAt(pos);
            if (!item) return;
            const QString id = item->data(Qt::UserRole).toString();
            const QString currentTitle = item->text();

            QMenu menu(this);
            menu.setStyleSheet(
                QStringLiteral("QMenu { background:%1; border:1px solid %2; "
                              "border-radius:4px; padding:4px; }"
                              "QMenu::item { color:%3; padding:4px 16px; "
                              "border-radius:3px; }"
                              "QMenu::item:selected { background:%4; }")
                    .arg(ChatTheme::InputBg, ChatTheme::Border,
                         ChatTheme::FgPrimary, ChatTheme::AccentFg));

            QAction *renameAct = menu.addAction(tr("Rename"));
            QAction *deleteAct = menu.addAction(tr("Delete"));
            deleteAct->setIcon(QIcon());

            QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
            if (chosen == renameAct) {
                bool ok = false;
                const QString newTitle = QInputDialog::getText(
                    this, tr("Rename Session"), tr("New title:"),
                    QLineEdit::Normal, currentTitle, &ok);
                if (ok && !newTitle.trimmed().isEmpty()) {
                    item->setText(newTitle.trimmed());
                    emit sessionRenamed(id, newTitle.trimmed());
                }
            } else if (chosen == deleteAct) {
                const int row = m_list->row(item);
                delete m_list->takeItem(row);
                m_sessions.erase(
                    std::remove_if(m_sessions.begin(), m_sessions.end(),
                        [&id](const SessionEntry &e) { return e.id == id; }),
                    m_sessions.end());
                emit sessionDeleted(id);
            }
        });

        layout->addWidget(m_list, 1);

        // New session button
        auto *newBtn = new QToolButton(this);
        newBtn->setText(tr("+ New Session"));
        newBtn->setStyleSheet(
            QStringLiteral("QToolButton { color:%1; background:%2; border:none;"
                          " padding:6px 12px; border-radius:4px; font-size:12px;"
                          " width:100%; }"
                          "QToolButton:hover { background:%3; }")
                .arg(ChatTheme::ButtonFg, ChatTheme::ButtonBg, ChatTheme::ButtonHover));
        newBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(newBtn, &QToolButton::clicked, this, [this]() {
            emit newSessionRequested();
            hide();
        });
        layout->addWidget(newBtn);
    }

    void setSessions(const QList<SessionEntry> &sessions)
    {
        m_sessions = sessions;
        rebuildList();
    }

    void showAt(const QPoint &globalPos)
    {
        move(globalPos);
        show();
        m_searchBox->setFocus();
        m_searchBox->clear();
    }

signals:
    void sessionSelected(const QString &sessionId);
    void sessionDeleted(const QString &sessionId);
    void sessionRenamed(const QString &sessionId, const QString &newTitle);
    void newSessionRequested();

private:
    void rebuildList()
    {
        m_list->clear();
        const auto filter = m_searchBox->text().trimmed();
        for (const auto &s : m_sessions) {
            if (!filter.isEmpty()
                && !s.title.contains(filter, Qt::CaseInsensitive))
                continue;

            auto *item = new QListWidgetItem(m_list);
            const auto subtitle = s.timestamp.toString(QStringLiteral("MMM dd, hh:mm"))
                + QStringLiteral(" \u00B7 ")
                + tr("%1 turns").arg(s.turnCount);
            item->setText(s.title.isEmpty()
                ? tr("Untitled session") : s.title);
            item->setToolTip(subtitle);
            item->setData(Qt::UserRole, s.id);
        }
    }

    void filterList()
    {
        rebuildList();
    }

    QLineEdit            *m_searchBox = nullptr;
    QListWidget          *m_list      = nullptr;
    QList<SessionEntry>   m_sessions;
};

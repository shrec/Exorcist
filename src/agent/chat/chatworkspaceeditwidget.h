#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "chatcontentpart.h"
#include "chatthemetokens.h"

// ── ChatWorkspaceEditWidget ───────────────────────────────────────────────────
//
// Renders a workspace-edit summary: N files changed, with per-file expandable
// diff-stat rows.  Each file has Keep / Undo buttons.  Mirrors VS Code's
// "Changes" section in Copilot Chat.

class ChatWorkspaceEditWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWorkspaceEditWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 4, 0, 4);
        root->setSpacing(0);

        m_card = new QWidget(this);
        m_card->setStyleSheet(
            QStringLiteral("background:%1; border-radius:4px;").arg(ChatTheme::PanelBg));

        m_cardLayout = new QVBoxLayout(m_card);
        m_cardLayout->setContentsMargins(10, 8, 10, 8);
        m_cardLayout->setSpacing(4);

        // Header row: "Changes (N files)"
        auto *hdr = new QHBoxLayout;
        hdr->setContentsMargins(0, 0, 0, 0);
        hdr->setSpacing(6);

        m_headerLabel = new QLabel(this);
        m_headerLabel->setStyleSheet(
            QStringLiteral("color:%1; font-weight:600; font-size:12px;")
                .arg(ChatTheme::FgPrimary));
        hdr->addWidget(m_headerLabel, 1);

        m_keepAllBtn = new QToolButton(this);
        m_keepAllBtn->setText(tr("Keep All"));
        m_keepAllBtn->setStyleSheet(buttonStyle(ChatTheme::ButtonBg,
                                                 ChatTheme::ButtonFg,
                                                 ChatTheme::ButtonHover));
        connect(m_keepAllBtn, &QToolButton::clicked, this, [this]() {
            emit keepAll();
        });
        hdr->addWidget(m_keepAllBtn);

        m_undoAllBtn = new QToolButton(this);
        m_undoAllBtn->setText(tr("Undo All"));
        m_undoAllBtn->setStyleSheet(buttonStyle(ChatTheme::SecondaryBtnBg,
                                                 ChatTheme::SecondaryBtnFg,
                                                 ChatTheme::SecondaryBtnHover));
        connect(m_undoAllBtn, &QToolButton::clicked, this, [this]() {
            emit undoAll();
        });
        hdr->addWidget(m_undoAllBtn);

        m_cardLayout->addLayout(hdr);

        // File list container
        m_fileListWidget = new QWidget(this);
        m_fileListLayout = new QVBoxLayout(m_fileListWidget);
        m_fileListLayout->setContentsMargins(0, 2, 0, 0);
        m_fileListLayout->setSpacing(2);
        m_cardLayout->addWidget(m_fileListWidget);

        root->addWidget(m_card);
    }

    void setEditedFiles(const QVector<ChatContentPart::EditedFile> &files)
    {
        // Clear existing
        while (auto *item = m_fileListLayout->takeAt(0)) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }

        int total = files.size();
        m_headerLabel->setText(
            tr("Changes (%1 file%2)").arg(total).arg(total == 1 ? "" : "s"));

        for (int i = 0; i < files.size(); ++i) {
            auto *row = createFileRow(files[i], i);
            m_fileListLayout->addWidget(row);
        }
    }

signals:
    void fileClicked(int fileIndex, const QString &path);
    void keepFile(int fileIndex, const QString &path);
    void undoFile(int fileIndex, const QString &path);
    void keepAll();
    void undoAll();

private:
    QWidget *createFileRow(const ChatContentPart::EditedFile &file, int index)
    {
        auto *container = new QWidget(this);
        auto *containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        auto *row = new QWidget(container);
        auto *rl = new QHBoxLayout(row);
        rl->setContentsMargins(4, 2, 4, 2);
        rl->setSpacing(6);

        // Action icon
        QChar actionChar;
        QString actionColor;
        switch (file.action) {
        case ChatContentPart::EditedFile::Action::Modified:
            actionChar = QLatin1Char('M');
            actionColor = ChatTheme::DiffModified;
            break;
        case ChatContentPart::EditedFile::Action::Created:
            actionChar = QLatin1Char('A');
            actionColor = ChatTheme::DiffAdded;
            break;
        case ChatContentPart::EditedFile::Action::Deleted:
            actionChar = QLatin1Char('D');
            actionColor = ChatTheme::DiffRemoved;
            break;
        case ChatContentPart::EditedFile::Action::Renamed:
            actionChar = QLatin1Char('R');
            actionColor = ChatTheme::DiffModified;
            break;
        }
        auto *actionLabel = new QLabel(actionChar, row);
        actionLabel->setFixedWidth(16);
        actionLabel->setAlignment(Qt::AlignCenter);
        actionLabel->setStyleSheet(
            QStringLiteral("color:%1; font-weight:bold; font-size:11px;")
                .arg(actionColor));
        rl->addWidget(actionLabel);

        // Diff expand toggle (only if diff available)
        QToolButton *expandBtn = nullptr;
        QTextBrowser *diffBrowser = nullptr;
        if (!file.diffPreview.isEmpty()) {
            expandBtn = new QToolButton(row);
            expandBtn->setText(QStringLiteral(">"));
            expandBtn->setCheckable(true);
            expandBtn->setChecked(false);
            expandBtn->setFixedSize(16, 16);
            expandBtn->setStyleSheet(
                QStringLiteral("QToolButton { color:%1; background:transparent;"
                              " border:none; font-size:10px; }"
                              "QToolButton:checked { color:%2; }")
                    .arg(ChatTheme::FgDimmed, ChatTheme::FgPrimary));
            rl->addWidget(expandBtn);
        }

        // Filename (clickable)
        auto *nameBtn = new QToolButton(row);
        nameBtn->setText(file.filePath.section(QLatin1Char('/'), -1));
        nameBtn->setToolTip(file.filePath);
        nameBtn->setStyleSheet(
            QStringLiteral("QToolButton { color:%1; background:transparent;"
                          " border:none; font-size:12px; text-align:left; }"
                          "QToolButton:hover { color:%2; text-decoration:underline; }")
                .arg(ChatTheme::FgPrimary, ChatTheme::LinkFg));
        const auto path = file.filePath;
        connect(nameBtn, &QToolButton::clicked, this, [this, index, path]() {
            emit fileClicked(index, path);
        });
        rl->addWidget(nameBtn, 1);

        // Insertion/deletion count
        if (file.insertions > 0 || file.deletions > 0) {
            auto *stats = new QLabel(row);
            stats->setText(QStringLiteral("<span style='color:%1'>+%2</span>"
                                         " <span style='color:%3'>-%4</span>")
                .arg(ChatTheme::DiffAdded).arg(file.insertions)
                .arg(ChatTheme::DiffRemoved).arg(file.deletions));
            stats->setStyleSheet(QStringLiteral("font-size:11px;"));
            rl->addWidget(stats);
        }

        // State badge
        auto *state = new QLabel(row);
        state->setStyleSheet(
            QStringLiteral("color:%1; font-size:10px;").arg(ChatTheme::FgDimmed));
        switch (file.state) {
        case ChatContentPart::EditedFile::State::Pending:
            state->setText(tr("pending"));
            break;
        case ChatContentPart::EditedFile::State::Applied:
            state->setText(QStringLiteral("\u2713 ") + tr("applied"));
            break;
        case ChatContentPart::EditedFile::State::Kept:
            state->setText(QStringLiteral("\u2713 ") + tr("kept"));
            break;
        case ChatContentPart::EditedFile::State::Undone:
            state->setText(tr("undone"));
            break;
        }
        rl->addWidget(state);

        // Keep / Undo per-file
        auto *keepBtn = new QToolButton(row);
        keepBtn->setText(tr("Keep"));
        keepBtn->setStyleSheet(smallButtonStyle());
        connect(keepBtn, &QToolButton::clicked, this, [this, index, path]() {
            emit keepFile(index, path);
        });
        rl->addWidget(keepBtn);

        auto *undoBtn = new QToolButton(row);
        undoBtn->setText(tr("Undo"));
        undoBtn->setStyleSheet(smallButtonStyle());
        connect(undoBtn, &QToolButton::clicked, this, [this, index, path]() {
            emit undoFile(index, path);
        });
        rl->addWidget(undoBtn);

        row->setStyleSheet(
            QStringLiteral("QWidget { background:%1; } QWidget:hover { background:%2; }")
                .arg(QStringLiteral("transparent"), ChatTheme::InputBg));

        containerLayout->addWidget(row);

        // ── Inline diff preview (collapsible) ─────────────────────────────
        if (!file.diffPreview.isEmpty() && expandBtn) {
            diffBrowser = new QTextBrowser(container);
            diffBrowser->setOpenLinks(false);
            diffBrowser->setFrameShape(QFrame::NoFrame);
            diffBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            diffBrowser->setStyleSheet(
                QStringLiteral("QTextBrowser { background:%1; color:%2;"
                              " font-family:'Cascadia Code','Fira Code',Consolas,monospace;"
                              " font-size:12px; padding:4px 8px; }")
                    .arg(ChatTheme::CodeBg, ChatTheme::FgPrimary));

            diffBrowser->setHtml(formatDiffHtml(file.diffPreview));
            diffBrowser->setVisible(false);

            // Size to content
            diffBrowser->document()->adjustSize();
            int docH = static_cast<int>(diffBrowser->document()->size().height()) + 8;
            diffBrowser->setFixedHeight(qMin(docH, 300));

            containerLayout->addWidget(diffBrowser);

            connect(expandBtn, &QToolButton::toggled, this,
                [expandBtn, diffBrowser](bool checked) {
                    expandBtn->setText(checked ? QStringLiteral("v") : QStringLiteral(">"));
                    diffBrowser->setVisible(checked);
                });
        }

        return container;
    }

    static QString buttonStyle(const QString &bg, const QString &fg, const QString &hover)
    {
        return QStringLiteral(
            "QToolButton { background:%1; color:%2; padding:2px 8px;"
            " border-radius:3px; font-size:11px; border:none; }"
            "QToolButton:hover { background:%3; }")
            .arg(bg, fg, hover);
    }

    static QString smallButtonStyle()
    {
        return QStringLiteral(
            "QToolButton { color:%1; background:transparent; border:none;"
            " font-size:11px; padding:1px 4px; }"
            "QToolButton:hover { color:%2; }")
            .arg(ChatTheme::FgDimmed, ChatTheme::FgPrimary);
    }

    static QString formatDiffHtml(const QString &diffText)
    {
        QString html = QStringLiteral("<pre style='margin:0; white-space:pre-wrap;'>");
        const auto lines = diffText.split(QLatin1Char('\n'));
        for (const auto &line : lines) {
            QString escaped = line.toHtmlEscaped();
            if (line.startsWith(QLatin1Char('+'))) {
                html += QStringLiteral("<span style='background:%1; color:%2;'>%3</span>\n")
                    .arg(ChatTheme::DiffInsertBg, ChatTheme::DiffInsertFg, escaped);
            } else if (line.startsWith(QLatin1Char('-'))) {
                html += QStringLiteral("<span style='background:%1; color:%2;'>%3</span>\n")
                    .arg(ChatTheme::DiffDeleteBg, ChatTheme::DiffDeleteFg, escaped);
            } else if (line.startsWith(QLatin1String("@@"))) {
                html += QStringLiteral("<span style='color:%1;'>%2</span>\n")
                    .arg(ChatTheme::FgDimmed, escaped);
            } else {
                html += escaped + QLatin1Char('\n');
            }
        }
        html += QStringLiteral("</pre>");
        return html;
    }

    QWidget     *m_card           = nullptr;
    QVBoxLayout *m_cardLayout     = nullptr;
    QLabel      *m_headerLabel    = nullptr;
    QToolButton *m_keepAllBtn     = nullptr;
    QToolButton *m_undoAllBtn     = nullptr;
    QWidget     *m_fileListWidget = nullptr;
    QVBoxLayout *m_fileListLayout = nullptr;
};

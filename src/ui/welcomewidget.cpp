#include "welcomewidget.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

WelcomeWidget::WelcomeWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // Center the content vertically and horizontally
    root->addStretch(2);

    auto *center = new QVBoxLayout;
    center->setAlignment(Qt::AlignHCenter);
    center->setSpacing(16);

    // ── Branding ──────────────────────────────────────────────────────────
    auto *title = new QLabel(tr("Exorcist IDE"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral(
        "font-size: 32px; font-weight: bold; color: #e0e0e0;"));
    center->addWidget(title);

    auto *subtitle = new QLabel(tr("Lightweight. Modular. Yours."), this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QStringLiteral(
        "font-size: 14px; color: #888; margin-bottom: 24px;"));
    center->addWidget(subtitle);

    // ── Action buttons ────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;
    btnRow->setAlignment(Qt::AlignCenter);
    btnRow->setSpacing(12);

    auto makeBtn = [this](const QString &text) {
        auto *btn = new QPushButton(text, this);
        btn->setFixedHeight(36);
        btn->setMinimumWidth(160);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #2d2d30; border: 1px solid #3e3e42; "
            "border-radius: 4px; padding: 6px 20px; color: #e0e0e0; font-size: 13px; }"
            "QPushButton:hover { background: #3e3e42; border-color: #007acc; }"));
        return btn;
    };

    auto *openBtn = makeBtn(tr("Open Folder..."));
    auto *newBtn  = makeBtn(tr("New Project..."));
    btnRow->addWidget(openBtn);
    btnRow->addWidget(newBtn);
    center->addLayout(btnRow);

    connect(openBtn, &QPushButton::clicked, this, &WelcomeWidget::openFolderBrowseRequested);
    connect(newBtn,  &QPushButton::clicked, this, &WelcomeWidget::newProjectRequested);

    // ── Recent folders ────────────────────────────────────────────────────
    auto *recentLabel = new QLabel(tr("Recent"), this);
    recentLabel->setStyleSheet(QStringLiteral(
        "font-size: 13px; color: #888; margin-top: 16px;"));
    recentLabel->setAlignment(Qt::AlignCenter);
    center->addWidget(recentLabel);

    m_recentList = new QListWidget(this);
    m_recentList->setMaximumWidth(480);
    m_recentList->setMinimumWidth(400);
    m_recentList->setFrameShape(QFrame::NoFrame);
    m_recentList->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { padding: 8px 12px; color: #e0e0e0; "
        "  border-bottom: 1px solid #2d2d30; }"
        "QListWidget::item:hover { background: #2d2d30; }"));
    m_recentList->setCursor(Qt::PointingHandCursor);
    center->addWidget(m_recentList, 0, Qt::AlignHCenter);

    connect(m_recentList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty())
            emit openFolderRequested(path);
    });

    root->addLayout(center);
    root->addStretch(3);

    refreshRecent();
}

void WelcomeWidget::refreshRecent()
{
    m_recentList->clear();

    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    const QStringList folders = s.value(QStringLiteral("recentFolders")).toStringList();

    for (const QString &folder : folders) {
        if (!QDir(folder).exists())
            continue;
        const QString name = QFileInfo(folder).fileName();
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(name, folder));
        item->setData(Qt::UserRole, folder);
        m_recentList->addItem(item);
    }

    // Hide the list and label if empty
    m_recentList->setVisible(m_recentList->count() > 0);
}

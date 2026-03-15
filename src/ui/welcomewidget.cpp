#include "welcomewidget.h"

#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

// ── Helpers ──────────────────────────────────────────────────────────────────

static QLabel *makeLink(const QString &icon, const QString &text, QWidget *parent)
{
    Q_UNUSED(icon)
    auto *lbl = new QLabel(text, parent);
    lbl->setCursor(Qt::PointingHandCursor);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: #3794ff; font-size: 13px; padding: 3px 0; }"
        "QLabel:hover { color: #4fc3f7; text-decoration: underline; }"));
    return lbl;
}

static QLabel *makeSectionHeader(const QString &text, QWidget *parent)
{
    auto *lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "font-size: 14px; font-weight: 600; color: #cccccc; "
        "margin-top: 8px; margin-bottom: 4px;"));
    return lbl;
}

// ── WelcomeWidget ────────────────────────────────────────────────────────────

WelcomeWidget::WelcomeWidget(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet(QStringLiteral("WelcomeWidget { background: #1e1e1e; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Vertical centering
    root->addStretch(2);

    // ── Content area: two-column layout ───────────────────────────────────
    auto *contentRow = new QHBoxLayout;
    contentRow->setSpacing(60);
    contentRow->setAlignment(Qt::AlignCenter);

    // ── LEFT COLUMN: Branding + Start + Recent ────────────────────────────
    auto *leftCol = new QVBoxLayout;
    leftCol->setSpacing(4);

    auto *title = new QLabel(tr("Exorcist IDE"), this);
    title->setStyleSheet(QStringLiteral(
        "font-size: 26px; font-weight: 300; color: #cccccc; margin-bottom: 2px;"));
    leftCol->addWidget(title);

    auto *subtitle = new QLabel(tr("Lightweight. Modular. Yours."), this);
    subtitle->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #888; margin-bottom: 20px;"));
    leftCol->addWidget(subtitle);

    // ── Start section ─────────────────────────────────────────────────
    leftCol->addWidget(makeSectionHeader(tr("Start"), this));

    auto *lnkNewFile = makeLink(QString(), tr("New File..."), this);
    auto *lnkOpenFile = makeLink(QString(), tr("Open File..."), this);
    auto *lnkOpenFolder = makeLink(QString(), tr("Open Folder..."), this);
    auto *lnkNewProject = makeLink(QString(), tr("New Project..."), this);

    leftCol->addWidget(lnkNewFile);
    leftCol->addWidget(lnkOpenFile);
    leftCol->addWidget(lnkOpenFolder);
    leftCol->addWidget(lnkNewProject);

    connect(lnkOpenFolder, &QLabel::linkActivated, this, &WelcomeWidget::openFolderBrowseRequested);
    connect(lnkNewProject, &QLabel::linkActivated, this, &WelcomeWidget::newProjectRequested);

    // Use mouse press for plain QLabels (no <a> markup)
    lnkNewFile->installEventFilter(this);
    lnkOpenFile->installEventFilter(this);
    lnkOpenFolder->installEventFilter(this);
    lnkNewProject->installEventFilter(this);

    lnkNewFile->setProperty("action", QStringLiteral("newFile"));
    lnkOpenFile->setProperty("action", QStringLiteral("openFile"));
    lnkOpenFolder->setProperty("action", QStringLiteral("openFolder"));
    lnkNewProject->setProperty("action", QStringLiteral("newProject"));

    // ── Recent section ────────────────────────────────────────────────
    m_recentLabel = makeSectionHeader(tr("Recent"), this);
    m_recentLabel->setStyleSheet(m_recentLabel->styleSheet()
        + QStringLiteral(" margin-top: 18px;"));
    leftCol->addWidget(m_recentLabel);

    m_recentList = new QListWidget(this);
    m_recentList->setFixedWidth(420);
    m_recentList->setMaximumHeight(200);
    m_recentList->setFrameShape(QFrame::NoFrame);
    m_recentList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_recentList->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { padding: 4px 0; color: #3794ff; font-size: 13px; }"
        "QListWidget::item:hover { color: #4fc3f7; background: transparent; }"));
    m_recentList->setCursor(Qt::PointingHandCursor);
    leftCol->addWidget(m_recentList);

    connect(m_recentList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty())
            emit openFolderRequested(path);
    });

    leftCol->addStretch();
    contentRow->addLayout(leftCol);

    // ── RIGHT COLUMN: Getting Started ─────────────────────────────────────
    auto *rightCol = new QVBoxLayout;
    rightCol->setSpacing(8);

    rightCol->addWidget(makeSectionHeader(tr("Getting Started"), this));

    auto makeCard = [this](const QString &title, const QString &desc) {
        auto *card = new QWidget(this);
        card->setFixedWidth(320);
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet(QStringLiteral(
            "QWidget { background: #2d2d30; border: 1px solid #3e3e42; "
            "border-radius: 6px; padding: 12px; }"
            "QWidget:hover { border-color: #007acc; }"));

        auto *lay = new QVBoxLayout(card);
        lay->setContentsMargins(12, 10, 12, 10);
        lay->setSpacing(4);

        auto *t = new QLabel(title, card);
        t->setStyleSheet(QStringLiteral(
            "font-size: 13px; font-weight: 600; color: #e0e0e0; "
            "background: transparent; border: none;"));
        lay->addWidget(t);

        auto *d = new QLabel(desc, card);
        d->setStyleSheet(QStringLiteral(
            "font-size: 12px; color: #888; background: transparent; border: none;"));
        d->setWordWrap(true);
        lay->addWidget(d);

        return card;
    };

    rightCol->addWidget(makeCard(
        tr("Get Started with Exorcist"),
        tr("Learn the basics and start coding")));
    rightCol->addWidget(makeCard(
        tr("Keyboard Shortcuts"),
        tr("Master the editor with essential shortcuts")));
    rightCol->addWidget(makeCard(
        tr("Configure Plugins"),
        tr("Enable AI providers, language packs, and tools")));
    rightCol->addWidget(makeCard(
        tr("Explore the Terminal"),
        tr("Integrated terminal with full PTY support")));

    rightCol->addStretch();
    contentRow->addLayout(rightCol);

    root->addLayout(contentRow);
    root->addStretch(3);

    refreshRecent();
}

bool WelcomeWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        const QString action = obj->property("action").toString();
        if (action == QLatin1String("openFolder"))
            emit openFolderBrowseRequested();
        else if (action == QLatin1String("newProject"))
            emit newProjectRequested();
        else if (action == QLatin1String("newFile"))
            emit newFileRequested();
        else if (action == QLatin1String("openFile"))
            emit openFileRequested();
        return true;
    }
    return QWidget::eventFilter(obj, event);
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
        const QString display = QStringLiteral("%1   %2").arg(name, folder);
        auto *item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, folder);
        m_recentList->addItem(item);
    }

    const bool hasRecent = m_recentList->count() > 0;
    m_recentList->setVisible(hasRecent);
    m_recentLabel->setVisible(hasRecent);
}

#include "welcomewidget.h"

#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
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
    // Plain QWidget subclasses ignore type-selector QSS background for
    // their own paint unless WA_StyledBackground is set.  Without this,
    // the widget falls through to a Fusion-default white background even
    // when the qApp dark theme is active (the visible "white welcome page
    // on dark theme" bug).
    setAttribute(Qt::WA_StyledBackground, true);
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

    m_recentContainer = new QWidget(this);
    m_recentContainer->setFixedWidth(420);
    m_recentLayout = new QVBoxLayout(m_recentContainer);
    m_recentLayout->setContentsMargins(0, 0, 0, 0);
    m_recentLayout->setSpacing(2);
    leftCol->addWidget(m_recentContainer);

    // Empty-state placeholder (created up-front, toggled in refreshRecent).
    m_recentEmpty = new QLabel(tr("No recent folders"), this);
    m_recentEmpty->setStyleSheet(QStringLiteral(
        "color: #707070; font-size: 12px; font-style: italic; padding: 4px 0;"));
    m_recentLayout->addWidget(m_recentEmpty);

    // Clear Recent link (hidden when list empty).
    m_clearRecent = new QLabel(tr("Clear Recent"), this);
    m_clearRecent->setCursor(Qt::PointingHandCursor);
    m_clearRecent->setStyleSheet(QStringLiteral(
        "QLabel { color: #808080; font-size: 12px; padding: 8px 0 0 0; }"
        "QLabel:hover { color: #4fc3f7; text-decoration: underline; }"));
    m_clearRecent->installEventFilter(this);
    m_clearRecent->setProperty("action", QStringLiteral("clearRecent"));
    leftCol->addWidget(m_clearRecent);

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
        if (action == QLatin1String("openFolder")) {
            emit openFolderBrowseRequested();
            return true;
        }
        if (action == QLatin1String("newProject")) {
            emit newProjectRequested();
            return true;
        }
        if (action == QLatin1String("newFile")) {
            emit newFileRequested();
            return true;
        }
        if (action == QLatin1String("openFile")) {
            emit openFileRequested();
            return true;
        }
        if (action == QLatin1String("clearRecent")) {
            QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
            s.remove(QStringLiteral("recentFolders"));
            refreshRecent();
            return true;
        }
        if (action == QLatin1String("openRecent")) {
            const QString path = obj->property("path").toString();
            if (!path.isEmpty())
                emit openFolderRequested(path);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void WelcomeWidget::refreshRecent()
{
    // Wipe all dynamic entry rows, but keep m_recentEmpty + m_clearRecent
    // (which are owned by m_recentLayout permanently).
    QList<QWidget *> toRemove;
    for (int i = 0; i < m_recentLayout->count(); ++i) {
        QLayoutItem *it = m_recentLayout->itemAt(i);
        if (!it)
            continue;
        QWidget *w = it->widget();
        if (!w)
            continue;
        if (w == m_recentEmpty || w == m_clearRecent)
            continue;
        toRemove.append(w);
    }
    for (QWidget *w : toRemove) {
        m_recentLayout->removeWidget(w);
        w->deleteLater();
    }

    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    QStringList folders = s.value(QStringLiteral("recentFolders")).toStringList();

    // Drop dead paths and cap at 10 (most-recent-first).
    QStringList valid;
    valid.reserve(folders.size());
    for (const QString &folder : folders) {
        if (QDir(folder).exists())
            valid.append(folder);
        if (valid.size() >= 10)
            break;
    }

    const bool hasRecent = !valid.isEmpty();
    m_recentEmpty->setVisible(!hasRecent);
    m_clearRecent->setVisible(hasRecent);

    if (!hasRecent)
        return;

    // Insert each entry above the empty/clear rows.
    const int insertBase = m_recentLayout->indexOf(m_recentEmpty);
    int insertAt = (insertBase >= 0) ? insertBase : 0;

    const int maxPathPx = 400;
    QFontMetrics fmPath(font());

    for (const QString &folder : valid) {
        const QString name = QFileInfo(folder).fileName().isEmpty()
            ? folder
            : QFileInfo(folder).fileName();

        auto *entry = new QWidget(m_recentContainer);
        auto *lay = new QVBoxLayout(entry);
        lay->setContentsMargins(0, 2, 0, 2);
        lay->setSpacing(0);

        auto *nameLbl = new QLabel(name, entry);
        nameLbl->setCursor(Qt::PointingHandCursor);
        nameLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #3794ff; font-size: 13px; padding: 0; }"
            "QLabel:hover { color: #4fc3f7; text-decoration: underline; }"));
        nameLbl->setProperty("action", QStringLiteral("openRecent"));
        nameLbl->setProperty("path", folder);
        nameLbl->installEventFilter(this);
        nameLbl->setToolTip(folder);

        const QString elided = fmPath.elidedText(
            QDir::toNativeSeparators(folder), Qt::ElideMiddle, maxPathPx);
        auto *pathLbl = new QLabel(elided, entry);
        pathLbl->setCursor(Qt::PointingHandCursor);
        pathLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #707070; font-size: 11px; padding: 0; }"
            "QLabel:hover { color: #999999; }"));
        pathLbl->setProperty("action", QStringLiteral("openRecent"));
        pathLbl->setProperty("path", folder);
        pathLbl->installEventFilter(this);
        pathLbl->setToolTip(folder);

        lay->addWidget(nameLbl);
        lay->addWidget(pathLbl);

        m_recentLayout->insertWidget(insertAt++, entry);
    }
}

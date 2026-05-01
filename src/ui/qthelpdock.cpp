#include "qthelpdock.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#if EXORCIST_HAS_QT_HELP
#  include <QtHelp/QHelpEngineCore>
#  if __has_include(<QtHelp/QHelpLink>)
#    include <QtHelp/QHelpLink>
#    define EXORCIST_QT_HELP_HAS_LINKS 1
#  else
#    define EXORCIST_QT_HELP_HAS_LINKS 0
#  endif
#endif

QtHelpDock::QtHelpDock(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Search field ──────────────────────────────────────────────────────
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search Qt docs (e.g. QString, QWidget, QFile::open)"));
    m_searchEdit->setClearButtonEnabled(true);
    root->addWidget(m_searchEdit);

    // ── Splitter: topic list (top) | doc viewer (bottom) ──────────────────
    m_splitter = new QSplitter(Qt::Vertical, this);

    m_topicList = new QListWidget(m_splitter);
    m_topicList->setUniformItemSizes(true);
    m_splitter->addWidget(m_topicList);

    m_textBrowser = new QTextBrowser(m_splitter);
    m_textBrowser->setOpenLinks(false);
    m_textBrowser->setOpenExternalLinks(false);
    m_splitter->addWidget(m_textBrowser);

    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 3);
    root->addWidget(m_splitter, 1);

    // ── Status label (errors / "no docs") ─────────────────────────────────
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    m_statusLabel->setVisible(false);
    root->addWidget(m_statusLabel);

    connect(m_searchEdit,  &QLineEdit::textEdited,
            this,          &QtHelpDock::onSearchTextEdited);
    connect(m_topicList,   &QListWidget::itemActivated,
            this,          &QtHelpDock::onTopicActivated);
    connect(m_topicList,   &QListWidget::itemClicked,
            this,          &QtHelpDock::onTopicActivated);
    connect(m_textBrowser, &QTextBrowser::anchorClicked,
            this, [this](const QUrl &url) { loadDocumentForUrl(url); });

    // ── Initialize the help engine (graceful fallback) ────────────────────
#if EXORCIST_HAS_QT_HELP
    const QString collection = findCollectionFile();
    if (collection.isEmpty()) {
        setStatus(tr("Qt documentation not found. Install Qt docs (qt.qhc) "
                     "via Qt Maintenance Tool or set the Qt docs path."));
    } else {
        m_engine = new QHelpEngineCore(collection, this);
        if (!m_engine->setupData()) {
            setStatus(tr("Failed to open Qt help collection: %1")
                          .arg(m_engine->error()));
            delete m_engine;
            m_engine = nullptr;
        } else {
            setStatus(tr("Qt help loaded: %1").arg(QFileInfo(collection).fileName()));
            // Hide status after a short delay (it's just an info ping).
            QTimer::singleShot(3000, m_statusLabel, [this]() {
                if (m_statusLabel)
                    m_statusLabel->setVisible(false);
            });
        }
    }
#else
    setStatus(tr("Qt6::Help component not built. Rebuild with Qt6 Help "
                 "component installed to enable this dock."));
#endif
}

QtHelpDock::~QtHelpDock() = default;

bool QtHelpDock::hasCollection() const
{
#if EXORCIST_HAS_QT_HELP
    return m_engine != nullptr;
#else
    return false;
#endif
}

void QtHelpDock::setStatus(const QString &text)
{
    if (!m_statusLabel)
        return;
    m_statusLabel->setText(text);
    m_statusLabel->setVisible(!text.isEmpty());
}

QString QtHelpDock::findCollectionFile()
{
    // Probe a list of well-known Qt docs install locations.
    // Order: explicit env override, then per-OS standard install paths.
    QStringList probeRoots;

    if (const QByteArray env = qgetenv("EXORCIST_QT_DOCS"); !env.isEmpty())
        probeRoots << QString::fromLocal8Bit(env);

#if defined(Q_OS_WIN)
    probeRoots << QStringLiteral("C:/Qt/Docs")
               << QStringLiteral("C:/Qt")
               << QStringLiteral("D:/Qt/Docs")
               << QStringLiteral("D:/Qt");
#elif defined(Q_OS_MAC)
    probeRoots << QStringLiteral("/Applications/Qt/Docs")
               << QStringLiteral("/usr/local/Qt/Docs")
               << QStringLiteral("/opt/Qt/Docs");
#else
    probeRoots << QStringLiteral("/usr/share/qt6/doc")
               << QStringLiteral("/usr/share/doc/qt6")
               << QStringLiteral("/opt/Qt/Docs")
               << QStringLiteral("/usr/local/Qt/Docs");
#endif

    for (const QString &root : std::as_const(probeRoots)) {
        QDir dir(root);
        if (!dir.exists())
            continue;

        // Iterate recursively (limited depth) for any .qhc file.
        // A typical layout is <root>/Qt-<version>/qt.qhc.
        QDirIterator it(root,
                        QStringList() << QStringLiteral("*.qhc"),
                        QDir::Files,
                        QDirIterator::Subdirectories);
        // Prefer one named "qt.qhc" if multiple are present.
        QString firstMatch;
        while (it.hasNext()) {
            const QString path = it.next();
            const QString name = QFileInfo(path).fileName().toLower();
            if (firstMatch.isEmpty())
                firstMatch = path;
            if (name == QLatin1String("qt.qhc"))
                return path;
        }
        if (!firstMatch.isEmpty())
            return firstMatch;
    }
    return {};
}

void QtHelpDock::onSearchTextEdited(const QString &text)
{
    lookupKeyword(text.trimmed());
}

void QtHelpDock::onTopicActivated(QListWidgetItem *item)
{
    if (!item)
        return;
    const QUrl url = item->data(Qt::UserRole).toUrl();
    if (!url.isEmpty())
        loadDocumentForUrl(url);
}

void QtHelpDock::lookupKeyword(const QString &keyword)
{
    m_topicList->clear();
    if (keyword.isEmpty())
        return;

    // Reflect the queried term in the search box (helps when invoked from F1).
    if (m_searchEdit->text().trimmed() != keyword) {
        QSignalBlocker block(m_searchEdit);
        m_searchEdit->setText(keyword);
    }

#if EXORCIST_HAS_QT_HELP
    if (!m_engine) {
        setStatus(tr("No Qt help collection loaded."));
        return;
    }

    // Map keyword -> list of (title, url) pairs via the engine.
    QList<QPair<QString, QUrl>> hits;

#  if EXORCIST_QT_HELP_HAS_LINKS
    // Qt 6: documentsForKeyword returns QList<QHelpLink>.
    const QList<QHelpLink> links = m_engine->documentsForKeyword(keyword, QString());
    hits.reserve(links.size());
    for (const QHelpLink &link : links)
        hits.append({ link.title.isEmpty() ? link.url.toString() : link.title, link.url });
#  else
    // Older Qt fallback: QMap<title, url>.
    const auto map = m_engine->linksForKeyword(keyword);
    hits.reserve(map.size());
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        hits.append({ it.key(), it.value() });
#  endif

    if (hits.isEmpty()) {
        setStatus(tr("No Qt docs found for '%1'.").arg(keyword));
        m_textBrowser->clear();
        return;
    }
    m_statusLabel->setVisible(false);

    for (const auto &pair : hits) {
        auto *item = new QListWidgetItem(pair.first, m_topicList);
        item->setData(Qt::UserRole, pair.second);
        item->setToolTip(pair.second.toString());
    }

    // Auto-load the single match (or the first of many).
    if (!hits.isEmpty()) {
        m_topicList->setCurrentRow(0);
        loadDocumentForUrl(hits.first().second);
    }
#else
    Q_UNUSED(keyword);
    setStatus(tr("Qt6::Help component not built."));
#endif
}

void QtHelpDock::loadDocumentForUrl(const QUrl &url)
{
#if EXORCIST_HAS_QT_HELP
    if (!m_engine || url.isEmpty())
        return;

    const QByteArray data = m_engine->fileData(url);
    if (data.isEmpty()) {
        setStatus(tr("No content for %1").arg(url.toString()));
        return;
    }

    // Hand the raw HTML to QTextBrowser. The url is set as the document's
    // base URL so relative links/images resolve via the help engine.
    const QString html = QString::fromUtf8(data);
    if (auto *doc = m_textBrowser->document())
        doc->setBaseUrl(url);
    m_textBrowser->setHtml(html);
#else
    Q_UNUSED(url);
#endif
}

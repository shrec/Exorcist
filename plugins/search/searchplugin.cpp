#include "searchplugin.h"

#include "search/searchpanel.h"
#include "search/searchservice.h"
#include "search/symbolindex.h"
#include "search/workspaceindexer.h"

#include "sdk/ihostservices.h"
#include "sdk/isearchservice.h"
#include "core/istatusbarmanager.h"

#include <QObject>
#include <QString>

// ── SearchServiceImpl (plugin-local) ─────────────────────────────────────────
// Bridges SearchPanel + WorkspaceIndexer to the ISearchService SDK interface.
// Lives here because those objects are owned by this plugin.

class SearchServiceImpl : public ISearchService
{
    Q_OBJECT
public:
    explicit SearchServiceImpl(SearchPanel *panel, WorkspaceIndexer *indexer,
                               QObject *parent = nullptr)
        : ISearchService(parent), m_panel(panel), m_indexer(indexer)
    {
        connect(m_panel, &SearchPanel::resultActivated,
                this, &ISearchService::resultActivated);
    }

    void setRootPath(const QString &path) override  { m_panel->setRootPath(path); }
    void activateSearch() override                  { m_panel->activateSearch(); }
    void indexWorkspace(const QString &root) override { m_indexer->indexWorkspace(root); }
    void reindexFile(const QString &file) override  { m_indexer->reindexFile(file); }
    QWidget *searchPanel() const override           { return m_panel; }

private:
    SearchPanel      *m_panel;
    WorkspaceIndexer *m_indexer;
};

// ── SearchPlugin ──────────────────────────────────────────────────────────────

SearchPlugin::SearchPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo SearchPlugin::info() const
{
    PluginInfo i;
    i.id          = QStringLiteral("org.exorcist.search");
    i.name        = QStringLiteral("Search");
    i.version     = QStringLiteral("1.0.0");
    i.description = QStringLiteral("Workspace search, file indexing, and symbol navigation.");
    return i;
}

bool SearchPlugin::initializePlugin()
{
    auto *searchService = new SearchService(this);
    m_panel       = new SearchPanel(searchService, nullptr);
    m_indexer     = new WorkspaceIndexer(this);
    m_symbolIndex = new SymbolIndex(this);

    // Wire indexer signals → symbol index rebuild + status bar
    connect(m_indexer, &WorkspaceIndexer::indexingStarted, this, [this] {
        showStatusMessage(tr("Indexing workspace..."));
    });
    connect(m_indexer, &WorkspaceIndexer::indexingProgress, this,
            [this](int done, int total) {
        showStatusMessage(
            tr("Indexing %1 / %2 files...").arg(done).arg(total));
    });
    connect(m_indexer, &WorkspaceIndexer::indexingFinished, this,
            [this](int files, int /*chunks*/) {
        showStatusMessage(
            tr("Indexed %1 files").arg(files), 5000);
        // Rebuild symbol index from freshly indexed chunks
        m_symbolIndex->clear();
        const auto chunks = m_indexer->search(QString(), 50000);
        for (const auto &c : chunks) {
            if (!c.symbolName.isEmpty())
                m_symbolIndex->indexFile(c.filePath, c.content);
        }
    });

    // Register all three services in ServiceRegistry
    auto *svcImpl = new SearchServiceImpl(m_panel, m_indexer, this);
    registerService(QStringLiteral("searchService"), svcImpl);
    registerService(QStringLiteral("symbolIndex"),  m_symbolIndex);
    registerService(QStringLiteral("workspaceIndexer"), m_indexer);

    return true;
}

void SearchPlugin::shutdownPlugin()
{
    m_panel       = nullptr;
    m_indexer     = nullptr;
    m_symbolIndex = nullptr;
}

QWidget *SearchPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QLatin1String("SearchDock") && m_panel) {
        m_panel->setParent(parent);
        return m_panel;
    }
    return nullptr;
}

#include "searchplugin.moc"

#include "cpplanguageplugin.h"
#include "clangdmanager.h"

#include "lsp/lspclient.h"
#include "sdk/ilspservice.h"
#include "sdk/ihostservices.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

// ── LspServiceBridge ──────────────────────────────────────────────────────────
// Implements ILspService by delegating to ClangdManager + LspClient.
// Replaces LspBootstrap's signal-forwarding role.

class LspServiceBridge : public ILspService
{
    Q_OBJECT

public:
    LspServiceBridge(ClangdManager *clangd, LspClient *lspClient, QObject *parent)
        : ILspService(parent), m_clangd(clangd), m_lspClient(lspClient)
    {
        connect(m_clangd, &ClangdManager::serverReady,
                this, &ILspService::serverReady);

        // Go-to-Definition (F12)
        connect(m_lspClient, &LspClient::definitionResult, this,
                [this](const QString & /*uri*/, const QJsonArray &locations) {
            if (locations.isEmpty()) {
                emit statusMessage(tr("No definition found"), 3000);
                return;
            }
            const QJsonObject loc = locations.first().toObject();
            const QString targetUri = loc.value(QStringLiteral("uri")).toString();
            const QJsonObject range = loc.value(QStringLiteral("range")).toObject();
            const QJsonObject start = range.value(QStringLiteral("start")).toObject();
            const int line      = start.value(QStringLiteral("line")).toInt();
            const int character = start.value(QStringLiteral("character")).toInt();

            QString path = QUrl(targetUri).toLocalFile();
            if (path.isEmpty()) path = targetUri;
            emit navigateToLocation(path, line, character);
        });

        // Find References
        connect(m_lspClient, &LspClient::referencesResult, this,
                [this](const QString & /*uri*/, const QJsonArray &locations) {
            if (locations.isEmpty()) {
                emit statusMessage(tr("No references found"), 3000);
                return;
            }
            emit referencesReady(locations);
        });

        // Rename Symbol
        connect(m_lspClient, &LspClient::renameResult, this,
                [this](const QString & /*uri*/, const QJsonObject &workspaceEdit) {
            emit workspaceEditRequested(workspaceEdit);
        });
    }

    void startServer(const QString &workspaceRoot,
                     const QStringList &args = {}) override
    {
        m_clangd->start(workspaceRoot, args);
    }

    void stopServer() override
    {
        m_clangd->stop();
    }

private:
    ClangdManager *m_clangd;
    LspClient     *m_lspClient;
};

// ── CppLanguagePlugin ─────────────────────────────────────────────────────────

CppLanguagePlugin::CppLanguagePlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo CppLanguagePlugin::info() const
{
    PluginInfo pi;
    pi.id      = QStringLiteral("org.exorcist.cpp-language");
    pi.name    = QStringLiteral("C/C++ Language Support");
    pi.version = QStringLiteral("1.0.0");
    return pi;
}

bool CppLanguagePlugin::initialize(IHostServices *host)
{
    m_host = host;

    m_clangd    = new ClangdManager(this);
    m_lspClient = new LspClient(m_clangd->transport(), this);

    auto *bridge = new LspServiceBridge(m_clangd, m_lspClient, this);
    m_lspService = bridge;

    host->registerService(QStringLiteral("lspClient"), m_lspClient);
    host->registerService(QStringLiteral("lspService"), bridge);
    return true;
}

void CppLanguagePlugin::shutdown()
{
    if (m_clangd)
        m_clangd->stop();
}

#include "cpplanguageplugin.moc"

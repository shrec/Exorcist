#include "lspbootstrap.h"

#include "../lsp/clangdmanager.h"
#include "../lsp/lspclient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

LspBootstrap::LspBootstrap(QObject *parent)
    : QObject(parent)
{
}

void LspBootstrap::initialize()
{
    m_clangd    = new ClangdManager(this);
    m_lspClient = new LspClient(m_clangd->transport(), this);

    connect(m_clangd, &ClangdManager::serverReady, this, [this]() {
        // Caller sets m_currentFolder before starting clangd
        // The LspClient::initialize() is called externally via lspReady signal
        emit lspReady();
    });

    connectLspSignals();
}

void LspBootstrap::connectLspSignals()
{
    // Go-to-Definition (F12)
    connect(m_lspClient, &LspClient::definitionResult, this,
            [this](const QString &/*uri*/, const QJsonArray &locations) {
        if (locations.isEmpty()) {
            emit statusMessage(tr("No definition found"), 3000);
            return;
        }
        const QJsonObject loc = locations.first().toObject();
        const QString targetUri = loc.value(QStringLiteral("uri")).toString();
        const QJsonObject range = loc.value(QStringLiteral("range")).toObject();
        const QJsonObject start = range.value(QStringLiteral("start")).toObject();
        const int line = start.value(QStringLiteral("line")).toInt();
        const int character = start.value(QStringLiteral("character")).toInt();

        QString path = QUrl(targetUri).toLocalFile();
        if (path.isEmpty()) path = targetUri;
        emit navigateToLocation(path, line, character);
    });

    // Find References
    connect(m_lspClient, &LspClient::referencesResult, this,
            [this](const QString &/*uri*/, const QJsonArray &locations) {
        if (locations.isEmpty()) {
            emit statusMessage(tr("No references found"), 3000);
            return;
        }
        emit referencesReady(locations);
    });

    // Rename Symbol
    connect(m_lspClient, &LspClient::renameResult, this,
            [this](const QString &/*uri*/, const QJsonObject &workspaceEdit) {
        emit workspaceEditRequested(workspaceEdit);
    });
}

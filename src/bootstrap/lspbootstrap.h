#pragma once

#include <QObject>

class ClangdManager;
class LspClient;

class LspBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit LspBootstrap(QObject *parent = nullptr);

    /// Create CLangd + LSP client pair. Must be called before setupUi().
    void initialize();

    ClangdManager *clangd() const { return m_clangd; }
    LspClient     *lspClient() const { return m_lspClient; }

signals:
    /// File navigation request from Go-to-Definition / References / Rename.
    void navigateToLocation(const QString &path, int line, int character);

    /// Rename response from LSP — caller should apply the workspace edit.
    void workspaceEditRequested(const QJsonObject &workspaceEdit);

    /// Show references list (from textDocument/references response).
    void referencesReady(const QJsonArray &locations);

    /// Definition not found / no references found.
    void statusMessage(const QString &msg, int timeout);

    /// LSP has initialized and is ready for requests.
    void lspReady();

private:
    void connectLspSignals();

    ClangdManager *m_clangd    = nullptr;
    LspClient     *m_lspClient = nullptr;
};

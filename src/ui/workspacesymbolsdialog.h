#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTimer;
class LspClient;

// ── WorkspaceSymbolsDialog ────────────────────────────────────────────────────
//
// VS Code-style "Go to Symbol in Workspace" dialog (Ctrl+T).
//
// Uses clangd's `workspace/symbol` LSP request to search across all indexed
// files for functions, classes, variables, etc. The dialog debounces user
// typing (200ms) before issuing each lookup to avoid hammering the server.
//
// Activation (double-click or Enter) emits `symbolActivated(filePath, line,
// character)`. Caller is responsible for opening the file and jumping to the
// location (typically via MainWindow::navigateToLocation).
//
// If no LspClient is provided (or it's not initialized), the dialog still
// shows but the result list will display a stub message — wiring the backend
// is left to the caller via setLspClient().

class WorkspaceSymbolsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WorkspaceSymbolsDialog(QWidget *parent = nullptr);
    ~WorkspaceSymbolsDialog() override;

    // Bind an LspClient as the backend. May be null — in which case the
    // dialog shows a placeholder hint instead of running real lookups.
    void setLspClient(LspClient *client);

signals:
    // Emitted when the user activates a symbol (double-click or Enter on a
    // selected row). The dialog closes itself before this fires.
    void symbolActivated(const QString &filePath, int line, int character);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void onQueryChanged(const QString &text);
    void onDebounceTimeout();
    void onItemActivated(QListWidgetItem *item);
    void onOpenClicked();

private:
    struct ResultEntry
    {
        QString filePath;
        int     line      = 0;
        int     character = 0;
        QString name;
        QString kind;
    };

    void  applyDarkTheme();
    void  populateResults(const QVector<ResultEntry> &entries);
    void  showStubResults(const QString &query);
    void  activateRow(int row);
    static QString kindLabel(int lspSymbolKind);

    QLineEdit               *m_queryEdit  = nullptr;
    QListWidget             *m_resultList = nullptr;
    QPushButton             *m_openBtn    = nullptr;
    QPushButton             *m_cancelBtn  = nullptr;
    QTimer                  *m_debounce   = nullptr;
    LspClient               *m_lspClient  = nullptr;
    QVector<ResultEntry>     m_results;
};

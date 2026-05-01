#include "workspacesymbolsdialog.h"

#include "../lsp/lspclient.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// LSP SymbolKind enumeration (LSP 3.17 §15.16). Values 1..26.
QString lspKindName(int kind)
{
    switch (kind) {
        case 1:  return QStringLiteral("file");
        case 2:  return QStringLiteral("module");
        case 3:  return QStringLiteral("namespace");
        case 4:  return QStringLiteral("package");
        case 5:  return QStringLiteral("class");
        case 6:  return QStringLiteral("method");
        case 7:  return QStringLiteral("property");
        case 8:  return QStringLiteral("field");
        case 9:  return QStringLiteral("constructor");
        case 10: return QStringLiteral("enum");
        case 11: return QStringLiteral("interface");
        case 12: return QStringLiteral("function");
        case 13: return QStringLiteral("variable");
        case 14: return QStringLiteral("constant");
        case 15: return QStringLiteral("string");
        case 16: return QStringLiteral("number");
        case 17: return QStringLiteral("boolean");
        case 18: return QStringLiteral("array");
        case 19: return QStringLiteral("object");
        case 20: return QStringLiteral("key");
        case 21: return QStringLiteral("null");
        case 22: return QStringLiteral("enum-member");
        case 23: return QStringLiteral("struct");
        case 24: return QStringLiteral("event");
        case 25: return QStringLiteral("operator");
        case 26: return QStringLiteral("type-parameter");
        default: return QStringLiteral("symbol");
    }
}

} // namespace

WorkspaceSymbolsDialog::WorkspaceSymbolsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Go to Symbol in Workspace"));
    setModal(true);
    resize(640, 480);

    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 10, 10, 10);
    vbox->setSpacing(8);

    auto *hint = new QLabel(
        tr("Type to search symbols across the workspace (functions, "
           "classes, variables)…"),
        this);
    hint->setObjectName(QStringLiteral("wsSymHint"));
    hint->setWordWrap(true);
    vbox->addWidget(hint);

    m_queryEdit = new QLineEdit(this);
    m_queryEdit->setPlaceholderText(tr("Symbol name…"));
    m_queryEdit->setClearButtonEnabled(true);
    m_queryEdit->installEventFilter(this);  // for Up/Down/Enter forwarding
    vbox->addWidget(m_queryEdit);

    m_resultList = new QListWidget(this);
    m_resultList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultList->setUniformItemSizes(true);
    m_resultList->setAlternatingRowColors(false);
    vbox->addWidget(m_resultList, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    m_openBtn = new QPushButton(tr("Open"), this);
    m_openBtn->setDefault(true);
    m_openBtn->setEnabled(false);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    btnRow->addWidget(m_openBtn);
    btnRow->addWidget(m_cancelBtn);
    vbox->addLayout(btnRow);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(200);

    connect(m_queryEdit, &QLineEdit::textChanged,
            this, &WorkspaceSymbolsDialog::onQueryChanged);
    connect(m_debounce, &QTimer::timeout,
            this, &WorkspaceSymbolsDialog::onDebounceTimeout);
    connect(m_resultList, &QListWidget::itemDoubleClicked,
            this, &WorkspaceSymbolsDialog::onItemActivated);
    connect(m_resultList, &QListWidget::currentRowChanged, this,
            [this](int row) { m_openBtn->setEnabled(row >= 0); });
    connect(m_openBtn, &QPushButton::clicked,
            this, &WorkspaceSymbolsDialog::onOpenClicked);
    connect(m_cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);

    applyDarkTheme();
    m_queryEdit->setFocus();
}

WorkspaceSymbolsDialog::~WorkspaceSymbolsDialog() = default;

bool WorkspaceSymbolsDialog::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_queryEdit && ev->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(ev);
        switch (ke->key()) {
            case Qt::Key_Down: {
                const int next = qMin(m_resultList->currentRow() + 1,
                                       m_resultList->count() - 1);
                if (next >= 0)
                    m_resultList->setCurrentRow(next);
                return true;
            }
            case Qt::Key_Up: {
                const int next = qMax(m_resultList->currentRow() - 1, 0);
                if (m_resultList->count() > 0)
                    m_resultList->setCurrentRow(next);
                return true;
            }
            case Qt::Key_Return:
            case Qt::Key_Enter:
                activateRow(m_resultList->currentRow());
                return true;
            default:
                break;
        }
    }
    return QDialog::eventFilter(obj, ev);
}

void WorkspaceSymbolsDialog::setLspClient(LspClient *client)
{
    if (m_lspClient == client)
        return;

    if (m_lspClient) {
        disconnect(m_lspClient, &LspClient::workspaceSymbolsResult,
                   this, nullptr);
    }
    m_lspClient = client;

    if (m_lspClient) {
        connect(m_lspClient, &LspClient::workspaceSymbolsResult, this,
            [this](const QJsonArray &symbols) {
                QVector<ResultEntry> entries;
                entries.reserve(symbols.size());
                for (const QJsonValue &v : symbols) {
                    const QJsonObject sym = v.toObject();
                    const QJsonObject loc = sym.value(QStringLiteral("location")).toObject();
                    const QString uri = loc.value(QStringLiteral("uri")).toString();
                    const QString path = QUrl(uri).toLocalFile();
                    const QJsonObject start = loc.value(QStringLiteral("range"))
                                                  .toObject()
                                                  .value(QStringLiteral("start"))
                                                  .toObject();
                    ResultEntry e;
                    e.name      = sym.value(QStringLiteral("name")).toString();
                    e.filePath  = path.isEmpty() ? uri : path;
                    e.line      = start.value(QStringLiteral("line")).toInt();
                    e.character = start.value(QStringLiteral("character")).toInt();
                    e.kind      = lspKindName(sym.value(QStringLiteral("kind")).toInt());
                    entries.append(e);
                }
                populateResults(entries);
            });
    }
}

void WorkspaceSymbolsDialog::onQueryChanged(const QString & /*text*/)
{
    m_debounce->start();
}

void WorkspaceSymbolsDialog::onDebounceTimeout()
{
    const QString q = m_queryEdit->text().trimmed();
    if (m_lspClient && m_lspClient->isInitialized()) {
        m_lspClient->requestWorkspaceSymbols(q);
    } else {
        // TODO: fallback — query open editors' symbol/tag list when the
        //       LSP service is unavailable. For now show a placeholder.
        showStubResults(q);
    }
}

void WorkspaceSymbolsDialog::onItemActivated(QListWidgetItem *item)
{
    activateRow(m_resultList->row(item));
}

void WorkspaceSymbolsDialog::onOpenClicked()
{
    activateRow(m_resultList->currentRow());
}

void WorkspaceSymbolsDialog::activateRow(int row)
{
    if (row < 0 || row >= m_results.size())
        return;
    const ResultEntry e = m_results[row];
    accept();  // close dialog first so caller can focus the editor
    emit symbolActivated(e.filePath, e.line, e.character);
}

void WorkspaceSymbolsDialog::populateResults(const QVector<ResultEntry> &entries)
{
    m_results = entries;
    m_resultList->clear();

    for (const ResultEntry &e : entries) {
        const QString file = QFileInfo(e.filePath).fileName();
        const QString label = QStringLiteral("%1  →  %2:%3  [%4]")
                                  .arg(e.name,
                                       file,
                                       QString::number(e.line + 1),
                                       e.kind);
        auto *item = new QListWidgetItem(label, m_resultList);
        item->setToolTip(e.filePath);
    }
    if (!m_results.isEmpty())
        m_resultList->setCurrentRow(0);
    m_openBtn->setEnabled(m_resultList->currentRow() >= 0);
}

void WorkspaceSymbolsDialog::showStubResults(const QString &query)
{
    m_results.clear();
    m_resultList->clear();
    auto *item = new QListWidgetItem(
        query.isEmpty()
            ? tr("(LSP not available — start a workspace with clangd to enable "
                 "symbol search)")
            : tr("(no LSP backend — would search for: %1)").arg(query),
        m_resultList);
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    m_openBtn->setEnabled(false);
}

QString WorkspaceSymbolsDialog::kindLabel(int lspSymbolKind)
{
    return lspKindName(lspSymbolKind);
}

void WorkspaceSymbolsDialog::applyDarkTheme()
{
    // VS Code dark-modern aligned palette — keep readable contrast and
    // dense, monospace-friendly layout for the result list.
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1f1f1f; color: #cccccc; }"
        "QLabel#wsSymHint { color: #9d9d9d; }"
        "QLineEdit {"
        "    background-color: #313131;"
        "    color: #e6e6e6;"
        "    border: 1px solid #3c3c3c;"
        "    border-radius: 2px;"
        "    padding: 5px 6px;"
        "    selection-background-color: #094771;"
        "    selection-color: #ffffff;"
        "}"
        "QLineEdit:focus { border: 1px solid #007fd4; }"
        "QListWidget {"
        "    background-color: #1f1f1f;"
        "    color: #d4d4d4;"
        "    border: 1px solid #3c3c3c;"
        "    border-radius: 2px;"
        "    outline: 0;"
        "    font-family: Consolas, 'Cascadia Mono', monospace;"
        "    font-size: 12px;"
        "}"
        "QListWidget::item { padding: 4px 6px; }"
        "QListWidget::item:hover { background-color: #2a2d2e; }"
        "QListWidget::item:selected {"
        "    background-color: #094771;"
        "    color: #ffffff;"
        "}"
        "QPushButton {"
        "    background-color: #0e639c;"
        "    color: #ffffff;"
        "    border: 1px solid #1177bb;"
        "    border-radius: 2px;"
        "    padding: 5px 14px;"
        "    min-width: 72px;"
        "}"
        "QPushButton:hover  { background-color: #1177bb; }"
        "QPushButton:pressed{ background-color: #0e5e93; }"
        "QPushButton:disabled {"
        "    background-color: #2a2d2e;"
        "    color: #6e6e6e;"
        "    border: 1px solid #3c3c3c;"
        "}"));
}

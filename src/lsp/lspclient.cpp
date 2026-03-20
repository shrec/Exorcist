#include "lspclient.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QUrl>

Q_LOGGING_CATEGORY(lcLsp, "exorcist.lsp")

// ── Constructor ───────────────────────────────────────────────────────────────

LspClient::LspClient(LspTransport *transport, QObject *parent)
    : QObject(parent),
      m_transport(transport)
{
    connect(m_transport, &LspTransport::messageReceived,
            this, &LspClient::onMessageReceived);
    connect(m_transport, &LspTransport::transportError,
            this, &LspClient::serverError);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString LspClient::pathToUri(const QString &path)
{
    return QUrl::fromLocalFile(path).toString();
}

QString LspClient::languageIdForPath(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "c" ||
        ext == "h"   || ext == "hpp" || ext == "hxx" || ext == "inl")
        return "cpp";
    if (ext == "cs")   return "csharp";
    if (ext == "py" || ext == "pyw") return "python";
    if (ext == "js" || ext == "mjs") return "javascript";
    if (ext == "ts")   return "typescript";
    if (ext == "tsx")  return "typescriptreact";
    if (ext == "jsx")  return "javascriptreact";
    if (ext == "rs")   return "rust";
    if (ext == "go")   return "go";
    if (ext == "java") return "java";
    if (ext == "kt" || ext == "kts") return "kotlin";
    if (ext == "swift") return "swift";
    if (ext == "dart") return "dart";
    if (ext == "rb")   return "ruby";
    if (ext == "php")  return "php";
    if (ext == "lua")  return "lua";
    if (ext == "scala") return "scala";
    if (ext == "html" || ext == "htm") return "html";
    if (ext == "css")  return "css";
    if (ext == "json") return "json";
    if (ext == "xml")  return "xml";
    if (ext == "yaml" || ext == "yml") return "yaml";
    if (ext == "toml") return "toml";
    if (ext == "sh" || ext == "bash") return "shellscript";
    if (ext == "ps1")  return "powershell";
    if (ext == "md")   return "markdown";
    if (ext == "cmake" || QFileInfo(path).fileName().toLower() == "cmakelists.txt")
        return "cmake";
    return "plaintext";
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void LspClient::initialize(const QString &workspaceRoot)
{
    const QString rootUri = pathToUri(workspaceRoot);

    QJsonObject textDocumentCaps;
    textDocumentCaps["synchronization"] = QJsonObject{
        {"dynamicRegistration", false},
        {"willSave", false},
        {"didSave", false},
    };
    textDocumentCaps["completion"] = QJsonObject{
        {"completionItem", QJsonObject{
            {"snippetSupport", false},
            {"documentationFormat", QJsonArray{"plaintext"}},
        }},
    };
    textDocumentCaps["hover"] = QJsonObject{
        {"contentFormat", QJsonArray{"markdown", "plaintext"}},
    };
    textDocumentCaps["signatureHelp"] = QJsonObject{
        {"signatureInformation", QJsonObject{
            {"documentationFormat", QJsonArray{"plaintext"}},
        }},
    };
    textDocumentCaps["publishDiagnostics"] = QJsonObject{
        {"relatedInformation", false},
    };
    textDocumentCaps["formatting"] = QJsonObject{};
    textDocumentCaps["rangeFormatting"] = QJsonObject{};
    textDocumentCaps["definition"] = QJsonObject{{"linkSupport", false}};
    textDocumentCaps["references"] = QJsonObject{};
    textDocumentCaps["rename"]     = QJsonObject{{"prepareSupport", false}};
    textDocumentCaps["documentSymbol"] = QJsonObject{
        {"hierarchicalDocumentSymbolSupport", true},
    };
    textDocumentCaps["codeAction"] = QJsonObject{
        {"codeActionLiteralSupport", QJsonObject{
            {"codeActionKind", QJsonObject{
                {"valueSet", QJsonArray{
                    "", "quickfix", "refactor", "refactor.extract",
                    "refactor.inline", "refactor.rewrite",
                    "source", "source.organizeImports",
                }},
            }},
        }},
    };

    QJsonObject params{
        {"processId",  static_cast<int>(QCoreApplication::applicationPid())},
        {"rootUri",    rootUri},
        {"capabilities", QJsonObject{
            {"textDocument", textDocumentCaps},
            {"workspace", QJsonObject{
                {"applyEdit", false},
                {"symbol", QJsonObject{
                    {"symbolKind", QJsonObject{
                        {"valueSet", QJsonArray{1,2,3,4,5,6,7,8,9,10,
                                               11,12,13,14,15,16,17,18,19,20,
                                               21,22,23,24,25,26}},
                    }},
                }},
            }},
        }},
        {"clientInfo", QJsonObject{
            {"name",    "Exorcist"},
            {"version", "0.1.0"},
        }},
    };

    const int id = sendRequest("initialize", params);
    m_pending[id] = {"initialize", rootUri, 0, 0};
}

void LspClient::shutdown()
{
    if (!m_initialized) return;

    const int id = sendRequest("shutdown", {});
    m_pending[id] = {"shutdown", {}, 0, 0};
    m_initialized = false;
}

// ── Document sync ─────────────────────────────────────────────────────────────

void LspClient::didOpen(const QString &uri, const QString &languageId,
                        const QString &text, int version)
{
    sendNotification("textDocument/didOpen", QJsonObject{
        {"textDocument", QJsonObject{
            {"uri",        uri},
            {"languageId", languageId},
            {"version",    version},
            {"text",       text},
        }},
    });
}

void LspClient::didChange(const QString &uri, const QString &text, int version)
{
    sendNotification("textDocument/didChange", QJsonObject{
        {"textDocument",    QJsonObject{{"uri", uri}, {"version", version}}},
        {"contentChanges",  QJsonArray{QJsonObject{{"text", text}}}},
    });
}

void LspClient::didClose(const QString &uri)
{
    sendNotification("textDocument/didClose", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
    });
}

// ── Requests ──────────────────────────────────────────────────────────────────

void LspClient::requestCompletion(const QString &uri, int line, int character,
                                  int triggerKind, const QString &triggerChar)
{
    if (!m_initialized) return;
    QJsonObject context{{"triggerKind", triggerKind}};
    if (!triggerChar.isEmpty())
        context["triggerCharacter"] = triggerChar;
    const int id = sendRequest("textDocument/completion", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
        {"position",     QJsonObject{{"line", line}, {"character", character}}},
        {"context",      context},
    });
    m_pending[id] = {"textDocument/completion", uri, line, character};
}

void LspClient::requestHover(const QString &uri, int line, int character)
{
    if (!m_initialized) return;
    const int id = sendRequest("textDocument/hover", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
        {"position",     QJsonObject{{"line", line}, {"character", character}}},
    });
    m_pending[id] = {"textDocument/hover", uri, line, character};
}

void LspClient::requestSignatureHelp(const QString &uri, int line, int character)
{
    if (!m_initialized) return;
    const int id = sendRequest("textDocument/signatureHelp", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
        {"position",     QJsonObject{{"line", line}, {"character", character}}},
    });
    m_pending[id] = {"textDocument/signatureHelp", uri, line, character};
}

void LspClient::requestDefinition(const QString &uri, int line, int character)
{
    if (!m_initialized) return;
    const int id = sendRequest("textDocument/definition", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
        {"position",     QJsonObject{{"line", line}, {"character", character}}},
    });
    m_pending[id] = {"textDocument/definition", uri, line, character};
}

void LspClient::requestDeclaration(const QString &uri, int line, int character)
{
    if (!m_initialized) return;
    const int id = sendRequest("textDocument/declaration", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
        {"position",     QJsonObject{{"line", line}, {"character", character}}},
    });
    m_pending[id] = {"textDocument/declaration", uri, line, character};
}

void LspClient::requestFormatting(const QString &uri, int tabSize, bool insertSpaces)
{
    if (!m_initialized) return;
    const int id = sendRequest("textDocument/formatting", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
        {"options", QJsonObject{
            {"tabSize",      tabSize},
            {"insertSpaces", insertSpaces},
        }},
    });
    m_pending[id] = {"textDocument/formatting", uri, 0, 0};
}

void LspClient::requestRangeFormatting(const QString &uri,
                                       int startLine, int startChar,
                                       int endLine,   int endChar,
                                       int tabSize,   bool insertSpaces)
{
    if (!m_initialized) return;
    const int id = sendRequest("textDocument/rangeFormatting", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
        {"range", QJsonObject{
            {"start", QJsonObject{{"line", startLine}, {"character", startChar}}},
            {"end",   QJsonObject{{"line", endLine},   {"character", endChar}}},
        }},
        {"options", QJsonObject{
            {"tabSize",      tabSize},
            {"insertSpaces", insertSpaces},
        }},
    });
    m_pending[id] = {"textDocument/rangeFormatting", uri, 0, 0};
}

void LspClient::requestOnTypeFormatting(const QString &uri, int line,
                                        int character, const QString &ch,
                                        int tabSize, bool insertSpaces)
{
    if (!m_initialized) return;
    const int id = sendRequest("textDocument/onTypeFormatting", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
        {"position",     QJsonObject{{"line", line}, {"character", character}}},
        {"ch",           ch},
        {"options", QJsonObject{
            {"tabSize",      tabSize},
            {"insertSpaces", insertSpaces},
        }},
    });
    m_pending[id] = {"textDocument/onTypeFormatting", uri, line, character};
}

void LspClient::requestReferences(const QString &uri, int line, int character,
                                   bool includeDeclaration)
{
    if (!m_initialized) return;
    const int id = sendRequest("textDocument/references", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
        {"position",     QJsonObject{{"line", line}, {"character", character}}},
        {"context",      QJsonObject{{"includeDeclaration", includeDeclaration}}},
    });
    m_pending[id] = {"textDocument/references", uri, line, character};
}

void LspClient::requestRename(const QString &uri, int line, int character,
                               const QString &newName)
{
    if (!m_initialized) return;
    const int id = sendRequest("textDocument/rename", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
        {"position",     QJsonObject{{"line", line}, {"character", character}}},
        {"newName",      newName},
    });
    m_pending[id] = {"textDocument/rename", uri, line, character};
}

void LspClient::requestDocumentSymbols(const QString &uri)
{
    if (!m_initialized) return;
    const int id = sendRequest("textDocument/documentSymbol", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
    });
    m_pending[id] = {"textDocument/documentSymbol", uri, 0, 0};
}

void LspClient::requestCodeAction(const QString &uri,
                                  int startLine, int startChar,
                                  int endLine,   int endChar,
                                  const QJsonArray &diagnostics)
{
    if (!m_initialized) return;
    const int id = sendRequest("textDocument/codeAction", QJsonObject{
        {"textDocument", QJsonObject{{"uri", uri}}},
        {"range", QJsonObject{
            {"start", QJsonObject{{"line", startLine}, {"character", startChar}}},
            {"end",   QJsonObject{{"line", endLine},   {"character", endChar}}},
        }},
        {"context", QJsonObject{
            {"diagnostics", diagnostics},
        }},
    });
    m_pending[id] = {"textDocument/codeAction", uri, startLine, startChar};
}

void LspClient::requestWorkspaceSymbols(const QString &query)
{
    if (!m_initialized) return;
    const int id = sendRequest("workspace/symbol", QJsonObject{
        {"query", query},
    });
    m_pending[id] = {"workspace/symbol", {}, 0, 0};
}

// ── Message dispatch ──────────────────────────────────────────────────────────

void LspClient::onMessageReceived(const LspMessage &msg)
{
    const QJsonObject &obj = msg.payload;

    if (obj.contains("id")) {
        // Response to a request we sent
        const int id = obj["id"].toInt(-1);
        const QJsonValue result = obj["result"];
        const QJsonObject error = obj["error"].toObject();
        handleResponse(id, result, error);
    } else if (obj.contains("method")) {
        // Notification from the server
        handleNotification(obj["method"].toString(),
                           obj["params"].toObject());
    }
}

void LspClient::handleResponse(int id, const QJsonValue &result,
                               const QJsonObject &error)
{
    if (!error.isEmpty()) {
        qCWarning(lcLsp) << "LSP error for id" << id
                         << error["message"].toString();
        m_pending.remove(id);
        return;
    }

    const PendingRequest req = m_pending.take(id);

    if (req.method == "initialize") {
        // Send initialized notification
        sendNotification("initialized", {});
        m_initialized = true;
        qCInfo(lcLsp) << "LSP server initialized";
        emit initialized();
    }
    else if (req.method == "shutdown") {
        sendNotification("exit", {});
    }
    else if (req.method == "textDocument/completion") {
        QJsonArray items;
        bool isIncomplete = false;
        if (result.isArray()) {
            items = result.toArray();
        } else if (result.isObject()) {
            const QJsonObject obj = result.toObject();
            items = obj["items"].toArray();
            isIncomplete = obj["isIncomplete"].toBool(false);
        }
        emit completionResult(req.uri, req.line, req.character,
                              items, isIncomplete);
    }
    else if (req.method == "textDocument/hover") {
        QString markdown;
        if (result.isObject()) {
            const QJsonValue contents = result.toObject()["contents"];
            if (contents.isString()) {
                markdown = contents.toString();
            } else if (contents.isObject()) {
                markdown = contents.toObject()["value"].toString();
            } else if (contents.isArray()) {
                for (const QJsonValue &v : contents.toArray()) {
                    if (v.isString()) {
                        markdown += v.toString() + "\n";
                    } else if (v.isObject()) {
                        markdown += v.toObject()["value"].toString() + "\n";
                    }
                }
            }
        }
        emit hoverResult(req.uri, req.line, req.character, markdown.trimmed());
    }
    else if (req.method == "textDocument/signatureHelp") {
        emit signatureHelpResult(req.uri, req.line, req.character,
                                 result.toObject());
    }
    else if (req.method == "textDocument/formatting" ||
             req.method == "textDocument/rangeFormatting" ||
             req.method == "textDocument/onTypeFormatting") {
        emit formattingResult(req.uri, result.toArray());
    }
    else if (req.method == "textDocument/definition") {
        QJsonArray locations;
        if (result.isArray()) {
            locations = result.toArray();
        } else if (result.isObject()) {
            locations = QJsonArray{result.toObject()};
        }
        emit definitionResult(req.uri, locations);
    }
    else if (req.method == "textDocument/declaration") {
        QJsonArray locations;
        if (result.isArray()) {
            locations = result.toArray();
        } else if (result.isObject()) {
            locations = QJsonArray{result.toObject()};
        }
        emit declarationResult(req.uri, locations);
    }
    else if (req.method == "textDocument/references") {
        emit referencesResult(req.uri, result.toArray());
    }
    else if (req.method == "textDocument/rename") {
        if (result.isObject())
            emit renameResult(req.uri, result.toObject());
    }
    else if (req.method == "textDocument/documentSymbol") {
        emit documentSymbolsResult(req.uri, result.toArray());
    }
    else if (req.method == "textDocument/codeAction") {
        emit codeActionResult(req.uri, req.line, req.character, result.toArray());
    }
    else if (req.method == "workspace/symbol") {
        emit workspaceSymbolsResult(result.toArray());
    }
}

void LspClient::handleNotification(const QString &method,
                                   const QJsonObject &params)
{
    if (method == "textDocument/publishDiagnostics") {
        const QString uri         = params["uri"].toString();
        const QJsonArray diags    = params["diagnostics"].toArray();
        emit diagnosticsPublished(uri, diags);
    }
    // Other notifications (window/showMessage, etc.) silently ignored for now.
}

// ── Wire helpers ──────────────────────────────────────────────────────────────

int LspClient::sendRequest(const QString &method, const QJsonObject &params)
{
    const int id = m_nextId++;
    LspMessage msg;
    msg.payload = QJsonObject{
        {"jsonrpc", "2.0"},
        {"id",      id},
        {"method",  method},
        {"params",  params},
    };
    m_transport->send(msg);
    return id;
}

void LspClient::sendNotification(const QString &method, const QJsonObject &params)
{
    LspMessage msg;
    QJsonObject obj{
        {"jsonrpc", "2.0"},
        {"method",  method},
    };
    if (!params.isEmpty())
        obj["params"] = params;
    msg.payload = obj;
    m_transport->send(msg);
}

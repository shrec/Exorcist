#include <QTest>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonDocument>

#include "lsp/lspmessage.h"
#include "lsp/lsptransport.h"
#include "lsp/lspclient.h"

// ── Mock transport ────────────────────────────────────────────────────────────

class MockTransport : public LspTransport
{
    Q_OBJECT
public:
    explicit MockTransport(QObject *parent = nullptr) : LspTransport(parent) {}

    void start() override {}
    void stop() override {}

    void send(const LspMessage &msg) override
    {
        m_sent.append(msg.payload);
    }

    /// Simulate an incoming message from the server.
    void injectMessage(const QJsonObject &payload)
    {
        emit messageReceived(LspMessage{payload});
    }

    QList<QJsonObject> m_sent;
};

// ── Tests ─────────────────────────────────────────────────────────────────────

class TestLspClient : public QObject
{
    Q_OBJECT

private slots:
    // ── Static helpers ────────────────────────────────────────────────────

    void pathToUri_unix()
    {
        const QString uri = LspClient::pathToUri("/home/user/main.cpp");
        QCOMPARE(uri, QStringLiteral("file:///home/user/main.cpp"));
    }

    void pathToUri_windows()
    {
        const QString uri = LspClient::pathToUri("C:/Users/dev/main.cpp");
        QVERIFY(uri.startsWith("file:///"));
        QVERIFY(uri.contains("main.cpp"));
    }

    void languageId_cpp()
    {
        QCOMPARE(LspClient::languageIdForPath("main.cpp"), QStringLiteral("cpp"));
        QCOMPARE(LspClient::languageIdForPath("header.h"), QStringLiteral("cpp"));
        QCOMPARE(LspClient::languageIdForPath("code.cxx"), QStringLiteral("cpp"));
        QCOMPARE(LspClient::languageIdForPath("impl.cc"),  QStringLiteral("cpp"));
        QCOMPARE(LspClient::languageIdForPath("hdr.hpp"),  QStringLiteral("cpp"));
        QCOMPARE(LspClient::languageIdForPath("tmpl.inl"), QStringLiteral("cpp"));
    }

    void languageId_python()
    {
        QCOMPARE(LspClient::languageIdForPath("script.py"),  QStringLiteral("python"));
        QCOMPARE(LspClient::languageIdForPath("gui.pyw"),    QStringLiteral("python"));
    }

    void languageId_web()
    {
        QCOMPARE(LspClient::languageIdForPath("app.js"),   QStringLiteral("javascript"));
        QCOMPARE(LspClient::languageIdForPath("app.mjs"),  QStringLiteral("javascript"));
        QCOMPARE(LspClient::languageIdForPath("app.ts"),   QStringLiteral("typescript"));
        QCOMPARE(LspClient::languageIdForPath("app.tsx"),  QStringLiteral("typescriptreact"));
        QCOMPARE(LspClient::languageIdForPath("app.jsx"),  QStringLiteral("javascriptreact"));
        QCOMPARE(LspClient::languageIdForPath("page.html"),QStringLiteral("html"));
        QCOMPARE(LspClient::languageIdForPath("style.css"),QStringLiteral("css"));
    }

    void languageId_misc()
    {
        QCOMPARE(LspClient::languageIdForPath("main.rs"),     QStringLiteral("rust"));
        QCOMPARE(LspClient::languageIdForPath("main.go"),     QStringLiteral("go"));
        QCOMPARE(LspClient::languageIdForPath("App.java"),    QStringLiteral("java"));
        QCOMPARE(LspClient::languageIdForPath("App.kt"),      QStringLiteral("kotlin"));
        QCOMPARE(LspClient::languageIdForPath("main.swift"),  QStringLiteral("swift"));
        QCOMPARE(LspClient::languageIdForPath("main.dart"),   QStringLiteral("dart"));
        QCOMPARE(LspClient::languageIdForPath("app.rb"),      QStringLiteral("ruby"));
        QCOMPARE(LspClient::languageIdForPath("index.php"),   QStringLiteral("php"));
        QCOMPARE(LspClient::languageIdForPath("init.lua"),    QStringLiteral("lua"));
        QCOMPARE(LspClient::languageIdForPath("build.scala"), QStringLiteral("scala"));
    }

    void languageId_config()
    {
        QCOMPARE(LspClient::languageIdForPath("data.json"),   QStringLiteral("json"));
        QCOMPARE(LspClient::languageIdForPath("feed.xml"),    QStringLiteral("xml"));
        QCOMPARE(LspClient::languageIdForPath("ci.yaml"),     QStringLiteral("yaml"));
        QCOMPARE(LspClient::languageIdForPath("ci.yml"),      QStringLiteral("yaml"));
        QCOMPARE(LspClient::languageIdForPath("pyproj.toml"), QStringLiteral("toml"));
        QCOMPARE(LspClient::languageIdForPath("run.sh"),      QStringLiteral("shellscript"));
        QCOMPARE(LspClient::languageIdForPath("run.bash"),    QStringLiteral("shellscript"));
        QCOMPARE(LspClient::languageIdForPath("run.ps1"),     QStringLiteral("powershell"));
        QCOMPARE(LspClient::languageIdForPath("README.md"),   QStringLiteral("markdown"));
    }

    void languageId_cmake()
    {
        QCOMPARE(LspClient::languageIdForPath("build.cmake"), QStringLiteral("cmake"));
        QCOMPARE(LspClient::languageIdForPath("CMakeLists.txt"), QStringLiteral("cmake"));
    }

    void languageId_csharp()
    {
        QCOMPARE(LspClient::languageIdForPath("Program.cs"), QStringLiteral("csharp"));
    }

    void languageId_unknown()
    {
        QCOMPARE(LspClient::languageIdForPath("notes.txt"), QStringLiteral("plaintext"));
        QCOMPARE(LspClient::languageIdForPath("file"),       QStringLiteral("plaintext"));
    }

    // ── JSON-RPC framing ──────────────────────────────────────────────────

    void initialize_sendsJsonRpc()
    {
        MockTransport transport;
        LspClient client(&transport);
        client.initialize("/workspace");

        QCOMPARE(transport.m_sent.size(), 1);
        const QJsonObject &msg = transport.m_sent.first();
        QCOMPARE(msg["jsonrpc"].toString(), QStringLiteral("2.0"));
        QCOMPARE(msg["method"].toString(), QStringLiteral("initialize"));
        QVERIFY(msg.contains("id"));
        QVERIFY(msg.contains("params"));

        const auto params = msg["params"].toObject();
        QVERIFY(params.contains("rootUri"));
        QVERIFY(params.contains("capabilities"));
        QCOMPARE(params["clientInfo"].toObject()["name"].toString(),
                 QStringLiteral("Exorcist"));
    }

    void initialize_response_emitsSignal()
    {
        MockTransport transport;
        LspClient client(&transport);
        QSignalSpy spy(&client, &LspClient::initialized);

        client.initialize("/workspace");
        QVERIFY(!client.isInitialized());

        // Simulate server response
        const int reqId = transport.m_sent.last()["id"].toInt();
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id",      reqId},
            {"result",  QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        QCOMPARE(spy.count(), 1);
        QVERIFY(client.isInitialized());

        // Should have sent "initialized" notification after response
        QCOMPARE(transport.m_sent.size(), 2);
        QCOMPARE(transport.m_sent[1]["method"].toString(),
                 QStringLiteral("initialized"));
    }

    void didOpen_sendsCorrectPayload()
    {
        MockTransport transport;
        LspClient client(&transport);

        // Force initialized state by simulating init handshake
        client.initialize("/ws");
        const int initId = transport.m_sent.last()["id"].toInt();
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"}, {"id", initId},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });
        transport.m_sent.clear();

        client.didOpen("file:///main.cpp", "cpp", "int main() {}", 1);

        QCOMPARE(transport.m_sent.size(), 1);
        const auto params = transport.m_sent[0]["params"].toObject();
        const auto td = params["textDocument"].toObject();
        QCOMPARE(td["uri"].toString(), QStringLiteral("file:///main.cpp"));
        QCOMPARE(td["languageId"].toString(), QStringLiteral("cpp"));
        QCOMPARE(td["text"].toString(), QStringLiteral("int main() {}"));
        QCOMPARE(td["version"].toInt(), 1);
    }

    void requestCompletion_beforeInit_ignored()
    {
        MockTransport transport;
        LspClient client(&transport);
        // Not initialized — request should be silently dropped
        client.requestCompletion("file:///a.cpp", 0, 0);
        // Only the initialize request should have been sent... but we haven't
        // called initialize either. So nothing should be sent.
        QCOMPARE(transport.m_sent.size(), 0);
    }

    void requestCompletion_sendsCorrectPayload()
    {
        MockTransport transport;
        LspClient client(&transport);

        // Initialize handshake
        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });
        transport.m_sent.clear();

        client.requestCompletion("file:///main.cpp", 5, 10, 1, ".");

        QCOMPARE(transport.m_sent.size(), 1);
        const auto &msg = transport.m_sent[0];
        QCOMPARE(msg["method"].toString(),
                 QStringLiteral("textDocument/completion"));

        const auto params = msg["params"].toObject();
        QCOMPARE(params["textDocument"].toObject()["uri"].toString(),
                 QStringLiteral("file:///main.cpp"));
        QCOMPARE(params["position"].toObject()["line"].toInt(), 5);
        QCOMPARE(params["position"].toObject()["character"].toInt(), 10);
        QCOMPARE(params["context"].toObject()["triggerCharacter"].toString(),
                 QStringLiteral("."));
    }

    void completionResponse_emitsSignal()
    {
        MockTransport transport;
        LspClient client(&transport);

        // Init
        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        QSignalSpy spy(&client, &LspClient::completionResult);
        client.requestCompletion("file:///a.cpp", 1, 2);
        const int reqId = transport.m_sent.last()["id"].toInt();

        // Simulate completion list response
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", reqId},
            {"result", QJsonObject{
                {"isIncomplete", true},
                {"items", QJsonArray{
                    QJsonObject{{"label", "foo"}},
                    QJsonObject{{"label", "bar"}},
                }},
            }},
        });

        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("file:///a.cpp"));
        QCOMPARE(args.at(1).toInt(), 1);  // line
        QCOMPARE(args.at(2).toInt(), 2);  // character
        QCOMPARE(args.at(3).toJsonArray().size(), 2);
        QCOMPARE(args.at(4).toBool(), true);  // isIncomplete
    }

    void hoverResponse_plainString()
    {
        MockTransport transport;
        LspClient client(&transport);

        // Init
        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        QSignalSpy spy(&client, &LspClient::hoverResult);
        client.requestHover("file:///a.cpp", 3, 5);
        const int reqId = transport.m_sent.last()["id"].toInt();

        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", reqId},
            {"result", QJsonObject{
                {"contents", "void foo()"},
            }},
        });

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(3).toString(), QStringLiteral("void foo()"));
    }

    void hoverResponse_markupContent()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        QSignalSpy spy(&client, &LspClient::hoverResult);
        client.requestHover("file:///a.cpp", 0, 0);
        const int reqId = transport.m_sent.last()["id"].toInt();

        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", reqId},
            {"result", QJsonObject{
                {"contents", QJsonObject{{"kind", "markdown"}, {"value", "```cpp\nint x;\n```"}}},
            }},
        });

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.takeFirst().at(3).toString().contains("int x;"));
    }

    void diagnosticsNotification()
    {
        MockTransport transport;
        LspClient client(&transport);

        QSignalSpy spy(&client, &LspClient::diagnosticsPublished);

        // Server pushes diagnostics (no prior request needed)
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"method", "textDocument/publishDiagnostics"},
            {"params", QJsonObject{
                {"uri", "file:///a.cpp"},
                {"diagnostics", QJsonArray{
                    QJsonObject{{"message", "undeclared identifier 'x'"}},
                }},
            }},
        });

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("file:///a.cpp"));
    }

    void definitionResponse_singleLocation()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        QSignalSpy spy(&client, &LspClient::definitionResult);
        client.requestDefinition("file:///a.cpp", 1, 5);
        const int reqId = transport.m_sent.last()["id"].toInt();

        // Server returns a single Location object (not array)
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", reqId},
            {"result", QJsonObject{
                {"uri", "file:///b.cpp"},
                {"range", QJsonObject{
                    {"start", QJsonObject{{"line", 10}, {"character", 0}}},
                    {"end",   QJsonObject{{"line", 10}, {"character", 5}}},
                }},
            }},
        });

        QCOMPARE(spy.count(), 1);
        // Should wrap single location in array
        QCOMPARE(spy.takeFirst().at(1).toJsonArray().size(), 1);
    }

    void shutdown_sendsExitAfterResponse()
    {
        MockTransport transport;
        LspClient client(&transport);

        // Init
        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });
        transport.m_sent.clear();
        QVERIFY(client.isInitialized());

        client.shutdown();
        QVERIFY(!client.isInitialized()); // set immediately

        QCOMPARE(transport.m_sent.size(), 1);
        QCOMPARE(transport.m_sent[0]["method"].toString(),
                 QStringLiteral("shutdown"));

        // Respond to shutdown
        const int shutId = transport.m_sent[0]["id"].toInt();
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", shutId},
            {"result", QJsonValue()},
        });

        // Should have sent "exit" notification
        QCOMPARE(transport.m_sent.size(), 2);
        QCOMPARE(transport.m_sent[1]["method"].toString(),
                 QStringLiteral("exit"));
    }

    void errorResponse_doesNotCrash()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        client.requestHover("file:///a.cpp", 0, 0);
        const int reqId = transport.m_sent.last()["id"].toInt();

        // Server returns an error
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", reqId},
            {"error", QJsonObject{{"code", -32601}, {"message", "Method not found"}}},
        });

        // Should not crash — verify by reaching this point
        QVERIFY(true);
    }

    void formattingResponse()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        QSignalSpy spy(&client, &LspClient::formattingResult);
        client.requestFormatting("file:///a.cpp", 4, true);
        const int reqId = transport.m_sent.last()["id"].toInt();

        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", reqId},
            {"result", QJsonArray{
                QJsonObject{
                    {"range", QJsonObject{
                        {"start", QJsonObject{{"line", 0}, {"character", 0}}},
                        {"end",   QJsonObject{{"line", 0}, {"character", 4}}},
                    }},
                    {"newText", "    "},
                },
            }},
        });

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(1).toJsonArray().size(), 1);
    }

    void requestIdIncrement()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        client.requestHover("file:///a.cpp", 0, 0);
        client.requestHover("file:///b.cpp", 1, 1);

        // IDs should be unique and incrementing
        const int id1 = transport.m_sent[transport.m_sent.size() - 2]["id"].toInt();
        const int id2 = transport.m_sent.last()["id"].toInt();
        QVERIFY(id2 > id1);
    }

    void requestInlayHints_sendsCorrectPayload()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });
        transport.m_sent.clear();

        client.requestInlayHints("file:///a.cpp", 0, 0, 50, 0);

        QCOMPARE(transport.m_sent.size(), 1);
        const auto params = transport.m_sent[0]["params"].toObject();
        QCOMPARE(params["textDocument"].toObject()["uri"].toString(),
                 QStringLiteral("file:///a.cpp"));
        const auto range = params["range"].toObject();
        QCOMPARE(range["start"].toObject()["line"].toInt(), 0);
        QCOMPARE(range["end"].toObject()["line"].toInt(), 50);
    }

    void inlayHintsResponse_emitsSignal()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        QSignalSpy spy(&client, &LspClient::inlayHintsResult);
        client.requestInlayHints("file:///a.cpp", 0, 0, 100, 0);
        const int reqId = transport.m_sent.last()["id"].toInt();

        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", reqId},
            {"result", QJsonArray{
                QJsonObject{
                    {"position", QJsonObject{{"line", 5}, {"character", 10}}},
                    {"label", "param: "},
                    {"paddingLeft", true},
                    {"paddingRight", false},
                },
                QJsonObject{
                    {"position", QJsonObject{{"line", 12}, {"character", 3}}},
                    {"label", QJsonArray{QJsonObject{{"value", "int"}}}},
                },
            }},
        });

        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("file:///a.cpp"));
        QCOMPARE(args.at(1).toJsonArray().size(), 2);
    }

    void requestTypeDefinition_sendsCorrectPayload()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });
        transport.m_sent.clear();

        client.requestTypeDefinition("file:///a.cpp", 7, 14);

        QCOMPARE(transport.m_sent.size(), 1);
        QCOMPARE(transport.m_sent[0]["method"].toString(),
                 QStringLiteral("textDocument/typeDefinition"));
        const auto params = transport.m_sent[0]["params"].toObject();
        QCOMPARE(params["textDocument"].toObject()["uri"].toString(),
                 QStringLiteral("file:///a.cpp"));
        QCOMPARE(params["position"].toObject()["line"].toInt(), 7);
        QCOMPARE(params["position"].toObject()["character"].toInt(), 14);
    }

    void typeDefinitionResponse_singleLocation()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        QSignalSpy spy(&client, &LspClient::typeDefinitionResult);
        client.requestTypeDefinition("file:///a.cpp", 3, 8);
        const int reqId = transport.m_sent.last()["id"].toInt();

        // Server returns single Location (not array)
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", reqId},
            {"result", QJsonObject{
                {"uri", "file:///types.h"},
                {"range", QJsonObject{
                    {"start", QJsonObject{{"line", 20}, {"character", 0}}},
                    {"end",   QJsonObject{{"line", 20}, {"character", 10}}},
                }},
            }},
        });

        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("file:///a.cpp"));
        // Single location should be wrapped in array
        QCOMPARE(args.at(1).toJsonArray().size(), 1);
    }

    void typeDefinitionResponse_arrayLocations()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        QSignalSpy spy(&client, &LspClient::typeDefinitionResult);
        client.requestTypeDefinition("file:///a.cpp", 1, 0);
        const int reqId = transport.m_sent.last()["id"].toInt();

        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", reqId},
            {"result", QJsonArray{
                QJsonObject{
                    {"uri", "file:///t1.h"},
                    {"range", QJsonObject{
                        {"start", QJsonObject{{"line", 5}, {"character", 0}}},
                        {"end",   QJsonObject{{"line", 5}, {"character", 8}}},
                    }},
                },
                QJsonObject{
                    {"uri", "file:///t2.h"},
                    {"range", QJsonObject{
                        {"start", QJsonObject{{"line", 10}, {"character", 0}}},
                        {"end",   QJsonObject{{"line", 10}, {"character", 6}}},
                    }},
                },
            }},
        });

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(1).toJsonArray().size(), 2);
    }

    void inlayHints_beforeInit_ignored()
    {
        MockTransport transport;
        LspClient client(&transport);

        // Not initialized — request should be silently ignored
        transport.m_sent.clear();
        client.requestInlayHints("file:///a.cpp", 0, 0, 50, 0);
        QCOMPARE(transport.m_sent.size(), 0);
    }

    void typeDefinition_beforeInit_ignored()
    {
        MockTransport transport;
        LspClient client(&transport);

        // Not initialized — request should be silently ignored
        transport.m_sent.clear();
        client.requestTypeDefinition("file:///a.cpp", 0, 0);
        QCOMPARE(transport.m_sent.size(), 0);
    }

    void typeDefinitionResponse_emptyResult()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        QSignalSpy spy(&client, &LspClient::typeDefinitionResult);
        client.requestTypeDefinition("file:///a.cpp", 0, 0);
        const int reqId = transport.m_sent.last()["id"].toInt();

        // Empty array response
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", reqId},
            {"result", QJsonArray{}},
        });

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(1).toJsonArray().size(), 0);
    }

    void inlayHintsResponse_emptyResult()
    {
        MockTransport transport;
        LspClient client(&transport);

        client.initialize("/ws");
        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", transport.m_sent.last()["id"].toInt()},
            {"result", QJsonObject{{"capabilities", QJsonObject{}}}},
        });

        QSignalSpy spy(&client, &LspClient::inlayHintsResult);
        client.requestInlayHints("file:///a.cpp", 0, 0, 100, 0);
        const int reqId = transport.m_sent.last()["id"].toInt();

        transport.injectMessage(QJsonObject{
            {"jsonrpc", "2.0"},
            {"id", reqId},
            {"result", QJsonArray{}},
        });

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(1).toJsonArray().size(), 0);
    }
};

QTEST_MAIN(TestLspClient)
#include "test_lspclient.moc"

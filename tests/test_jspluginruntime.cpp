#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "jspluginruntime.h"
#include "ihostservices.h"
#include "icommandservice.h"
#include "inotificationservice.h"

// ── Minimal mock services ────────────────────────────────────────────────────

class MockCommandService : public ICommandService
{
public:
    void registerCommand(const QString &id, const QString &title,
                         std::function<void()> handler) override
    {
        m_commands[id] = {title, std::move(handler)};
    }

    void unregisterCommand(const QString &id) override
    {
        m_commands.remove(id);
    }

    bool executeCommand(const QString &id) override
    {
        auto it = m_commands.find(id);
        if (it == m_commands.end()) return false;
        it->handler();
        return true;
    }

    bool hasCommand(const QString &id) const override
    {
        return m_commands.contains(id);
    }

    int commandCount() const { return m_commands.size(); }

    struct Entry { QString title; std::function<void()> handler; };
    QHash<QString, Entry> m_commands;
};

class MockNotificationService : public INotificationService
{
public:
    void info(const QString &text) override    { m_infos << text; }
    void warning(const QString &text) override { m_warnings << text; }
    void error(const QString &text) override   { m_errors << text; }
    void statusMessage(const QString &text, int timeoutMs = 5000) override
    {
        Q_UNUSED(timeoutMs);
        m_statusMessages << text;
    }

    QStringList m_infos;
    QStringList m_warnings;
    QStringList m_errors;
    QStringList m_statusMessages;
};

class MockHostServices : public IHostServices
{
public:
    MockCommandService      cmdService;
    MockNotificationService notifyService;

    ICommandService      *commands()      override { return &cmdService; }
    IWorkspaceService    *workspace()     override { return nullptr; }
    IEditorService       *editor()        override { return nullptr; }
    IViewService         *views()         override { return nullptr; }
    INotificationService *notifications() override { return &notifyService; }
    IGitService          *git()           override { return nullptr; }
    ITerminalService     *terminal()      override { return nullptr; }
    IDiagnosticsService  *diagnostics()   override { return nullptr; }
    ITaskService         *tasks()         override { return nullptr; }

    void registerService(const QString &, QObject *) override {}
    QObject *queryService(const QString &) override { return nullptr; }
};

// ── Helper to write a JS plugin to disk ──────────────────────────────────────

static void writeJsPlugin(const QString &dir, const QString &id,
                           const QString &jsCode,
                           const QStringList &permissions = {})
{
    QDir().mkpath(dir);

    QJsonObject obj;
    obj["id"]      = id;
    obj["name"]    = id;
    obj["version"] = "1.0.0";
    obj["entry"]   = "main.js";
    if (!permissions.isEmpty()) {
        QJsonArray arr;
        for (const auto &p : permissions) arr.append(p);
        obj["permissions"] = arr;
    }

    QFile manifest(dir + "/plugin.json");
    if (!manifest.open(QIODevice::WriteOnly)) return;
    manifest.write(QJsonDocument(obj).toJson());
    manifest.close();

    QFile js(dir + "/main.js");
    if (!js.open(QIODevice::WriteOnly)) return;
    js.write(jsCode.toUtf8());
    js.close();
}

// ── Test class ───────────────────────────────────────────────────────────────

class TestJsPluginRuntime : public QObject
{
    Q_OBJECT

private slots:

    // ── Basic loading ────────────────────────────────────────────────────

    void loadMinimalPlugin()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeJsPlugin(tmp.path() + "/hello", "test.hello",
                       "function initialize() {}",
                       {"commands", "notifications"});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        QCOMPARE(runtime.loadPluginsFrom(tmp.path()), 1);
        QCOMPARE(runtime.plugins().size(), 1u);
        QCOMPARE(runtime.plugins()[0].info.id, QStringLiteral("test.hello"));
    }

    void loadSkipsMissingManifest()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QDir().mkpath(tmp.path() + "/no-manifest");

        QFile js(tmp.path() + "/no-manifest/main.js");
        js.open(QIODevice::WriteOnly);
        js.write("function initialize() {}");
        js.close();

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        QCOMPARE(runtime.loadPluginsFrom(tmp.path()), 0);
    }

    void loadSkipsMissingMainJs()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QDir().mkpath(tmp.path() + "/no-script");

        QJsonObject obj;
        obj["id"] = "test.noscript";
        obj["name"] = "No Script";
        QFile manifest(tmp.path() + "/no-script/plugin.json");
        manifest.open(QIODevice::WriteOnly);
        manifest.write(QJsonDocument(obj).toJson());
        manifest.close();

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        QCOMPARE(runtime.loadPluginsFrom(tmp.path()), 0);
    }

    void loadEmptyDirectoryReturnsZero()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        QCOMPARE(runtime.loadPluginsFrom(tmp.path()), 0);
    }

    void loadNonexistentDirectoryReturnsZero()
    {
        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        QCOMPARE(runtime.loadPluginsFrom("/this/path/does/not/exist"), 0);
    }

    // ── Initialize / Shutdown ────────────────────────────────────────────

    void initializeShutsDown()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeJsPlugin(tmp.path() + "/lifecycle", "test.lifecycle",
                       "var inited = false; var shut = false;\n"
                       "function initialize() { inited = true; }\n"
                       "function shutdown()   { shut = true; }",
                       {"commands"});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        runtime.loadPluginsFrom(tmp.path());
        runtime.initializeAll();
        QVERIFY(runtime.plugins()[0].initialized);

        runtime.shutdownAll();
        QCOMPARE(runtime.plugins().size(), 0u);
    }

    // ── Command registration ─────────────────────────────────────────────

    void commandRegisterAndExecute()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeJsPlugin(tmp.path() + "/cmd", "test.cmd",
                       "function initialize() {\n"
                       "  ex.commands.register('test.greet', 'Greet', function() {\n"
                       "    ex.notify.info('Hello from JS!');\n"
                       "  });\n"
                       "}",
                       {"commands", "notifications"});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        runtime.loadPluginsFrom(tmp.path());
        runtime.initializeAll();

        QVERIFY(host.cmdService.hasCommand("test.greet"));
        QVERIFY(host.cmdService.executeCommand("test.greet"));
        QCOMPARE(host.notifyService.m_infos.size(), 1);
        QCOMPARE(host.notifyService.m_infos.first(), QStringLiteral("Hello from JS!"));
    }

    void commandUnregisteredOnShutdown()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeJsPlugin(tmp.path() + "/cmd2", "test.cmd2",
                       "function initialize() {\n"
                       "  ex.commands.register('test.foo', 'Foo', function() {});\n"
                       "}",
                       {"commands"});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        runtime.loadPluginsFrom(tmp.path());
        runtime.initializeAll();
        QVERIFY(host.cmdService.hasCommand("test.foo"));

        runtime.shutdownAll();
        QVERIFY(!host.cmdService.hasCommand("test.foo"));
    }

    // ── Notifications ────────────────────────────────────────────────────

    void notifyAllLevels()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeJsPlugin(tmp.path() + "/notify", "test.notify",
                       "function initialize() {\n"
                       "  ex.notify.info('INFO');\n"
                       "  ex.notify.warning('WARN');\n"
                       "  ex.notify.error('ERR');\n"
                       "  ex.notify.status('STATUS', 3000);\n"
                       "}",
                       {"notifications"});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        runtime.loadPluginsFrom(tmp.path());
        runtime.initializeAll();

        QCOMPARE(host.notifyService.m_infos, QStringList{"INFO"});
        QCOMPARE(host.notifyService.m_warnings, QStringList{"WARN"});
        QCOMPARE(host.notifyService.m_errors, QStringList{"ERR"});
        QCOMPARE(host.notifyService.m_statusMessages, QStringList{"STATUS"});
    }

    // ── Permission enforcement ───────────────────────────────────────────

    void permissionDeniedNo_ExNamespace()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // Plugin claims no permissions → ex.commands should not exist
        writeJsPlugin(tmp.path() + "/noperm", "test.noperm",
                       "function initialize() {\n"
                       "  if (typeof ex.commands !== 'undefined') {\n"
                       "    ex.notify.info('SHOULD NOT SEE THIS');\n"
                       "  }\n"
                       "}",
                       {});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        runtime.loadPluginsFrom(tmp.path());
        runtime.initializeAll();

        // Without notifications permission, notify is also absent
        QCOMPARE(host.notifyService.m_infos.size(), 0);
    }

    // ── Script syntax error ──────────────────────────────────────────────

    void syntaxErrorCaptured()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeJsPlugin(tmp.path() + "/bad", "test.bad",
                       "function initialize( { broken syntax",
                       {"commands"});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        QCOMPARE(runtime.loadPluginsFrom(tmp.path()), 0);
        QVERIFY(!runtime.errors().isEmpty());
    }

    // ── Events ───────────────────────────────────────────────────────────

    void eventsFired()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeJsPlugin(tmp.path() + "/events", "test.events",
                       "function initialize() {\n"
                       "  ex.events.on('fileSaved', function(path) {\n"
                       "    ex.notify.info('Saved: ' + path);\n"
                       "  });\n"
                       "}",
                       {"events", "notifications"});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        runtime.loadPluginsFrom(tmp.path());
        runtime.initializeAll();

        runtime.fireEvent("fileSaved", {"test.cpp"});
        QCOMPARE(host.notifyService.m_infos.size(), 1);
        QCOMPARE(host.notifyService.m_infos.first(), QStringLiteral("Saved: test.cpp"));
    }

    void eventsOff()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeJsPlugin(tmp.path() + "/evoff", "test.evoff",
                       "function initialize() {\n"
                       "  ex.events.on('tick', function() {\n"
                       "    ex.notify.info('tick');\n"
                       "  });\n"
                       "  ex.events.off('tick');\n"
                       "}",
                       {"events", "notifications"});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        runtime.loadPluginsFrom(tmp.path());
        runtime.initializeAll();

        runtime.fireEvent("tick");
        QCOMPARE(host.notifyService.m_infos.size(), 0);
    }

    // ── Multiple plugins ─────────────────────────────────────────────────

    void multiplePluginsLoaded()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeJsPlugin(tmp.path() + "/p1", "test.p1",
                       "function initialize() { ex.notify.info('P1'); }",
                       {"notifications"});
        writeJsPlugin(tmp.path() + "/p2", "test.p2",
                       "function initialize() { ex.notify.info('P2'); }",
                       {"notifications"});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        QCOMPARE(runtime.loadPluginsFrom(tmp.path()), 2);
        runtime.initializeAll();
        QCOMPARE(host.notifyService.m_infos.size(), 2);
        QVERIFY(host.notifyService.m_infos.contains("P1"));
        QVERIFY(host.notifyService.m_infos.contains("P2"));
    }

    // ── Reload ───────────────────────────────────────────────────────────

    void reloadPlugin()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeJsPlugin(tmp.path() + "/rel", "test.rel",
                       "function initialize() { ex.notify.info('v1'); }",
                       {"notifications"});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        runtime.loadPluginsFrom(tmp.path());
        runtime.initializeAll();
        QCOMPARE(host.notifyService.m_infos, QStringList{"v1"});

        // Overwrite the script
        writeJsPlugin(tmp.path() + "/rel", "test.rel",
                       "function initialize() { ex.notify.info('v2'); }",
                       {"notifications"});

        host.notifyService.m_infos.clear();
        QVERIFY(runtime.reloadPlugin("test.rel"));
        QCOMPARE(host.notifyService.m_infos, QStringList{"v2"});
    }

    // ── Logging (smoke test — output goes to qDebug/qInfo etc.) ──────────

    void logDoesNotCrash()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeJsPlugin(tmp.path() + "/lg", "test.lg",
                       "function initialize() {\n"
                       "  ex.log.debug('d');\n"
                       "  ex.log.info('i');\n"
                       "  ex.log.warning('w');\n"
                       "  ex.log.error('e');\n"
                       "}",
                       {"logging"});

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        runtime.loadPluginsFrom(tmp.path());
        runtime.initializeAll();
        // If we get here without crashing, the test passes
        QVERIFY(true);
    }

    // ── Missing plugin id in manifest ────────────────────────────────────

    void missingIdSkipsPlugin()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QDir().mkpath(tmp.path() + "/noid");

        QJsonObject obj;
        obj["name"] = "No ID";
        QFile manifest(tmp.path() + "/noid/plugin.json");
        manifest.open(QIODevice::WriteOnly);
        manifest.write(QJsonDocument(obj).toJson());
        manifest.close();

        QFile js(tmp.path() + "/noid/main.js");
        js.open(QIODevice::WriteOnly);
        js.write("function initialize() {}");
        js.close();

        MockHostServices host;
        jssdk::JsPluginRuntime runtime(&host);

        QCOMPARE(runtime.loadPluginsFrom(tmp.path()), 0);
        QVERIFY(!runtime.errors().isEmpty());
    }
};

QTEST_MAIN(TestJsPluginRuntime)
#include "test_jspluginruntime.moc"

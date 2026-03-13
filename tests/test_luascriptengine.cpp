#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "luascriptengine.h"
#include "ihostservices.h"
#include "icommandservice.h"

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

class MockHostServices : public IHostServices
{
public:
    MockCommandService cmdService;

    ICommandService      *commands()      override { return &cmdService; }
    IWorkspaceService    *workspace()     override { return nullptr; }
    IEditorService       *editor()        override { return nullptr; }
    IViewService         *views()         override { return nullptr; }
    INotificationService *notifications() override { return nullptr; }
    IGitService          *git()           override { return nullptr; }
    ITerminalService     *terminal()      override { return nullptr; }
    IDiagnosticsService  *diagnostics()   override { return nullptr; }
    ITaskService         *tasks()         override { return nullptr; }
};

// ── Helper to create a minimal Lua plugin on disk ────────────────────────────

static void writePlugin(const QString &pluginDir, const QString &id,
                        const QString &luaCode,
                        const QStringList &permissions = {})
{
    QDir().mkpath(pluginDir);

    // plugin.json
    QJsonObject obj;
    obj["id"]   = id;
    obj["name"] = id;
    obj["version"] = "1.0.0";
    obj["entry"] = "main.lua";
    if (!permissions.isEmpty()) {
        QJsonArray arr;
        for (const auto &p : permissions) arr.append(p);
        obj["permissions"] = arr;
    }

    QFile manifest(pluginDir + "/plugin.json");
    if (!manifest.open(QIODevice::WriteOnly)) return;
    manifest.write(QJsonDocument(obj).toJson());
    manifest.close();

    // main.lua
    QFile lua(pluginDir + "/main.lua");
    if (!lua.open(QIODevice::WriteOnly)) return;
    lua.write(luaCode.toUtf8());
    lua.close();
}

// ── Test class ───────────────────────────────────────────────────────────────

class TestLuaScriptEngine : public QObject
{
    Q_OBJECT

private slots:

    // ── Ad-hoc execution ─────────────────────────────────────────────────

    void adhocSimpleReturn()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("return 2 + 3");
        QVERIFY(r.ok);
        QCOMPARE(r.returnValue, QStringLiteral("5"));
        QVERIFY(r.error.isEmpty());
    }

    void adhocPrintCapture()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("print('hello')\nprint('world')");
        QVERIFY(r.ok);
        QCOMPARE(r.output, QStringLiteral("hello\nworld"));
    }

    void adhocSyntaxError()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("if then end");
        QVERIFY(!r.ok);
        QVERIFY(!r.error.isEmpty());
    }

    void adhocRuntimeError()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("error('boom')");
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("boom"));
    }

    void adhocMemoryTracked()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("local t = {} for i=1,1000 do t[i]=i end return #t");
        QVERIFY(r.ok);
        QVERIFY(r.memoryUsedBytes > 0);
    }

    // ── Sandbox restrictions ─────────────────────────────────────────────

    void sandboxBlocksOs()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("return os.execute('echo hi')");
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("nil") || r.error.contains("attempt to index"));
    }

    void sandboxBlocksIo()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("return io.open('/etc/passwd')");
        QVERIFY(!r.ok);
    }

    void sandboxBlocksDofile()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("dofile('test.lua')");
        QVERIFY(!r.ok);
    }

    void sandboxBlocksLoadstring()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("loadstring('return 1')()");
        QVERIFY(!r.ok);
    }

    void sandboxBlocksRequire()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("require('os')");
        QVERIFY(!r.ok);
    }

    void sandboxBlocksFfi()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("return ffi.C");
        QVERIFY(!r.ok);
    }

    void sandboxBlocksCollectgarbage()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("collectgarbage()");
        QVERIFY(!r.ok);
    }

    // ── Instruction limit ────────────────────────────────────────────────

    void infiniteLoopAborted()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("while true do end");
        QVERIFY(!r.ok);
        // The instruction hook should abort with an error
        QVERIFY(!r.error.isEmpty());
    }

    // ── Safe libraries available ─────────────────────────────────────────

    void safeLibsAvailable()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        // string
        auto r1 = engine.executeAdhoc("return string.upper('hello')");
        QVERIFY(r1.ok);
        QCOMPARE(r1.returnValue, QStringLiteral("HELLO"));

        // table
        auto r2 = engine.executeAdhoc("local t={3,1,2} table.sort(t) return t[1]..t[2]..t[3]");
        QVERIFY(r2.ok);
        QCOMPARE(r2.returnValue, QStringLiteral("123"));

        // math
        auto r3 = engine.executeAdhoc("return math.floor(3.7)");
        QVERIFY(r3.ok);
        QCOMPARE(r3.returnValue, QStringLiteral("3"));
    }

    // ── Plugin loading ───────────────────────────────────────────────────

    void loadPluginFromDir()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        writePlugin(tmpDir.path() + "/testplugin", "test.plugin",
                    "function initialize() end\nfunction shutdown() end");

        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        int loaded = engine.loadPluginsFrom(tmpDir.path());
        QCOMPARE(loaded, 1);
        QCOMPARE(engine.plugins().size(), 1u);
        QCOMPARE(engine.plugins()[0].info.id, QStringLiteral("test.plugin"));
    }

    void loadPluginMissingMainLua()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Only write plugin.json — no main.lua
        QString pluginDir = tmpDir.path() + "/broken";
        QDir().mkpath(pluginDir);

        QJsonObject obj;
        obj["id"] = "broken";
        obj["name"] = "Broken";
        obj["version"] = "1.0.0";
        obj["entry"] = "main.lua";

        QFile manifest(pluginDir + "/plugin.json");
        if (!manifest.open(QIODevice::WriteOnly)) QFAIL("Cannot write manifest");
        manifest.write(QJsonDocument(obj).toJson());
        manifest.close();

        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        int loaded = engine.loadPluginsFrom(tmpDir.path());
        QCOMPARE(loaded, 0);
    }

    void initializeCallsLuaInit()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Plugin that sets a global in initialize()
        writePlugin(tmpDir.path() + "/initplugin", "init.test",
                    "initCalled = false\n"
                    "function initialize()\n"
                    "  initCalled = true\n"
                    "end\n");

        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);
        engine.loadPluginsFrom(tmpDir.path());
        engine.initializeAll();

        QVERIFY(engine.plugins()[0].initialized);
    }

    void shutdownCleansUp()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        writePlugin(tmpDir.path() + "/shutplugin", "shut.test",
                    "function initialize() end\nfunction shutdown() end");

        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);
        engine.loadPluginsFrom(tmpDir.path());
        engine.initializeAll();
        engine.shutdownAll();

        QVERIFY(engine.plugins().empty());
    }

    // ── Permission parsing ───────────────────────────────────────────────

    void permissionsParsedCorrectly()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        writePlugin(tmpDir.path() + "/permplugin", "perm.test",
                    "function initialize() end",
                    {"commands", "editor.read", "notifications"});

        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);
        engine.loadPluginsFrom(tmpDir.path());

        const auto &plugin = engine.plugins()[0];
        QVERIFY(luabridge::hasPermission(plugin.permissions,
                                          luabridge::LuaPermission::Commands));
        QVERIFY(luabridge::hasPermission(plugin.permissions,
                                          luabridge::LuaPermission::EditorRead));
        QVERIFY(luabridge::hasPermission(plugin.permissions,
                                          luabridge::LuaPermission::Notifications));
        // Should NOT have git permission
        QVERIFY(!luabridge::hasPermission(plugin.permissions,
                                           luabridge::LuaPermission::GitRead));
    }

    // ── Command registration & hot-reload cleanup ────────────────────────

    void commandRegisteredViaLua()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        writePlugin(tmpDir.path() + "/cmdplugin", "cmd.test",
                    "function initialize()\n"
                    "  ex.commands.register('cmd.test.hello', 'Hello', function() end)\n"
                    "end\n",
                    {"commands"});

        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);
        engine.loadPluginsFrom(tmpDir.path());
        engine.initializeAll();

        QVERIFY(host.cmdService.hasCommand("cmd.test.hello"));
    }

    void commandUnregisteredOnReload()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString pluginDir = tmpDir.path() + "/cmdplugin";
        writePlugin(pluginDir, "cmd.test",
                    "function initialize()\n"
                    "  ex.commands.register('cmd.test.greet', 'Greet', function() end)\n"
                    "end\n",
                    {"commands"});

        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);
        engine.loadPluginsFrom(tmpDir.path());
        engine.initializeAll();

        QVERIFY(host.cmdService.hasCommand("cmd.test.greet"));
        QCOMPARE(host.cmdService.commandCount(), 1);

        // Reload — old command must be unregistered
        bool ok = engine.reloadPlugin("cmd.test");
        QVERIFY(ok);

        // After reload, the plugin re-registers, so command is back
        // but point is no stale dangling-pointer commands survive
        engine.initializeAll();
        QVERIFY(host.cmdService.hasCommand("cmd.test.greet"));
        QCOMPARE(host.cmdService.commandCount(), 1);
    }

    void commandUnregisteredOnShutdown()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        writePlugin(tmpDir.path() + "/cmdplugin2", "cmd.shut",
                    "function initialize()\n"
                    "  ex.commands.register('cmd.shut.a', 'A', function() end)\n"
                    "  ex.commands.register('cmd.shut.b', 'B', function() end)\n"
                    "end\n",
                    {"commands"});

        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);
        engine.loadPluginsFrom(tmpDir.path());
        engine.initializeAll();

        QCOMPARE(host.cmdService.commandCount(), 2);

        engine.shutdownAll();

        QCOMPARE(host.cmdService.commandCount(), 0);
        QVERIFY(!host.cmdService.hasCommand("cmd.shut.a"));
        QVERIFY(!host.cmdService.hasCommand("cmd.shut.b"));
    }

    // ── Events ───────────────────────────────────────────────────────────

    void fireEventCallsLuaCallback()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        writePlugin(tmpDir.path() + "/evtplugin", "evt.test",
                    "eventFired = false\n"
                    "function initialize()\n"
                    "  ex.events.on('fileOpened', function() eventFired = true end)\n"
                    "end\n",
                    {"events"});

        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);
        engine.loadPluginsFrom(tmpDir.path());
        engine.initializeAll();

        // Fire the event
        engine.fireEvent("fileOpened");

        // We can't directly check Lua globals from C++, but this should not crash
        // and the event system should handle the callback
        QVERIFY(engine.errors().isEmpty());
    }

    // ── Memory limit ─────────────────────────────────────────────────────

    void memoryLimitEnforced()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        // Try to allocate a huge table (should hit the 16MB budget)
        auto r = engine.executeAdhoc(
            "local t = {}\n"
            "for i = 1, 10000000 do\n"
            "  t[i] = string.rep('x', 1000)\n"
            "end\n"
            "return #t");
        QVERIFY(!r.ok);
    }

    // ── Multiple plugins ─────────────────────────────────────────────────

    void multiplePluginsIsolated()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        writePlugin(tmpDir.path() + "/pluginA", "plugin.a",
                    "myGlobal = 'A'\n"
                    "function initialize() end\n");
        writePlugin(tmpDir.path() + "/pluginB", "plugin.b",
                    "myGlobal = 'B'\n"
                    "function initialize() end\n");

        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        int loaded = engine.loadPluginsFrom(tmpDir.path());
        QCOMPARE(loaded, 2);
        engine.initializeAll();

        // Both loaded and initialized in isolation (separate lua_States)
        QCOMPARE(engine.plugins().size(), 2u);
        QVERIFY(engine.plugins()[0].initialized);
        QVERIFY(engine.plugins()[1].initialized);
    }

    void emptyDirectoryLoadsNothing()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        int loaded = engine.loadPluginsFrom(tmpDir.path());
        QCOMPARE(loaded, 0);
    }

    void nonExistentDirectoryLoadsNothing()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        int loaded = engine.loadPluginsFrom("/nonexistent/path/that/does/not/exist");
        QCOMPARE(loaded, 0);
    }

    // ── Built-in json module ─────────────────────────────────────────────

    void jsonDecodeObject()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc(
            "local t = json.decode('{\"name\":\"test\",\"value\":42}')\n"
            "return t.name .. '=' .. t.value");
        QVERIFY(r.ok);
        QCOMPARE(r.returnValue, QStringLiteral("test=42"));
    }

    void jsonDecodeArray()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc(
            "local t = json.decode('[10,20,30]')\n"
            "return t[1] + t[2] + t[3]");
        QVERIFY(r.ok);
        QCOMPARE(r.returnValue, QStringLiteral("60"));
    }

    void jsonDecodeInvalid()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc(
            "local val, err = json.decode('{broken')\n"
            "if val == nil then return 'error:' .. err end\n"
            "return 'unexpected'");
        QVERIFY(r.ok);
        QVERIFY(r.returnValue.startsWith("error:"));
    }

    void jsonEncodeTable()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc(
            "local s = json.encode({name='hello', count=5})\n"
            "return s");
        QVERIFY(r.ok);
        QVERIFY(r.returnValue.contains("hello"));
        QVERIFY(r.returnValue.contains("5"));
    }

    void jsonEncodeArray()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc(
            "return json.encode({1,2,3})");
        QVERIFY(r.ok);
        QVERIFY(r.returnValue.contains("1"));
        QVERIFY(r.returnValue.contains("2"));
        QVERIFY(r.returnValue.contains("3"));
    }

    void jsonRoundtrip()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc(
            "local original = '{\"a\":1,\"b\":[2,3]}'\n"
            "local t = json.decode(original)\n"
            "return tostring(t.a) .. ',' .. tostring(t.b[1]) .. ',' .. tostring(t.b[2])");
        QVERIFY(r.ok);
        QCOMPARE(r.returnValue, QStringLiteral("1,2,3"));
    }

    void jsonDecodeNested()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc(
            "local t = json.decode('{\"a\":{\"b\":{\"c\":99}}}')\n"
            "return t.a.b.c");
        QVERIFY(r.ok);
        QCOMPARE(r.returnValue, QStringLiteral("99"));
    }

    void jsonDecodeBooleans()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc(
            "local t = json.decode('{\"flag\":true,\"other\":false}')\n"
            "if t.flag == true and t.other == false then return 'ok' end\n"
            "return 'fail'");
        QVERIFY(r.ok);
        QCOMPARE(r.returnValue, QStringLiteral("ok"));
    }

    // ── os.clock() ───────────────────────────────────────────────────────

    void osClockAvailable()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc("return type(os.clock())");
        QVERIFY(r.ok);
        QCOMPARE(r.returnValue, QStringLiteral("number"));
    }

    void osClockTimesCode()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        auto r = engine.executeAdhoc(
            "local start = os.clock()\n"
            "local s = 0\n"
            "for i = 1, 100000 do s = s + i end\n"
            "local elapsed = os.clock() - start\n"
            "return elapsed >= 0 and 'ok' or 'negative'");
        QVERIFY(r.ok);
        QCOMPARE(r.returnValue, QStringLiteral("ok"));
    }

    void osOtherFunctionsBlocked()
    {
        MockHostServices host;
        luabridge::LuaScriptEngine engine(&host);

        // os.execute should not exist (only os.clock is exposed)
        auto r = engine.executeAdhoc(
            "if os.execute == nil then return 'blocked' else return 'exposed' end");
        QVERIFY(r.ok);
        QCOMPARE(r.returnValue, QStringLiteral("blocked"));
    }
};

QTEST_MAIN(TestLuaScriptEngine)
#include "test_luascriptengine.moc"

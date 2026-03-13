#include <QTest>
#include <QJsonObject>
#include <QJsonArray>

#include "agent/itool.h"

// ── Fake tool for testing ─────────────────────────────────────────────────────

class FakeTool : public ITool
{
public:
    FakeTool(const QString &name, AgentToolPermission perm,
             const QStringList &contexts = {})
    {
        m_spec.name = name;
        m_spec.description = name + " description";
        m_spec.permission = perm;
        m_spec.contexts = contexts;
        m_spec.inputSchema = QJsonObject{
            {"type", "object"},
            {"properties", QJsonObject{
                {"path", QJsonObject{{"type", "string"}, {"description", "File path"}}},
            }},
            {"required", QJsonArray{"path"}},
        };
    }

    ToolSpec spec() const override { return m_spec; }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        m_lastArgs = args;
        return {true, {}, "ok", {}};
    }

    QJsonObject m_lastArgs;

private:
    ToolSpec m_spec;
};

// ── Tests ─────────────────────────────────────────────────────────────────────

class TestToolRegistry : public QObject
{
    Q_OBJECT

private slots:

    void registerAndRetrieve()
    {
        ToolRegistry reg;
        reg.registerTool(std::make_unique<FakeTool>("read_file", AgentToolPermission::ReadOnly));

        QVERIFY(reg.hasTool("read_file"));
        QVERIFY(reg.tool("read_file") != nullptr);
        QCOMPARE(reg.tool("read_file")->spec().name, QStringLiteral("read_file"));
    }

    void unknownReturnsNull()
    {
        ToolRegistry reg;
        QVERIFY(!reg.hasTool("nonexistent"));
        QVERIFY(reg.tool("nonexistent") == nullptr);
    }

    void removeTool()
    {
        ToolRegistry reg;
        reg.registerTool(std::make_unique<FakeTool>("t1", AgentToolPermission::ReadOnly));
        QVERIFY(reg.hasTool("t1"));

        reg.removeTool("t1");
        QVERIFY(!reg.hasTool("t1"));
    }

    void removeNonexistent_noCrash()
    {
        ToolRegistry reg;
        reg.removeTool("doesnt_exist");
        QVERIFY(true);
    }

    void toolNames()
    {
        ToolRegistry reg;
        reg.registerTool(std::make_unique<FakeTool>("alpha", AgentToolPermission::ReadOnly));
        reg.registerTool(std::make_unique<FakeTool>("beta",  AgentToolPermission::SafeMutate));

        auto names = reg.toolNames();
        names.sort();
        QCOMPARE(names, QStringList({"alpha", "beta"}));
    }

    void registerNull_ignored()
    {
        ToolRegistry reg;
        reg.registerTool(nullptr);
        QCOMPARE(reg.toolNames().size(), 0);
    }

    void overwrite_replaces()
    {
        ToolRegistry reg;
        reg.registerTool(std::make_unique<FakeTool>("t", AgentToolPermission::ReadOnly));
        QCOMPARE(reg.tool("t")->spec().permission, AgentToolPermission::ReadOnly);

        reg.registerTool(std::make_unique<FakeTool>("t", AgentToolPermission::Dangerous));
        QCOMPARE(reg.tool("t")->spec().permission, AgentToolPermission::Dangerous);
    }

    // ── Definitions ───────────────────────────────────────────────────────

    void allDefinitions()
    {
        ToolRegistry reg;
        reg.registerTool(std::make_unique<FakeTool>("a", AgentToolPermission::ReadOnly));
        reg.registerTool(std::make_unique<FakeTool>("b", AgentToolPermission::Dangerous));

        auto defs = reg.allDefinitions();
        QCOMPARE(defs.size(), 2);
    }

    void definitions_filteredByPermission()
    {
        ToolRegistry reg;
        reg.registerTool(std::make_unique<FakeTool>("read", AgentToolPermission::ReadOnly));
        reg.registerTool(std::make_unique<FakeTool>("write", AgentToolPermission::SafeMutate));
        reg.registerTool(std::make_unique<FakeTool>("danger", AgentToolPermission::Dangerous));

        // ReadOnly filter — only "read" passes
        auto defs = reg.definitions(AgentToolPermission::ReadOnly);
        QCOMPARE(defs.size(), 1);
        QCOMPARE(defs[0].name, QStringLiteral("read"));

        // SafeMutate filter — "read" + "write"
        defs = reg.definitions(AgentToolPermission::SafeMutate);
        QCOMPARE(defs.size(), 2);

        // Dangerous filter — all pass
        defs = reg.definitions(AgentToolPermission::Dangerous);
        QCOMPARE(defs.size(), 3);
    }

    void definitions_filteredByDisabled()
    {
        ToolRegistry reg;
        reg.registerTool(std::make_unique<FakeTool>("a", AgentToolPermission::ReadOnly));
        reg.registerTool(std::make_unique<FakeTool>("b", AgentToolPermission::ReadOnly));
        reg.registerTool(std::make_unique<FakeTool>("c", AgentToolPermission::ReadOnly));

        reg.setDisabledTools({"b"});
        QVERIFY(!reg.isToolEnabled("b"));
        QVERIFY(reg.isToolEnabled("a"));

        auto defs = reg.definitions(AgentToolPermission::Dangerous);
        QCOMPARE(defs.size(), 2);
        for (const auto &d : defs)
            QVERIFY(d.name != "b");
    }

    void definitions_filteredByContext()
    {
        ToolRegistry reg;
        // Universal tool (empty contexts)
        reg.registerTool(std::make_unique<FakeTool>("universal", AgentToolPermission::ReadOnly));
        // cpp-only tool
        reg.registerTool(std::make_unique<FakeTool>("cpp_lint", AgentToolPermission::ReadOnly,
                                                     QStringList{"cpp"}));
        // python-only tool
        reg.registerTool(std::make_unique<FakeTool>("py_lint", AgentToolPermission::ReadOnly,
                                                     QStringList{"python"}));

        reg.setActiveContexts({"cpp"});
        auto defs = reg.definitions(AgentToolPermission::Dangerous);

        // Universal + cpp_lint should pass; py_lint should not
        QCOMPARE(defs.size(), 2);
        QStringList names;
        for (const auto &d : defs) names.append(d.name);
        names.sort();
        QCOMPARE(names, QStringList({"cpp_lint", "universal"}));
    }

    void definitions_noActiveContext_allPass()
    {
        ToolRegistry reg;
        reg.registerTool(std::make_unique<FakeTool>("universal", AgentToolPermission::ReadOnly));
        reg.registerTool(std::make_unique<FakeTool>("scoped", AgentToolPermission::ReadOnly,
                                                     QStringList{"cpp"}));

        // No active contexts set — scoped tools should still pass
        // (only filtered when activeContexts is non-empty)
        auto defs = reg.definitions(AgentToolPermission::Dangerous);
        QCOMPARE(defs.size(), 2);
    }

    // ── ToolDefinition structure ──────────────────────────────────────────

    void definitionHasParameters()
    {
        ToolRegistry reg;
        reg.registerTool(std::make_unique<FakeTool>("t", AgentToolPermission::ReadOnly));

        auto defs = reg.allDefinitions();
        QCOMPARE(defs.size(), 1);
        QCOMPARE(defs[0].parameters.size(), 1);
        QCOMPARE(defs[0].parameters[0].name, QStringLiteral("path"));
        QCOMPARE(defs[0].parameters[0].type, QStringLiteral("string"));
        QVERIFY(defs[0].parameters[0].required);
    }

    // ── Tool invocation ───────────────────────────────────────────────────

    void invokeTool()
    {
        ToolRegistry reg;
        auto fakeTool = std::make_unique<FakeTool>("t", AgentToolPermission::ReadOnly);
        auto *rawPtr = fakeTool.get();
        reg.registerTool(std::move(fakeTool));

        QJsonObject args{{"path", "/test.txt"}};
        auto result = reg.tool("t")->invoke(args);

        QVERIFY(result.ok);
        QCOMPARE(result.textContent, QStringLiteral("ok"));
        QCOMPARE(rawPtr->m_lastArgs["path"].toString(), QStringLiteral("/test.txt"));
    }
};

QTEST_MAIN(TestToolRegistry)
#include "test_toolregistry.moc"

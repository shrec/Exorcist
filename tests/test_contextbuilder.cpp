#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "agent/contextbuilder.h"
#include "agent/icontextprovider.h"
#include "core/ifilesystem.h"
#include "aiinterface.h"

// ── Mock file system ──────────────────────────────────────────────────────────

class MockFileSystem : public IFileSystem
{
public:
    QHash<QString, QString> m_files;

    bool exists(const QString &path) const override
    {
        return m_files.contains(path);
    }

    QString readTextFile(const QString &path, QString *error) const override
    {
        if (!m_files.contains(path)) {
            if (error) *error = "File not found";
            return {};
        }
        return m_files[path];
    }

    bool writeTextFile(const QString &, const QString &, QString *) override
    {
        return false;
    }

    QStringList listDir(const QString &, QString *) const override
    {
        return {};
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

class TestContextBuilder : public QObject
{
    Q_OBJECT

private slots:

    // ── Basic construction ────────────────────────────────────────────────

    void emptyContextNoCallbacks()
    {
        ContextBuilder builder;
        auto ctx = builder.buildContext();
        QVERIFY(ctx.items.isEmpty());
    }

    // ── Active file ───────────────────────────────────────────────────────

    void activeFileIncluded()
    {
        ContextBuilder builder;
        MockFileSystem fs;
        fs.m_files["/proj/main.cpp"] = "int main() {}";
        builder.setFileSystem(&fs);
        builder.setWorkspaceRoot("/proj");

        auto ctx = builder.buildContext("fix this", "/proj/main.cpp", "", "cpp");
        bool found = false;
        for (const auto &item : ctx.items) {
            if (item.type == ContextItem::Type::ActiveFile) {
                QVERIFY(item.content.contains("int main"));
                found = true;
            }
        }
        QVERIFY(found);
    }

    void activeFile_missingFile_noItem()
    {
        ContextBuilder builder;
        MockFileSystem fs;
        builder.setFileSystem(&fs);

        auto ctx = builder.buildContext("fix", "/proj/missing.cpp", "", "cpp");
        for (const auto &item : ctx.items)
            QVERIFY(item.type != ContextItem::Type::ActiveFile);
    }

    void activeFile_tooLarge_skipped()
    {
        ContextBuilder builder;
        MockFileSystem fs;
        // File > 100KB
        fs.m_files["/proj/big.cpp"] = QString(110 * 1024, 'x');
        builder.setFileSystem(&fs);

        auto ctx = builder.buildContext("fix", "/proj/big.cpp", "", "cpp");
        for (const auto &item : ctx.items)
            QVERIFY(item.type != ContextItem::Type::ActiveFile);
    }

    // ── Selection ─────────────────────────────────────────────────────────

    void selectionIncluded()
    {
        ContextBuilder builder;
        auto ctx = builder.buildContext("explain", "/a.cpp", "int x = 42;", "cpp");

        bool found = false;
        for (const auto &item : ctx.items) {
            if (item.type == ContextItem::Type::Selection) {
                QCOMPARE(item.content, QStringLiteral("int x = 42;"));
                found = true;
            }
        }
        QVERIFY(found);
    }

    void emptySelection_noItem()
    {
        ContextBuilder builder;
        auto ctx = builder.buildContext("explain", "/a.cpp", "", "cpp");
        for (const auto &item : ctx.items)
            QVERIFY(item.type != ContextItem::Type::Selection);
    }

    // ── Open files ────────────────────────────────────────────────────────

    void openFilesFromCallback()
    {
        ContextBuilder builder;
        builder.setOpenFilesGetter([] {
            return QStringList{"/proj/a.cpp", "/proj/b.cpp"};
        });

        auto ctx = builder.buildContext();
        bool found = false;
        for (const auto &item : ctx.items) {
            if (item.type == ContextItem::Type::OpenFiles) {
                QVERIFY(item.content.contains("a.cpp"));
                QVERIFY(item.content.contains("b.cpp"));
                found = true;
            }
        }
        QVERIFY(found);
    }

    // ── Diagnostics ───────────────────────────────────────────────────────

    void diagnosticsFromCallback()
    {
        ContextBuilder builder;
        builder.setDiagnosticsGetter([] {
            AgentDiagnostic d;
            d.filePath = "main.cpp";
            d.line = 10;
            d.column = 1;
            d.message = "undeclared 'x'";
            d.severity = AgentDiagnostic::Severity::Error;
            return QList<AgentDiagnostic>{d};
        });

        auto ctx = builder.buildContext();
        bool found = false;
        for (const auto &item : ctx.items) {
            if (item.type == ContextItem::Type::Diagnostics) {
                QVERIFY(item.content.contains("undeclared"));
                found = true;
            }
        }
        QVERIFY(found);
    }

    // ── Git context ───────────────────────────────────────────────────────

    void gitStatusAndDiff()
    {
        ContextBuilder builder;
        builder.setGitStatusGetter([] { return QStringLiteral("M main.cpp"); });
        builder.setGitDiffGetter([] { return QStringLiteral("+int x = 1;"); });

        auto ctx = builder.buildContext();

        bool foundStatus = false, foundDiff = false;
        for (const auto &item : ctx.items) {
            if (item.type == ContextItem::Type::GitStatus) foundStatus = true;
            if (item.type == ContextItem::Type::GitDiff) foundDiff = true;
        }
        QVERIFY(foundStatus);
        QVERIFY(foundDiff);
    }

    void gitDiff_tooLarge_omitted()
    {
        ContextBuilder builder;
        builder.setGitDiffGetter([] {
            return QString(60 * 1024, 'd');  // > 50KB
        });

        auto ctx = builder.buildContext();
        for (const auto &item : ctx.items)
            QVERIFY(item.type != ContextItem::Type::GitDiff);
    }

    // ── Custom instructions ───────────────────────────────────────────────

    void customInstructionsIncluded()
    {
        ContextBuilder builder;
        builder.setCustomInstructions("Always use snake_case.");

        auto ctx = builder.buildContext();
        bool found = false;
        for (const auto &item : ctx.items) {
            if (item.type == ContextItem::Type::Custom &&
                item.content.contains("snake_case")) {
                found = true;
            }
        }
        QVERIFY(found);
    }

    // ── Type enable/disable ───────────────────────────────────────────────

    void disableContextType()
    {
        ContextBuilder builder;
        MockFileSystem fs;
        fs.m_files["/a.cpp"] = "code";
        builder.setFileSystem(&fs);

        builder.disableContextType(ContextItem::Type::ActiveFile);
        auto ctx = builder.buildContext("fix", "/a.cpp", "", "cpp");

        for (const auto &item : ctx.items)
            QVERIFY(item.type != ContextItem::Type::ActiveFile);
    }

    void enableContextType()
    {
        ContextBuilder builder;
        builder.disableContextType(ContextItem::Type::Selection);
        QVERIFY(!builder.isContextTypeEnabled(ContextItem::Type::Selection));

        builder.enableContextType(ContextItem::Type::Selection);
        QVERIFY(builder.isContextTypeEnabled(ContextItem::Type::Selection));
    }

    // ── Pin/unpin ─────────────────────────────────────────────────────────

    void pinContextType()
    {
        ContextBuilder builder;
        builder.pinContextType(ContextItem::Type::Selection);
        QVERIFY(builder.isContextTypePinned(ContextItem::Type::Selection));

        builder.unpinContextType(ContextItem::Type::Selection);
        QVERIFY(!builder.isContextTypePinned(ContextItem::Type::Selection));
    }

    void pinnedItemsSurvivePruning()
    {
        ContextBuilder builder;
        builder.setMaxContextChars(100);  // very tight budget

        builder.setCustomInstructions(QString(200, 'A'));
        builder.pinContextType(ContextItem::Type::Custom);

        builder.setOpenFilesGetter([] {
            return QStringList{"/a", "/b", "/c"};
        });

        auto ctx = builder.buildContext();

        // Custom (pinned) should survive; OpenFiles (unpinned, low priority) may be pruned
        bool customFound = false;
        for (const auto &item : ctx.items) {
            if (item.type == ContextItem::Type::Custom)
                customFound = true;
        }
        QVERIFY(customFound);
    }

    // ── Pruning ───────────────────────────────────────────────────────────

    void pruneContext_removesLowPriority()
    {
        ContextSnapshot ctx;

        ContextItem low;
        low.type = ContextItem::Type::WorkspaceInfo;
        low.label = "workspace";
        low.content = QString(500, 'w');
        low.priority = 1;
        ctx.items.append(low);

        ContextItem high;
        high.type = ContextItem::Type::ActiveFile;
        high.label = "file";
        high.content = QString(500, 'f');
        high.priority = 10;
        ctx.items.append(high);

        // Budget fits only one item (~500 chars ≈ 125 tokens → budget 500 chars)
        ContextBuilder::pruneContext(ctx, 600);

        // Low-priority workspace item should be removed, high-priority kept
        QCOMPARE(ctx.items.size(), 1);
        QCOMPARE(ctx.items[0].type, ContextItem::Type::ActiveFile);
    }

    void pruneContext_pinnedNeverRemoved()
    {
        ContextSnapshot ctx;

        ContextItem pinned;
        pinned.type = ContextItem::Type::Custom;
        pinned.label = "instructions";
        pinned.content = QString(1000, 'i');
        pinned.priority = 1;  // very low priority
        pinned.pinned = true;
        ctx.items.append(pinned);

        ContextItem unpinned;
        unpinned.type = ContextItem::Type::ActiveFile;
        unpinned.label = "file";
        unpinned.content = QString(500, 'f');
        unpinned.priority = 10;  // high priority
        ctx.items.append(unpinned);

        // Budget fits about 1 item, but pinned item can't be removed
        ContextBuilder::pruneContext(ctx, 600);

        bool pinnedFound = false;
        for (const auto &item : ctx.items) {
            if (item.type == ContextItem::Type::Custom && item.pinned)
                pinnedFound = true;
        }
        QVERIFY(pinnedFound);
    }

    void pruneContext_noOpWhenUnderBudget()
    {
        ContextSnapshot ctx;

        ContextItem item;
        item.type = ContextItem::Type::ActiveFile;
        item.content = "small";
        item.priority = 5;
        ctx.items.append(item);

        ContextBuilder::pruneContext(ctx, 100000);  // huge budget
        QCOMPARE(ctx.items.size(), 1);
    }

    // ── Token budget ──────────────────────────────────────────────────────

    void setMaxTokenBudget()
    {
        ContextBuilder builder;
        builder.setMaxTokenBudget(8000); // 8000 * 4 * 0.8 = 25600
        QCOMPARE(builder.maxContextChars(), 25600);
    }

    void estimatedTokens()
    {
        ContextSnapshot ctx;
        ContextItem item;
        item.content = QString(400, 'x');
        ctx.items.append(item);
        // 400 chars / 4 = 100 tokens
        QCOMPARE(ctx.estimatedTokens(), 100);
    }
};

QTEST_MAIN(TestContextBuilder)
#include "test_contextbuilder.moc"

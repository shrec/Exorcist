#include <QtTest>
#include <QTextDocument>
#include <QTextBlock>

#include "editor/highlighterfactory.h"
#include "editor/treesitterhighlighter.h"
#include "editor/syntaxhighlighter.h"

class TestHighlighterFactory : public QObject
{
    Q_OBJECT

private slots:
    // ── hasTreeSitter ─────────────────────────────────────────────────────

    void hasTreeSitter_compiled()
    {
#ifdef EXORCIST_HAS_TREESITTER
        QVERIFY(HighlighterFactory::hasTreeSitter());
#else
        QVERIFY(!HighlighterFactory::hasTreeSitter());
#endif
    }

    // ── Tree-sitter preferred for supported extensions ────────────────────

#ifdef EXORCIST_HAS_TREESITTER
    void create_c_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int x;"));
        auto *hl = HighlighterFactory::create(QStringLiteral("test.c"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }

    void create_cpp_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("class Foo {};"));
        auto *hl = HighlighterFactory::create(QStringLiteral("test.cpp"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }

    void create_h_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("#pragma once"));
        auto *hl = HighlighterFactory::create(QStringLiteral("test.h"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }

    void create_python_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("def f(): pass"));
        auto *hl = HighlighterFactory::create(QStringLiteral("test.py"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }

    void create_js_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("const x = 1;"));
        auto *hl = HighlighterFactory::create(QStringLiteral("app.js"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }

    void create_ts_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("let x: number = 1;"));
        auto *hl = HighlighterFactory::create(QStringLiteral("app.ts"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }

    void create_rust_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("fn main() {}"));
        auto *hl = HighlighterFactory::create(QStringLiteral("main.rs"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }

    void create_json_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("{\"key\": 1}"));
        auto *hl = HighlighterFactory::create(QStringLiteral("data.json"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }

    void create_jsx_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("const Comp = () => <div/>;"));
        auto *hl = HighlighterFactory::create(QStringLiteral("App.jsx"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }

    void create_tsx_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("const Comp: FC = () => <div/>;"));
        auto *hl = HighlighterFactory::create(QStringLiteral("App.tsx"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }
#endif

    // ── Regex fallback for unsupported tree-sitter extensions ──────────────

    void create_go_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("package main"));
        auto *hl = HighlighterFactory::create(QStringLiteral("main.go"), doc.get());
        QVERIFY(hl);
        // Go now has tree-sitter grammar
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }

    void create_java_returnsRegex()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("class Main {}"));
        auto *hl = HighlighterFactory::create(QStringLiteral("Main.java"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<SyntaxHighlighter *>(hl));
    }

    void create_yaml_returnsTreeSitter()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("key: value"));
        auto *hl = HighlighterFactory::create(QStringLiteral("config.yaml"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }

    void create_cmake_returnsRegex()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("cmake_minimum_required(VERSION 3.21)"));
        auto *hl = HighlighterFactory::create(QStringLiteral("CMakeLists.txt"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<SyntaxHighlighter *>(hl));
    }

    // ── Null for plain text ───────────────────────────────────────────────

    void create_txt_returnsNull()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("hello"));
        auto *hl = HighlighterFactory::create(QStringLiteral("readme.txt"), doc.get());
        QVERIFY(!hl);
    }

    void create_unknown_returnsNull()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("data"));
        auto *hl = HighlighterFactory::create(QStringLiteral("file.zzz"), doc.get());
        QVERIFY(!hl);
    }

    // ── Edge cases ────────────────────────────────────────────────────────

    void create_emptyPath_returnsNull()
    {
        auto doc = std::make_unique<QTextDocument>();
        auto *hl = HighlighterFactory::create(QString(), doc.get());
        QVERIFY(!hl);
    }

    void create_nullDoc_returnsNull()
    {
        auto *hl = HighlighterFactory::create(QStringLiteral("test.cpp"), nullptr);
        QVERIFY(!hl);
    }

    void create_caseInsensitive()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int x;"));
        auto *hl = HighlighterFactory::create(QStringLiteral("test.CPP"), doc.get());
        // Should still work — HighlighterFactory lowercases extension
        QVERIFY(hl);
    }

    // ── Tree-sitter highlighter applies formats ───────────────────────────

#ifdef EXORCIST_HAS_TREESITTER
    void treeSitter_appliesFormats()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int main() { return 0; }"));
        auto *hl = HighlighterFactory::create(QStringLiteral("test.c"), doc.get());
        QVERIFY(hl);

        QTextBlock block = doc->firstBlock();
        const auto fmts = block.layout()->formats();
        QVERIFY(!fmts.isEmpty());
    }
#endif

    // ── Language lookup callback (plugin-driven) ────────────────────────

#ifdef EXORCIST_HAS_TREESITTER
    void languageLookup_overridesHardCoded()
    {
        // Set a lookup that maps "c" extension to "cpp" language ID
        HighlighterFactory::setLanguageLookup([](const QString &ext) -> QString {
            if (ext == QLatin1String("c"))
                return QStringLiteral("cpp");  // remap C → C++ grammar
            return {};
        });

        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int x;"));
        auto *hl = HighlighterFactory::create(QStringLiteral("test.c"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));

        // Reset
        HighlighterFactory::setLanguageLookup({});
    }

    void languageLookup_fallsBackWhenEmpty()
    {
        // Set a lookup that never claims any extension
        HighlighterFactory::setLanguageLookup([](const QString &) -> QString {
            return {};
        });

        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int x;"));
        auto *hl = HighlighterFactory::create(QStringLiteral("test.cpp"), doc.get());
        QVERIFY(hl);
        // Should still get tree-sitter via hard-coded fallback
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));

        // Reset
        HighlighterFactory::setLanguageLookup({});
    }

    void languageLookup_unknownIdFallsBack()
    {
        // Lookup returns an ID for which there is no tree-sitter grammar
        HighlighterFactory::setLanguageLookup([](const QString &ext) -> QString {
            if (ext == QLatin1String("cpp"))
                return QStringLiteral("unknown_lang");
            return {};
        });

        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int x;"));
        auto *hl = HighlighterFactory::create(QStringLiteral("test.cpp"), doc.get());
        QVERIFY(hl);
        // tsLanguageForId("unknown_lang") returns nullptr, so falls back to
        // hard-coded extension map which gives tree_sitter_cpp()
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));

        // Reset
        HighlighterFactory::setLanguageLookup({});
    }

    void languageLookup_noCallbackUsesHardCoded()
    {
        // Ensure no callback is set
        HighlighterFactory::setLanguageLookup({});

        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("fn main() {}"));
        auto *hl = HighlighterFactory::create(QStringLiteral("main.rs"), doc.get());
        QVERIFY(hl);
        QVERIFY(qobject_cast<TreeSitterHighlighter *>(hl));
    }
#endif

    // ── Regex highlighter applies formats ─────────────────────────────────

    void regex_appliesFormats()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("package main\nfunc main() {}"));
        auto *hl = HighlighterFactory::create(QStringLiteral("main.go"), doc.get());
        QVERIFY(hl);
        QCoreApplication::processEvents();

        QTextBlock block = doc->firstBlock();
        const auto fmts = block.layout()->formats();
        QVERIFY(!fmts.isEmpty());
    }
};

QTEST_MAIN(TestHighlighterFactory)
#include "test_highlighterfactory.moc"

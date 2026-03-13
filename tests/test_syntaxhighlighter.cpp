#include <QtTest>
#include <QTextDocument>
#include <QTextBlock>

#include "editor/syntaxhighlighter.h"

class TestSyntaxHighlighter : public QObject
{
    Q_OBJECT

private slots:
    // ── Factory: extension-based creation ─────────────────────────────────

    void create_cpp()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int main() { return 0; }"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.cpp"), doc.get());
        QVERIFY(hl);
    }

    void create_python()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("def hello(): pass"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.py"), doc.get());
        QVERIFY(hl);
    }

    void create_javascript()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("function f() {}"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("app.js"), doc.get());
        QVERIFY(hl);
    }

    void create_json()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("{\"key\": 1}"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("data.json"), doc.get());
        QVERIFY(hl);
    }

    void create_rust()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("fn main() {}"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("main.rs"), doc.get());
        QVERIFY(hl);
    }

    void create_csharp()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("class Foo {}"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("Program.cs"), doc.get());
        QVERIFY(hl);
    }

    void create_go()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("package main"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("main.go"), doc.get());
        QVERIFY(hl);
    }

    void create_yaml()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("key: value"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("config.yaml"), doc.get());
        QVERIFY(hl);
    }

    void create_cmake()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("cmake_minimum_required(VERSION 3.21)"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("CMakeLists.txt"), doc.get());
        QVERIFY(hl);
    }

    void create_shell()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("echo hello"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("script.sh"), doc.get());
        QVERIFY(hl);
    }

    void create_sql()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("SELECT * FROM t;"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("query.sql"), doc.get());
        QVERIFY(hl);
    }

    void create_xml()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("<root/>"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("file.xml"), doc.get());
        QVERIFY(hl);
    }

    // ── Factory: returns null for unknown ─────────────────────────────────

    void create_plainText_returnsNull()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("hello"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("readme.txt"), doc.get());
        QVERIFY(!hl);
    }

    void create_unknownExtension_returnsNull()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("data"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("file.zzz"), doc.get());
        QVERIFY(!hl);
    }

    // ── Factory: filename-based detection ─────────────────────────────────

    void create_makefile()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("all:\n\techo done"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("Makefile"), doc.get());
        QVERIFY(hl);
    }

    void create_dockerfile()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("FROM ubuntu:22.04"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("Dockerfile"), doc.get());
        QVERIFY(hl);
    }

    void create_gemfile()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("source 'https://rubygems.org'"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("Gemfile"), doc.get());
        QVERIFY(hl);
    }

    void create_bashrc()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("export PATH=/usr/bin"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral(".bashrc"), doc.get());
        QVERIFY(hl);
    }

    void create_gitconfig()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("[user]\n  name = Test"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral(".gitconfig"), doc.get());
        QVERIFY(hl);
    }

    // ── Factory: shebang detection ────────────────────────────────────────

    void create_shebang_python()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("#!/usr/bin/env python3\nprint('hi')"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("myscript"), doc.get());
        QVERIFY(hl);
    }

    void create_shebang_bash()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("#!/bin/bash\necho hello"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("myscript"), doc.get());
        QVERIFY(hl);
    }

    void create_shebang_node()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("#!/usr/bin/env node\nconsole.log(1)"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("myscript"), doc.get());
        QVERIFY(hl);
    }

    // ── Highlighting: formats applied ─────────────────────────────────────

    void highlight_cpp_keyword_applies_format()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("class Foo { int x; };"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.cpp"), doc.get());
        QVERIFY(hl);
        QCoreApplication::processEvents();

        QTextBlock block = doc->firstBlock();
        const auto fmts = block.layout()->formats();
        QVERIFY(!fmts.isEmpty());
    }

    void highlight_python_keyword_applies_format()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("def greet(name):\n    return 'hello ' + name"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.py"), doc.get());
        QVERIFY(hl);
        QCoreApplication::processEvents();

        QTextBlock block = doc->firstBlock();
        const auto fmts = block.layout()->formats();
        QVERIFY(!fmts.isEmpty());
    }

    void highlight_js_keyword_applies_format()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("const x = 'hello';"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.js"), doc.get());
        QVERIFY(hl);
        QCoreApplication::processEvents();

        QTextBlock block = doc->firstBlock();
        const auto fmts = block.layout()->formats();
        QVERIFY(!fmts.isEmpty());
    }

    void highlight_rust_keyword_applies_format()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("fn main() { let x = 42; }"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("main.rs"), doc.get());
        QVERIFY(hl);
        QCoreApplication::processEvents();

        QTextBlock block = doc->firstBlock();
        const auto fmts = block.layout()->formats();
        QVERIFY(!fmts.isEmpty());
    }

    // ── Block comments: multiline state ───────────────────────────────────

    void blockComment_cpp_spans_lines()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("/* this is\na block comment */\nint x;"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.cpp"), doc.get());
        QVERIFY(hl);
        QCoreApplication::processEvents();

        QTextBlock b1 = doc->firstBlock();
        // First line opens block comment → state should be 1 (inside)
        QCOMPARE(b1.userState(), 1);

        // Second line closes block comment → state should be 0
        QTextBlock b2 = b1.next();
        QCOMPARE(b2.userState(), 0);

        // Third line is normal code → state should be 0
        QTextBlock b3 = b2.next();
        QCOMPARE(b3.userState(), 0);
    }

    void blockComment_python_spans_lines()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("\"\"\"multi\nline\ndoc\"\"\"\nx = 1"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.py"), doc.get());
        QVERIFY(hl);
        QCoreApplication::processEvents();

        QTextBlock b1 = doc->firstBlock();
        // Opens triple-quote → in-block state
        QCOMPARE(b1.userState(), 1);
    }

    void blockComment_noBlockLang_stateUnset()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("{\"key\": \"value\"}\n{\"other\": 2}"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.json"), doc.get());
        QVERIFY(hl);

        // JSON has no block comments — state should be default (-1)
        QTextBlock b = doc->firstBlock();
        QCOMPARE(b.userState(), -1);
    }

    // ── Edge cases ────────────────────────────────────────────────────────

    void highlight_empty_document()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QString());
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.cpp"), doc.get());
        QVERIFY(hl);

        QTextBlock b = doc->firstBlock();
        QVERIFY(b.isValid());
    }

    void highlight_single_char()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral(";"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.cpp"), doc.get());
        QVERIFY(hl);

        QTextBlock b = doc->firstBlock();
        QVERIFY(b.isValid());
    }

    void highlight_multiline_code()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral(
            "#include <stdio.h>\n"
            "int main() {\n"
            "    printf(\"hello\\n\");\n"
            "    return 0;\n"
            "}\n"
        ));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.c"), doc.get());
        QVERIFY(hl);
        QCoreApplication::processEvents();

        // Multiple blocks should have formats
        QTextBlock b = doc->firstBlock();
        QVERIFY(!b.layout()->formats().isEmpty()); // #include
        b = b.next();
        QVERIFY(!b.layout()->formats().isEmpty()); // int main()
    }

    // ── Case insensitive extension matching ───────────────────────────────

    void create_uppercase_extension()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("class Foo {};"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.CPP"), doc.get());
        QVERIFY(hl);
    }

    void create_mixed_case_extension()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("def hello(): pass"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("test.Py"), doc.get());
        QVERIFY(hl);
    }

    // ── Additional language coverage ──────────────────────────────────────

    void create_toml()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("[package]\nname = \"foo\""));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("Cargo.toml"), doc.get());
        QVERIFY(hl);
    }

    void create_lua()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("local x = 42"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("init.lua"), doc.get());
        QVERIFY(hl);
    }

    void create_powershell()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("Get-ChildItem"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("script.ps1"), doc.get());
        QVERIFY(hl);
    }

    void create_kotlin()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("fun main() {}"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("Main.kt"), doc.get());
        QVERIFY(hl);
    }

    void create_swift()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("import Foundation"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("App.swift"), doc.get());
        QVERIFY(hl);
    }

    void create_css()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("body { color: red; }"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("style.css"), doc.get());
        QVERIFY(hl);
    }

    void create_markdown()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("# Hello\n\nWorld"));
        auto *hl = SyntaxHighlighter::create(QStringLiteral("README.md"), doc.get());
        QVERIFY(hl);
    }
};

QTEST_MAIN(TestSyntaxHighlighter)
#include "test_syntaxhighlighter.moc"

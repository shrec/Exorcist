#include <QTest>
#include <QTextDocument>
#include <QTextBlock>
#include <QByteArray>

#include "editor/treesitterhighlighter.h"

#ifdef EXORCIST_HAS_TREESITTER
#include <tree_sitter/api.h>
extern "C" {
const TSLanguage *tree_sitter_c();
const TSLanguage *tree_sitter_cpp();
const TSLanguage *tree_sitter_python();
const TSLanguage *tree_sitter_javascript();
const TSLanguage *tree_sitter_json();
}
#endif

// ── Tests for TreeSitterHighlighter ──────────────────────────────────────────
//
// The byte↔char conversion helpers are compiled unconditionally (no tree-sitter
// needed). We access them via the friend declaration in the header.
// The full highlighting path requires EXORCIST_HAS_TREESITTER.

class TestTreeSitterHighlighter : public QObject
{
    Q_OBJECT

private:
    static uint32_t charToByte(const QByteArray &utf8, int charOffset)
    {
        return TreeSitterHighlighter::charToByteOffset(utf8, charOffset);
    }

    static int byteToChar(const QByteArray &utf8, uint32_t byteOffset)
    {
        return TreeSitterHighlighter::byteToCharOffset(utf8, byteOffset);
    }

private slots:

    // ── charToByteOffset — ASCII ──────────────────────────────────────────

    void charToByte_ascii_zero()
    {
        QByteArray utf8 = "Hello";
        QCOMPARE(charToByte(utf8, 0), (uint32_t)0);
    }

    void charToByte_ascii_middle()
    {
        QByteArray utf8 = "Hello";
        QCOMPARE(charToByte(utf8, 3), (uint32_t)3);
    }

    void charToByte_ascii_end()
    {
        QByteArray utf8 = "Hello";
        QCOMPARE(charToByte(utf8, 5), (uint32_t)5);
    }

    void charToByte_ascii_beyond()
    {
        QByteArray utf8 = "Hi";
        QCOMPARE(charToByte(utf8, 10), (uint32_t)2); // clamped to size
    }

    // ── charToByteOffset — 2-byte UTF-8 ──────────────────────────────────

    void charToByte_twoByte()
    {
        // "café" → 'c'=1, 'a'=1, 'f'=1, 'é'=2 bytes → total 5 bytes
        QByteArray utf8 = QString::fromUtf8("café").toUtf8();
        QCOMPARE(utf8.size(), 5);
        QCOMPARE(charToByte(utf8, 0), (uint32_t)0); // 'c'
        QCOMPARE(charToByte(utf8, 1), (uint32_t)1); // 'a'
        QCOMPARE(charToByte(utf8, 2), (uint32_t)2); // 'f'
        QCOMPARE(charToByte(utf8, 3), (uint32_t)3); // 'é' starts at byte 3
        QCOMPARE(charToByte(utf8, 4), (uint32_t)5); // after 'é' → byte 5
    }

    // ── charToByteOffset — 3-byte UTF-8 ──────────────────────────────────

    void charToByte_threeByte()
    {
        // "A中B" → 'A'=1, '中'=3, 'B'=1 bytes → total 5 bytes
        QByteArray utf8 = QString::fromUtf8("A中B").toUtf8();
        QCOMPARE(utf8.size(), 5);
        QCOMPARE(charToByte(utf8, 0), (uint32_t)0); // 'A'
        QCOMPARE(charToByte(utf8, 1), (uint32_t)1); // '中' starts at byte 1
        QCOMPARE(charToByte(utf8, 2), (uint32_t)4); // 'B' at byte 4
        QCOMPARE(charToByte(utf8, 3), (uint32_t)5); // end
    }

    // ── charToByteOffset — 4-byte UTF-8 (surrogate pair) ─────────────────

    void charToByte_fourByte()
    {
        // U+1F600 (😀) → 4 bytes UTF-8, 2 code units in UTF-16
        QByteArray utf8 = QString::fromUtf8("A😀B").toUtf8();
        QCOMPARE(utf8.size(), 6); // 1 + 4 + 1
        QCOMPARE(charToByte(utf8, 0), (uint32_t)0); // 'A'
        QCOMPARE(charToByte(utf8, 1), (uint32_t)1); // start of 😀
        // 😀 = 2 Qt chars (surrogate pair), so char offset 3 = after 😀
        QCOMPARE(charToByte(utf8, 3), (uint32_t)5); // 'B'
        QCOMPARE(charToByte(utf8, 4), (uint32_t)6); // end
    }

    // ── charToByteOffset — empty ──────────────────────────────────────────

    void charToByte_empty()
    {
        QByteArray utf8;
        QCOMPARE(charToByte(utf8, 0), (uint32_t)0);
        QCOMPARE(charToByte(utf8, 5), (uint32_t)0);
    }

    // ── byteToCharOffset — ASCII ──────────────────────────────────────────

    void byteToChar_ascii_zero()
    {
        QByteArray utf8 = "Hello";
        QCOMPARE(byteToChar(utf8, 0), 0);
    }

    void byteToChar_ascii_middle()
    {
        QByteArray utf8 = "Hello";
        QCOMPARE(byteToChar(utf8, 3), 3);
    }

    void byteToChar_ascii_end()
    {
        QByteArray utf8 = "Hello";
        QCOMPARE(byteToChar(utf8, 5), 5);
    }

    // ── byteToCharOffset — 2-byte ────────────────────────────────────────

    void byteToChar_twoByte()
    {
        QByteArray utf8 = QString::fromUtf8("café").toUtf8();
        QCOMPARE(byteToChar(utf8, 0), 0); // 'c'
        QCOMPARE(byteToChar(utf8, 3), 3); // start of 'é'
        QCOMPARE(byteToChar(utf8, 5), 4); // end
    }

    // ── byteToCharOffset — 3-byte ────────────────────────────────────────

    void byteToChar_threeByte()
    {
        QByteArray utf8 = QString::fromUtf8("A中B").toUtf8();
        QCOMPARE(byteToChar(utf8, 0), 0); // 'A'
        QCOMPARE(byteToChar(utf8, 1), 1); // '中' start
        QCOMPARE(byteToChar(utf8, 4), 2); // 'B'
        QCOMPARE(byteToChar(utf8, 5), 3); // end
    }

    // ── byteToCharOffset — 4-byte (surrogate pair) ───────────────────────

    void byteToChar_fourByte()
    {
        QByteArray utf8 = QString::fromUtf8("A😀B").toUtf8();
        QCOMPARE(byteToChar(utf8, 0), 0); // 'A'
        QCOMPARE(byteToChar(utf8, 1), 1); // 😀 start
        QCOMPARE(byteToChar(utf8, 5), 3); // 'B' (after 2 Qt chars for 😀)
        QCOMPARE(byteToChar(utf8, 6), 4); // end
    }

    // ── byteToCharOffset — empty ──────────────────────────────────────────

    void byteToChar_empty()
    {
        QByteArray utf8;
        QCOMPARE(byteToChar(utf8, 0), 0);
    }

    // ── Roundtrip consistency ─────────────────────────────────────────────

    void roundtrip_ascii()
    {
        QByteArray utf8 = "Hello World";
        for (int i = 0; i <= utf8.size(); ++i) {
            int ch = byteToChar(utf8, i);
            QCOMPARE(charToByte(utf8, ch), (uint32_t)i);
        }
    }

    void roundtrip_mixed()
    {
        // Mix of 1, 2, 3 byte characters
        QByteArray utf8 = QString::fromUtf8("Héllo中World").toUtf8();
        // Forward: char → byte → char should be identity
        int totalChars = QString::fromUtf8(utf8).length();
        for (int c = 0; c <= totalChars; ++c) {
            uint32_t b = charToByte(utf8, c);
            int c2 = byteToChar(utf8, b);
            QCOMPARE(c2, c);
        }
    }

    // ── Constructor / stub behavior ───────────────────────────────────────

    void constructWithDocument()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText("int main() { return 0; }");
        TreeSitterHighlighter hl(doc.get());
        // Without tree-sitter compiled in, hasLanguage() returns false
        // With tree-sitter, no language set yet
        QVERIFY(!hl.hasLanguage());
    }

    void setLanguage_null()
    {
        auto doc = std::make_unique<QTextDocument>();
        TreeSitterHighlighter hl(doc.get());
        QVERIFY(!hl.setLanguage(nullptr));
    }

    // ── Multi-line byte offset consistency ────────────────────────────────

    void multiline_offsets()
    {
        QByteArray utf8 = "line1\nline2\nline3";
        // newline at byte 5 and 11
        QCOMPARE(byteToChar(utf8, 5), 5);   // '\n'
        QCOMPARE(byteToChar(utf8, 6), 6);   // 'l' of line2
        QCOMPARE(byteToChar(utf8, 11), 11); // '\n'
        QCOMPARE(byteToChar(utf8, 12), 12); // 'l' of line3
    }

    void multiline_utf8_offsets()
    {
        // "café\nnaïve"
        QByteArray utf8 = QString::fromUtf8("café\nnaïve").toUtf8();
        // café = 5 bytes (c=1,a=1,f=1,é=2), \n = 1, naïve = 6 bytes (n=1,a=1,ï=2,v=1,e=1) → total 12
        QCOMPARE(utf8.size(), 12);
        QCOMPARE(byteToChar(utf8, 5), 4);   // '\n' (char 4)
        QCOMPARE(byteToChar(utf8, 6), 5);   // 'n'
        QCOMPARE(byteToChar(utf8, 8), 7);   // 'ï' start
        QCOMPARE(byteToChar(utf8, 12), 10); // end (10 chars total)
    }

    // ── Boundary — large offset beyond string ─────────────────────────────

    void byteToChar_beyondEnd()
    {
        QByteArray utf8 = "AB";
        // byteToCharOffset clamps to utf8.size()
        QCOMPARE(byteToChar(utf8, 100), 2);
    }

    void charToByte_beyondEnd()
    {
        QByteArray utf8 = "AB";
        QCOMPARE(charToByte(utf8, 100), (uint32_t)2);
    }

    // ── Integration: setLanguage with real grammar ────────────────────────

#ifdef EXORCIST_HAS_TREESITTER
    void setLanguage_c_valid()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int x = 42;"));
        TreeSitterHighlighter hl(doc.get());
        QVERIFY(hl.setLanguage(tree_sitter_c()));
        QVERIFY(hl.hasLanguage());
    }

    void setLanguage_cpp_valid()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("class Foo {};"));
        TreeSitterHighlighter hl(doc.get());
        QVERIFY(hl.setLanguage(tree_sitter_cpp()));
        QVERIFY(hl.hasLanguage());
    }

    void setLanguage_python_valid()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("def hello():\n    pass"));
        TreeSitterHighlighter hl(doc.get());
        QVERIFY(hl.setLanguage(tree_sitter_python()));
        QVERIFY(hl.hasLanguage());
    }

    void setLanguage_javascript_valid()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("function f() { return 1; }"));
        TreeSitterHighlighter hl(doc.get());
        QVERIFY(hl.setLanguage(tree_sitter_javascript()));
        QVERIFY(hl.hasLanguage());
    }

    void setLanguage_json_valid()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("{\"key\": 42}"));
        TreeSitterHighlighter hl(doc.get());
        QVERIFY(hl.setLanguage(tree_sitter_json()));
        QVERIFY(hl.hasLanguage());
    }

    // ── Integration: highlight applies formats ────────────────────────────

    void highlight_c_keyword_has_format()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int main() { return 0; }"));
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_c());

        // "int" at position 0 should have a format (keyword)
        QTextBlock block = doc->firstBlock();
        auto layouts = block.layout();
        QVERIFY(layouts);

        // Check that formats were actually applied (non-empty format ranges)
        const auto formats = layouts->formats();
        QVERIFY(!formats.isEmpty());
    }

    void highlight_c_comment_has_format()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("// this is a comment\nint x;"));
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_c());

        QTextBlock block = doc->firstBlock();
        const auto formats = block.layout()->formats();
        QVERIFY(!formats.isEmpty());
    }

    void highlight_c_string_has_format()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("char *s = \"hello world\";"));
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_c());

        QTextBlock block = doc->firstBlock();
        const auto formats = block.layout()->formats();
        QVERIFY(!formats.isEmpty());
    }

    void highlight_multiline_formats_both_blocks()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int x;\nfloat y;"));
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_c());

        // Both blocks should have formats
        QTextBlock b1 = doc->firstBlock();
        QTextBlock b2 = b1.next();
        QVERIFY(b1.isValid());
        QVERIFY(b2.isValid());
        QVERIFY(!b1.layout()->formats().isEmpty());
        QVERIFY(!b2.layout()->formats().isEmpty());
    }

    void highlight_python_def_has_format()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("def greet(name):\n    print(name)"));
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_python());

        QTextBlock block = doc->firstBlock();
        const auto formats = block.layout()->formats();
        QVERIFY(!formats.isEmpty());
    }

    void highlight_js_function_has_format()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("function add(a, b) { return a + b; }"));
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_javascript());

        QTextBlock block = doc->firstBlock();
        const auto formats = block.layout()->formats();
        QVERIFY(!formats.isEmpty());
    }

    // ── Integration: reparse after edit ───────────────────────────────────

    void reparse_after_edit()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int x = 1;"));
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_c());

        QTextBlock b = doc->firstBlock();
        QVERIFY(!b.layout()->formats().isEmpty());

        // Edit document — should trigger incremental reparse
        QTextCursor cursor(doc.get());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(QStringLiteral("\nfloat y = 2.0f;"));

        // Second block should also get formatted
        QTextBlock b2 = doc->firstBlock().next();
        QVERIFY(b2.isValid());
        // After rehighlight, layout formats may be populated
        hl.reparse();
        QVERIFY(hl.hasLanguage());
    }

    void reparse_explicit()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("int a;"));
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_c());

        // Change text completely
        doc->setPlainText(QStringLiteral("double b;"));
        hl.reparse();

        QTextBlock b = doc->firstBlock();
        QVERIFY(!b.layout()->formats().isEmpty());
    }

    // ── Integration: empty document ───────────────────────────────────────

    void highlight_empty_document()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QString());
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_c());

        QTextBlock block = doc->firstBlock();
        // Empty document — should not crash, formats may be empty
        QVERIFY(block.isValid());
    }

    // ── Integration: JSON highlighting ────────────────────────────────────

    void highlight_json_object()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("{\"name\": \"Exorcist\", \"version\": 1}"));
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_json());

        QTextBlock block = doc->firstBlock();
        const auto formats = block.layout()->formats();
        QVERIFY(!formats.isEmpty());
    }

    // ── Integration: UTF-8 in source code ─────────────────────────────────

    void highlight_utf8_in_string()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral("char *s = \"café\";"));
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_c());

        QTextBlock b = doc->firstBlock();
        QVERIFY(!b.layout()->formats().isEmpty());
    }

    // ── Integration: large multiline code ─────────────────────────────────

    void highlight_larger_code()
    {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText(QStringLiteral(
            "#include <stdio.h>\n"
            "\n"
            "int main() {\n"
            "    // Hello\n"
            "    printf(\"Hello, world!\\n\");\n"
            "    return 0;\n"
            "}\n"
        ));
        TreeSitterHighlighter hl(doc.get());
        hl.setLanguage(tree_sitter_c());

        // Should not crash and should have formats on code lines
        QTextBlock b = doc->firstBlock(); // #include
        QVERIFY(!b.layout()->formats().isEmpty());

        b = b.next().next(); // int main()
        QVERIFY(!b.layout()->formats().isEmpty());
    }
#endif
};

QTEST_MAIN(TestTreeSitterHighlighter)
#include "test_treesitter_highlighter.moc"

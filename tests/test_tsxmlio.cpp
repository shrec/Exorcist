// test_tsxmlio.cpp — round-trip + counting tests for TsXmlIO.
//
// Covers:
//   - load(path) parses contexts / messages / locations / status types
//   - save(path) emits .ts XML that round-trips through load()
//   - finished/unfinished/obsolete/total counts on TsFile
//
// We exercise minimal hand-written .ts XML buffers via QTemporaryDir so the
// test is hermetic and does not depend on Qt Linguist.
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include "forms/linguist/tsxmlio.h"

using exo::forms::linguist::TsContext;
using exo::forms::linguist::TsFile;
using exo::forms::linguist::TsMessage;
using exo::forms::linguist::TsXmlIO;

namespace {

// Minimal: 1 context, 1 unfinished message with one location.
constexpr const char *kMinimalTs = R"(<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ka_GE" sourcelanguage="en">
 <context>
  <name>MainWindow</name>
  <message>
   <location filename="../mainwindow.cpp" line="42"/>
   <source>Hello</source>
   <translation type="unfinished">გამარჯობა</translation>
  </message>
 </context>
</TS>
)";

// 3 contexts × 5 messages each, mixed statuses and multiple locations.
QString buildMultiContextTs()
{
    QString ts = QStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                                "<!DOCTYPE TS>\n"
                                "<TS version=\"2.1\" language=\"de\" sourcelanguage=\"en\">\n");
    const QStringList ctxs = {QStringLiteral("FileMenu"),
                              QStringLiteral("EditMenu"),
                              QStringLiteral("ViewMenu")};
    for (const QString &name : ctxs) {
        ts += QStringLiteral(" <context>\n  <name>%1</name>\n").arg(name);
        for (int i = 0; i < 5; ++i) {
            QString status;
            // Pattern: 0=finished, 1=unfinished, 2=obsolete, 3=finished, 4=unfinished
            if (i == 1 || i == 4) status = QStringLiteral(" type=\"unfinished\"");
            else if (i == 2)      status = QStringLiteral(" type=\"obsolete\"");
            ts += QStringLiteral(
                "  <message>\n"
                "   <location filename=\"../%1.cpp\" line=\"%2\"/>\n"
                "   <location filename=\"../%1.h\" line=\"%3\"/>\n"
                "   <source>%4_%5</source>\n"
                "   <translation%6>tx_%4_%5</translation>\n"
                "  </message>\n").arg(name).arg(10 + i).arg(20 + i)
                                  .arg(name).arg(i).arg(status);
        }
        ts += QStringLiteral(" </context>\n");
    }
    ts += QStringLiteral("</TS>\n");
    return ts;
}

QString writeTempFile(QTemporaryDir &dir, const QString &name, const QString &content)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString();
    f.write(content.toUtf8());
    f.close();
    return path;
}

} // namespace

class TestTsXmlIO : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void parseMinimalTsFile();
    void parseMultiContextFile();
    void preservesStatusTypes();
    void preservesLocations();
    void saveOutputIsParseableByLoad();
    void countersAreComputedCorrectly();
    void loadFailsOnMissingRoot();
    void loadFailsOnMissingFile();

private:
    QTemporaryDir m_dir;
};

void TestTsXmlIO::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

void TestTsXmlIO::parseMinimalTsFile()
{
    const QString path = writeTempFile(m_dir, QStringLiteral("minimal.ts"),
                                       QString::fromUtf8(kMinimalTs));
    TsFile out;
    QString err;
    QVERIFY2(TsXmlIO::load(path, out, &err), qPrintable(err));
    QCOMPARE(out.contexts.size(), 1);
    QCOMPARE(out.contexts[0].name, QStringLiteral("MainWindow"));
    QCOMPARE(out.contexts[0].messages.size(), 1);
    QCOMPARE(out.language, QStringLiteral("ka_GE"));
    QCOMPARE(out.sourceLanguage, QStringLiteral("en"));

    const TsMessage &m = out.contexts[0].messages[0];
    QCOMPARE(m.source, QStringLiteral("Hello"));
    QCOMPARE(m.status, QStringLiteral("unfinished"));
    QCOMPARE(m.locations.size(), 1);
    QCOMPARE(m.locations[0].first, QStringLiteral("../mainwindow.cpp"));
    QCOMPARE(m.locations[0].second, 42);
}

void TestTsXmlIO::parseMultiContextFile()
{
    const QString path = writeTempFile(m_dir, QStringLiteral("multi.ts"),
                                       buildMultiContextTs());
    TsFile out;
    QVERIFY(TsXmlIO::load(path, out, nullptr));
    QCOMPARE(out.contexts.size(), 3);
    for (const TsContext &c : out.contexts)
        QCOMPARE(c.messages.size(), 5);
    QCOMPARE(out.language, QStringLiteral("de"));
}

void TestTsXmlIO::preservesStatusTypes()
{
    const QString path = writeTempFile(m_dir, QStringLiteral("status.ts"),
                                       buildMultiContextTs());
    TsFile out;
    QVERIFY(TsXmlIO::load(path, out, nullptr));

    // Index pattern: 0=finished, 1=unfinished, 2=obsolete, 3=finished, 4=unfinished
    const TsContext &c = out.contexts.first();
    QVERIFY(c.messages[0].status.isEmpty());
    QCOMPARE(c.messages[1].status, QStringLiteral("unfinished"));
    QCOMPARE(c.messages[2].status, QStringLiteral("obsolete"));
    QVERIFY(c.messages[3].status.isEmpty());
    QCOMPARE(c.messages[4].status, QStringLiteral("unfinished"));
}

void TestTsXmlIO::preservesLocations()
{
    const QString path = writeTempFile(m_dir, QStringLiteral("loc.ts"),
                                       buildMultiContextTs());
    TsFile out;
    QVERIFY(TsXmlIO::load(path, out, nullptr));

    const TsMessage &m0 = out.contexts.first().messages.first();
    QCOMPARE(m0.locations.size(), 2);
    QCOMPARE(m0.locations[0].first, QStringLiteral("../FileMenu.cpp"));
    QCOMPARE(m0.locations[0].second, 10);
    QCOMPARE(m0.locations[1].first, QStringLiteral("../FileMenu.h"));
    QCOMPARE(m0.locations[1].second, 20);
}

void TestTsXmlIO::saveOutputIsParseableByLoad()
{
    // Build a TsFile programmatically, save, re-load and compare structure.
    TsFile orig;
    orig.language       = QStringLiteral("fr_FR");
    orig.sourceLanguage = QStringLiteral("en");

    TsContext c;
    c.name = QStringLiteral("Dialog");

    TsMessage m1;
    m1.source      = QStringLiteral("Open");
    m1.translation = QStringLiteral("Ouvrir");
    m1.status      = QString();   // finished
    m1.locations << qMakePair(QStringLiteral("dlg.cpp"), 11);
    c.messages << m1;

    TsMessage m2;
    m2.source      = QStringLiteral("Save");
    m2.translation = QStringLiteral("Enregistrer");
    m2.status      = QStringLiteral("unfinished");
    m2.comment     = QStringLiteral("toolbar action");
    m2.locations << qMakePair(QStringLiteral("dlg.cpp"), 22);
    c.messages << m2;

    TsMessage m3;
    m3.source      = QStringLiteral("Quit");
    m3.translation = QStringLiteral("Quitter");
    m3.status      = QStringLiteral("obsolete");
    c.messages << m3;

    orig.contexts << c;

    const QString path = m_dir.filePath(QStringLiteral("saved.ts"));
    QString err;
    QVERIFY2(TsXmlIO::save(path, orig, &err), qPrintable(err));
    QVERIFY(QFileInfo::exists(path));

    TsFile reloaded;
    QVERIFY2(TsXmlIO::load(path, reloaded, &err), qPrintable(err));
    QCOMPARE(reloaded.language, orig.language);
    QCOMPARE(reloaded.sourceLanguage, orig.sourceLanguage);
    QCOMPARE(reloaded.contexts.size(), 1);
    QCOMPARE(reloaded.contexts[0].name, QStringLiteral("Dialog"));
    QCOMPARE(reloaded.contexts[0].messages.size(), 3);

    QCOMPARE(reloaded.contexts[0].messages[0].translation, QStringLiteral("Ouvrir"));
    QVERIFY(reloaded.contexts[0].messages[0].status.isEmpty());

    QCOMPARE(reloaded.contexts[0].messages[1].status, QStringLiteral("unfinished"));
    QCOMPARE(reloaded.contexts[0].messages[1].comment, QStringLiteral("toolbar action"));

    QCOMPARE(reloaded.contexts[0].messages[2].status, QStringLiteral("obsolete"));
}

void TestTsXmlIO::countersAreComputedCorrectly()
{
    const QString path = writeTempFile(m_dir, QStringLiteral("counts.ts"),
                                       buildMultiContextTs());
    TsFile out;
    QVERIFY(TsXmlIO::load(path, out, nullptr));

    // 3 contexts × 5 messages each = 15. Per context: 2 finished, 2 unfinished, 1 obsolete.
    QCOMPARE(out.totalCount(),     12);  // total excludes obsolete (3 obsolete excluded)
    QCOMPARE(out.finishedCount(),   6);  // 2 finished × 3 contexts
    QCOMPARE(out.unfinishedCount(), 6);  // 2 unfinished × 3 contexts
    QCOMPARE(out.obsoleteCount(),   3);  // 1 obsolete × 3 contexts
}

void TestTsXmlIO::loadFailsOnMissingRoot()
{
    const QString path = writeTempFile(
        m_dir, QStringLiteral("no_root.ts"),
        QStringLiteral("<?xml version=\"1.0\"?>\n<other></other>\n"));
    TsFile out;
    QString err;
    QVERIFY(!TsXmlIO::load(path, out, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains(QStringLiteral("TS")) || err.contains(QStringLiteral("root")));
}

void TestTsXmlIO::loadFailsOnMissingFile()
{
    const QString path = m_dir.filePath(QStringLiteral("no_such_file.ts"));
    TsFile out;
    QString err;
    QVERIFY(!TsXmlIO::load(path, out, &err));
    QVERIFY(!err.isEmpty());
}

QTEST_MAIN(TestTsXmlIO)
#include "test_tsxmlio.moc"

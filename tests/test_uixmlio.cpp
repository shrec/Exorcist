// test_uixmlio.cpp — round-trip + edge-case coverage for UiXmlIO.
//
// Covers the Designer .ui file I/O surface used by the custom form editor:
//
//   - load(path)            — parse .ui via QUiLoader → live QWidget tree
//   - save(path, root, conn)— write .ui XML from a live widget tree
//   - loadConnections(path) — parse <connections> block from a saved .ui
//
// The tests use minimal hand-crafted .ui XML strings written into a
// QTemporaryDir so they don't depend on the wider form editor or canvas.
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>
#include <QXmlStreamReader>

#include "formeditor/uixmlio.h"
// uixmlio.h transitively includes signalsloteditor.h for the FormConnection
// struct.  We deliberately do NOT compile signalsloteditor.cpp into this test
// — including it would force linking the SignalSlotEditor QDialog class and
// its formcanvas.cpp dependency.  We only need the POD struct.

using exo::forms::UiXmlIO;
using exo::forms::FormConnection;

namespace {

// Minimal .ui file with one root widget hosting a single QPushButton.
constexpr const char *kSimpleButtonUi = R"(<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>Form</class>
 <widget class="QWidget" name="Form">
  <property name="geometry">
   <rect><x>0</x><y>0</y><width>320</width><height>200</height></rect>
  </property>
  <property name="windowTitle">
   <string>SimpleForm</string>
  </property>
  <widget class="QPushButton" name="okBtn">
   <property name="geometry">
    <rect><x>10</x><y>10</y><width>120</width><height>30</height></rect>
   </property>
   <property name="text">
    <string>OK</string>
   </property>
  </widget>
 </widget>
 <resources/>
 <connections/>
</ui>
)";

// A .ui file with a root containing 3 children + a connection block.
constexpr const char *kThreeChildUi = R"(<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>Form</class>
 <widget class="QWidget" name="Form">
  <property name="geometry">
   <rect><x>0</x><y>0</y><width>400</width><height>300</height></rect>
  </property>
  <widget class="QPushButton" name="btnOk">
   <property name="geometry">
    <rect><x>20</x><y>20</y><width>100</width><height>30</height></rect>
   </property>
   <property name="text"><string>OK</string></property>
  </widget>
  <widget class="QPushButton" name="btnCancel">
   <property name="geometry">
    <rect><x>140</x><y>20</y><width>100</width><height>30</height></rect>
   </property>
   <property name="text"><string>Cancel</string></property>
  </widget>
  <widget class="QLineEdit" name="lineEdit">
   <property name="geometry">
    <rect><x>20</x><y>60</y><width>240</width><height>24</height></rect>
   </property>
  </widget>
 </widget>
 <resources/>
 <connections>
  <connection>
   <sender>btnCancel</sender>
   <signal>clicked()</signal>
   <receiver>Form</receiver>
   <slot>close()</slot>
  </connection>
 </connections>
</ui>
)";

// Garbled XML — must fail QUiLoader.
constexpr const char *kInvalidUi = R"(this is &&& not <ui> XML at all)";

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

bool fileContainsXmlElement(const QString &path, const QString &elementName)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QXmlStreamReader r(&f);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == elementName)
            return true;
    }
    return false;
}

} // namespace

class TestUiXmlIO : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void loadSimpleButtonUi();
    void saveProducesValidXmlReloadable();
    void roundTripPreservesHierarchyOfThreeChildren();
    void roundTripPreservesPropertyValues();
    void connectionsBlockRoundTrips();
    void invalidUiReturnsNullptr();
    void loadConnectionsHandlesMissingFile();
    void saveRejectsNullRoot();

private:
    QTemporaryDir m_dir;
};

void TestUiXmlIO::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

void TestUiXmlIO::loadSimpleButtonUi()
{
    const QString path = writeTempFile(m_dir, QStringLiteral("simple.ui"),
                                       QString::fromUtf8(kSimpleButtonUi));
    QVERIFY(!path.isEmpty());

    QWidget *w = UiXmlIO::load(path);
    QVERIFY(w != nullptr);
    QCOMPARE(w->objectName(), QStringLiteral("Form"));

    auto *btn = w->findChild<QPushButton *>(QStringLiteral("okBtn"));
    QVERIFY(btn != nullptr);
    QCOMPARE(btn->text(), QStringLiteral("OK"));
    delete w;
}

void TestUiXmlIO::saveProducesValidXmlReloadable()
{
    // Build a small live form in memory.
    auto *root = new QWidget;
    root->setObjectName(QStringLiteral("Form"));
    root->setGeometry(0, 0, 240, 160);

    auto *btn = new QPushButton(QStringLiteral("Hello"), root);
    btn->setObjectName(QStringLiteral("hello"));
    btn->setGeometry(10, 10, 100, 28);

    const QString path = m_dir.filePath(QStringLiteral("written.ui"));
    QVERIFY(UiXmlIO::save(path, root));
    QVERIFY(QFileInfo::exists(path));
    QVERIFY(fileContainsXmlElement(path, QStringLiteral("ui")));
    QVERIFY(fileContainsXmlElement(path, QStringLiteral("widget")));

    // Re-load: QUiLoader must parse what we just wrote.
    QWidget *reloaded = UiXmlIO::load(path);
    QVERIFY(reloaded != nullptr);
    auto *reBtn = reloaded->findChild<QPushButton *>(QStringLiteral("hello"));
    QVERIFY(reBtn != nullptr);

    delete reloaded;
    delete root;
}

void TestUiXmlIO::roundTripPreservesHierarchyOfThreeChildren()
{
    const QString src = writeTempFile(m_dir, QStringLiteral("three.ui"),
                                      QString::fromUtf8(kThreeChildUi));
    QWidget *w = UiXmlIO::load(src);
    QVERIFY(w != nullptr);

    // Save it back out, re-load, and verify the 3 children survive.
    const QString out = m_dir.filePath(QStringLiteral("three_out.ui"));
    QVERIFY(UiXmlIO::save(out, w));
    delete w;

    QWidget *re = UiXmlIO::load(out);
    QVERIFY(re != nullptr);

    QVERIFY(re->findChild<QPushButton *>(QStringLiteral("btnOk")) != nullptr);
    QVERIFY(re->findChild<QPushButton *>(QStringLiteral("btnCancel")) != nullptr);
    QVERIFY(re->findChild<QLineEdit *>(QStringLiteral("lineEdit")) != nullptr);
    delete re;
}

void TestUiXmlIO::roundTripPreservesPropertyValues()
{
    const QString src = writeTempFile(m_dir, QStringLiteral("props.ui"),
                                      QString::fromUtf8(kThreeChildUi));
    QWidget *w = UiXmlIO::load(src);
    QVERIFY(w != nullptr);

    auto *ok = w->findChild<QPushButton *>(QStringLiteral("btnOk"));
    QVERIFY(ok != nullptr);
    QCOMPARE(ok->text(), QStringLiteral("OK"));
    QCOMPARE(ok->geometry().width(), 100);
    QCOMPARE(ok->geometry().height(), 30);

    // Mark text as user-modified so save() will emit it (allowlist still fires
    // for "text" too, but we explicitly mark to exercise the dynamic-property
    // path used by the property inspector).
    ok->setProperty("exo_modified_text", true);

    const QString out = m_dir.filePath(QStringLiteral("props_out.ui"));
    QVERIFY(UiXmlIO::save(out, w));

    delete w;

    QWidget *re = UiXmlIO::load(out);
    QVERIFY(re != nullptr);
    auto *reOk = re->findChild<QPushButton *>(QStringLiteral("btnOk"));
    QVERIFY(reOk != nullptr);
    QCOMPARE(reOk->text(), QStringLiteral("OK"));
    QCOMPARE(reOk->geometry().width(), 100);
    delete re;
}

void TestUiXmlIO::connectionsBlockRoundTrips()
{
    const QString src = writeTempFile(m_dir, QStringLiteral("conn.ui"),
                                      QString::fromUtf8(kThreeChildUi));

    // Read the connections from the input file.
    const QList<FormConnection> in = UiXmlIO::loadConnections(src);
    QCOMPARE(in.size(), 1);
    QCOMPARE(in[0].sender,   QStringLiteral("btnCancel"));
    QCOMPARE(in[0].signal_,  QStringLiteral("clicked()"));
    QCOMPARE(in[0].receiver, QStringLiteral("Form"));
    QCOMPARE(in[0].slot_,    QStringLiteral("close()"));

    // Save with a fresh tree + a custom connection list, then re-parse.
    auto *root = new QWidget;
    root->setObjectName(QStringLiteral("Form"));
    auto *child = new QPushButton(QStringLiteral("X"), root);
    child->setObjectName(QStringLiteral("xBtn"));

    QList<FormConnection> custom;
    FormConnection c;
    c.sender   = QStringLiteral("xBtn");
    c.signal_  = QStringLiteral("clicked()");
    c.receiver = QStringLiteral("Form");
    c.slot_    = QStringLiteral("close()");
    custom << c;
    FormConnection c2;
    c2.sender   = QStringLiteral("xBtn");
    c2.signal_  = QStringLiteral("toggled(bool)");
    c2.receiver = QStringLiteral("Form");
    c2.slot_    = QStringLiteral("setEnabled(bool)");
    custom << c2;

    const QString out = m_dir.filePath(QStringLiteral("conn_out.ui"));
    QVERIFY(UiXmlIO::save(out, root, custom));
    delete root;

    const QList<FormConnection> back = UiXmlIO::loadConnections(out);
    QCOMPARE(back.size(), 2);
    QCOMPARE(back[0].sender,   QStringLiteral("xBtn"));
    QCOMPARE(back[0].slot_,    QStringLiteral("close()"));
    QCOMPARE(back[1].signal_,  QStringLiteral("toggled(bool)"));
    QCOMPARE(back[1].slot_,    QStringLiteral("setEnabled(bool)"));
}

void TestUiXmlIO::invalidUiReturnsNullptr()
{
    const QString path = writeTempFile(m_dir, QStringLiteral("broken.ui"),
                                       QString::fromUtf8(kInvalidUi));
    // Capture qWarning output — UiXmlIO is documented to log on failure.
    QWidget *w = UiXmlIO::load(path);
    QVERIFY(w == nullptr);
}

void TestUiXmlIO::loadConnectionsHandlesMissingFile()
{
    const QString missing = m_dir.filePath(QStringLiteral("no_such_file.ui"));
    const QList<FormConnection> empty = UiXmlIO::loadConnections(missing);
    QVERIFY(empty.isEmpty());
}

void TestUiXmlIO::saveRejectsNullRoot()
{
    const QString out = m_dir.filePath(QStringLiteral("null_root.ui"));
    QVERIFY(!UiXmlIO::save(out, nullptr));
    QVERIFY(!QFileInfo::exists(out));
}

QTEST_MAIN(TestUiXmlIO)
#include "test_uixmlio.moc"

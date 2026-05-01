#include "uixmlio.h"

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QMetaProperty>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QUiLoader>
#include <QVariant>
#include <QWidget>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

namespace exo::forms {
namespace {

// ── Property-emit policy ─────────────────────────────────────────────────────
//
// We don't blindly serialize every property the meta-system reports.  Most
// QWidget properties are inherited (visible, enabled, focusPolicy, palette,
// font…) and emitting them would make every saved .ui huge and brittle
// against Qt version changes.  Instead we maintain a curated allowlist of
// "interesting" properties per common widget type — geometry plus the things
// Designer typically writes.  Anything outside the list is skipped, even if
// the user changed it; Phase 2 can promote individual properties.
//
// The inspector marks every user-edited property with a dynamic property
// "exo_modified_<propname>" = true.  We always emit those, regardless of
// the allowlist, so user intent wins.
const QStringList &commonAllowlist() {
    static const QStringList kList = {
        "geometry", "objectName", "windowTitle",
        "text", "title", "placeholderText", "toolTip", "whatsThis",
        "checked", "checkable",
        "minimum", "maximum", "value", "singleStep", "pageStep",
        "orientation",
        "frameShape", "frameShadow", "lineWidth",
        "readOnly", "echoMode",
        "alignment",
        "icon", "iconSize",
        "flat", "autoExclusive",
        "currentIndex",
        "tabPosition", "tabShape",
        "spacing"
    };
    return kList;
}

bool isUserModified(const QWidget *w, const char *propName) {
    const QByteArray dyn = QByteArray("exo_modified_") + propName;
    return w->property(dyn.constData()).toBool();
}

void writeProperty(QXmlStreamWriter &xw, const QString &name, const QVariant &v) {
    xw.writeStartElement("property");
    xw.writeAttribute("name", name);

    switch (v.metaType().id()) {
    case QMetaType::Bool:
        xw.writeTextElement("bool", v.toBool() ? "true" : "false");
        break;
    case QMetaType::Int:
    case QMetaType::Long:
    case QMetaType::LongLong:
        xw.writeTextElement("number", QString::number(v.toLongLong()));
        break;
    case QMetaType::Double:
    case QMetaType::Float:
        xw.writeTextElement("double", QString::number(v.toDouble()));
        break;
    case QMetaType::QRect: {
        const QRect r = v.toRect();
        xw.writeStartElement("rect");
        xw.writeTextElement("x", QString::number(r.x()));
        xw.writeTextElement("y", QString::number(r.y()));
        xw.writeTextElement("width", QString::number(r.width()));
        xw.writeTextElement("height", QString::number(r.height()));
        xw.writeEndElement();
        break;
    }
    case QMetaType::QSize: {
        const QSize sz = v.toSize();
        xw.writeStartElement("size");
        xw.writeTextElement("width", QString::number(sz.width()));
        xw.writeTextElement("height", QString::number(sz.height()));
        xw.writeEndElement();
        break;
    }
    case QMetaType::QPoint: {
        const QPoint p = v.toPoint();
        xw.writeStartElement("point");
        xw.writeTextElement("x", QString::number(p.x()));
        xw.writeTextElement("y", QString::number(p.y()));
        xw.writeEndElement();
        break;
    }
    default:
        // Default to <string>; covers QString, enums (rendered via toString)
        // and anything QVariant can stringify.  Designer is lenient here.
        xw.writeTextElement("string", v.toString());
        break;
    }
    xw.writeEndElement(); // </property>
}

void writeWidget(QXmlStreamWriter &xw, QWidget *w,
                 const UiPromotionMap *promotions) {
    if (!w) return;

    xw.writeStartElement("widget");
    // Phase 3: if this widget is promoted, the <widget> element advertises
    // the promoted class name (e.g. "MyButton") instead of its Qt base
    // class.  Designer does the same — that's what makes the customwidgets
    // section meaningful: the loader on the other end (Designer or our
    // own UiXmlIO::load) sees the promoted name, finds it in
    // <customwidgets>, and knows what to extend.
    QString classToWrite = QString::fromLatin1(w->metaObject()->className());
    if (promotions && !w->objectName().isEmpty()) {
        auto it = promotions->find(w->objectName());
        if (it != promotions->end() && !it.value().promotedName.isEmpty())
            classToWrite = it.value().promotedName;
    }
    xw.writeAttribute("class", classToWrite);
    if (!w->objectName().isEmpty())
        xw.writeAttribute("name", w->objectName());

    const QStringList &allow = commonAllowlist();
    const QMetaObject *mo = w->metaObject();
    for (int i = 0; i < mo->propertyCount(); ++i) {
        const QMetaProperty p = mo->property(i);
        if (!p.isReadable()) continue;
        const QString name = QString::fromLatin1(p.name());
        if (name == QLatin1String("objectName")) continue; // already an attribute

        const bool modified = isUserModified(w, p.name());
        const bool inAllow = allow.contains(name);
        if (!modified && !inAllow) continue;

        const QVariant v = p.read(w);
        if (!v.isValid()) continue;
        writeProperty(xw, name, v);
    }

    // Walk direct child widgets only (skip layouts for Phase 1 — children
    // declared at the canvas level are positioned absolutely via geometry).
    const QList<QObject *> kids = w->children();
    for (QObject *child : kids) {
        QWidget *cw = qobject_cast<QWidget *>(child);
        if (!cw) continue;
        if (cw->property("exo_isHandle").toBool()) continue;     // grip handles
        if (cw->property("exo_isOverlay").toBool()) continue;    // selection paint
        writeWidget(xw, cw, promotions);
    }

    xw.writeEndElement(); // </widget>
}

// Phase 3: Emit the <customwidgets> block.  Designer's exact format:
//   <customwidgets>
//     <customwidget>
//       <class>MyButton</class>
//       <extends>QPushButton</extends>
//       <header>mybutton.h</header>
//     </customwidget>
//   </customwidgets>
// When the include is global, the <header> element gets a location="global"
// attribute (Designer convention).  We also dedupe by promoted class name
// because the same custom type can be applied to many widgets in one form
// but should appear in the list exactly once.
void writeCustomWidgets(QXmlStreamWriter &xw, const UiPromotionMap &promotions) {
    if (promotions.isEmpty()) return;

    // Dedupe: collect unique promoted-class entries.  Different objectNames
    // mapping to the same custom class collapse here.
    QHash<QString, UiCustomWidget> unique;
    for (auto it = promotions.begin(); it != promotions.end(); ++it) {
        const UiCustomWidget &cw = it.value();
        if (cw.promotedName.isEmpty()) continue;
        unique.insert(cw.promotedName, cw);
    }
    if (unique.isEmpty()) return;

    xw.writeStartElement("customwidgets");
    for (auto it = unique.begin(); it != unique.end(); ++it) {
        const UiCustomWidget &cw = it.value();
        xw.writeStartElement("customwidget");
        xw.writeTextElement("class", cw.promotedName);
        xw.writeTextElement("extends",
            cw.baseClass.isEmpty() ? QStringLiteral("QWidget") : cw.baseClass);
        xw.writeStartElement("header");
        if (cw.globalInclude) xw.writeAttribute("location", "global");
        xw.writeCharacters(cw.headerFile);
        xw.writeEndElement(); // </header>
        xw.writeEndElement(); // </customwidget>
    }
    xw.writeEndElement(); // </customwidgets>
}

} // namespace

QWidget *UiXmlIO::load(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[UiXmlIO] Cannot open" << path << ":" << f.errorString();
        return nullptr;
    }
    QUiLoader loader;
    QWidget *w = loader.load(&f, /*parent=*/nullptr);
    f.close();
    if (!w)
        qWarning() << "[UiXmlIO] QUiLoader failed for" << path;
    return w;
}

bool UiXmlIO::save(const QString &path, QWidget *root,
                   const QList<FormConnection> &connections,
                   const UiPromotionMap &promotions) {
    if (!root) return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[UiXmlIO] Cannot write" << path << ":" << f.errorString();
        return false;
    }

    QXmlStreamWriter xw(&f);
    xw.setAutoFormatting(true);
    xw.writeStartDocument();
    xw.writeStartElement("ui");
    xw.writeAttribute("version", "4.0");
    xw.writeTextElement("class", root->objectName().isEmpty()
                                     ? QStringLiteral("Form")
                                     : root->objectName());
    writeWidget(xw, root, promotions.isEmpty() ? nullptr : &promotions);

    // Phase 3: Designer places <customwidgets> after the form's <widget>
    // tree and before <resources>/<connections>.  We follow that order so
    // round-tripping through Designer leaves the file byte-for-byte
    // sensible.
    writeCustomWidgets(xw, promotions);

    xw.writeStartElement("resources"); xw.writeEndElement();

    // Phase 2: emit <connections> in Designer's exact format.  Each entry is
    //   <connection>
    //     <sender>name</sender><signal>clicked()</signal>
    //     <receiver>name</receiver><slot>close()</slot>
    //   </connection>
    xw.writeStartElement("connections");
    for (const FormConnection &c : connections) {
        if (c.sender.isEmpty() || c.receiver.isEmpty()) continue;
        if (c.signal_.isEmpty() || c.slot_.isEmpty()) continue;
        xw.writeStartElement("connection");
        xw.writeTextElement("sender",   c.sender);
        xw.writeTextElement("signal",   c.signal_);
        xw.writeTextElement("receiver", c.receiver);
        xw.writeTextElement("slot",     c.slot_);
        xw.writeEndElement(); // </connection>
    }
    xw.writeEndElement();   // </connections>

    xw.writeEndElement(); // </ui>
    xw.writeEndDocument();

    f.close();
    return true;
}

QList<FormConnection> UiXmlIO::loadConnections(const QString &path) {
    QList<FormConnection> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return out;
    QXmlStreamReader xr(&f);
    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isStartElement() && xr.name() == QStringLiteral("connection")) {
            FormConnection c;
            // Walk children.
            while (!(xr.isEndElement() && xr.name() == QStringLiteral("connection"))
                   && !xr.atEnd()) {
                xr.readNext();
                if (xr.isStartElement()) {
                    const QString tag = xr.name().toString();
                    const QString text = xr.readElementText();
                    if      (tag == "sender")   c.sender   = text;
                    else if (tag == "signal")   c.signal_  = text;
                    else if (tag == "receiver") c.receiver = text;
                    else if (tag == "slot")     c.slot_    = text;
                }
            }
            if (!c.sender.isEmpty() && !c.receiver.isEmpty()) out << c;
        }
    }
    f.close();
    return out;
}

// Phase 3: read the <customwidgets> block back into a (className → entry)
// map.  We scan the whole file so the order in which we encounter
// <customwidgets> vs <widget> doesn't matter — Designer is consistent but
// our writer evolves and we want to be tolerant.  Keys are promoted class
// names; the editor loop matches them up to live widgets via the live
// tree's <widget class="…"> attributes (which we read separately by
// re-parsing the XML during the load path — see UiFormEditor).
QHash<QString, UiCustomWidget> UiXmlIO::loadCustomWidgets(const QString &path) {
    QHash<QString, UiCustomWidget> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return out;
    QXmlStreamReader xr(&f);
    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isStartElement() && xr.name() == QStringLiteral("customwidget")) {
            UiCustomWidget cw;
            while (!(xr.isEndElement() && xr.name() == QStringLiteral("customwidget"))
                   && !xr.atEnd()) {
                xr.readNext();
                if (xr.isStartElement()) {
                    const QString tag = xr.name().toString();
                    if (tag == QStringLiteral("header")) {
                        // Capture the location attribute BEFORE consuming text,
                        // because readElementText() advances the reader past
                        // the start-element's attributes.
                        const QString loc = xr.attributes().value("location").toString();
                        cw.globalInclude = (loc == QStringLiteral("global"));
                        cw.headerFile = xr.readElementText();
                    } else {
                        const QString text = xr.readElementText();
                        if      (tag == QStringLiteral("class"))   cw.promotedName = text;
                        else if (tag == QStringLiteral("extends")) cw.baseClass    = text;
                    }
                }
            }
            if (!cw.promotedName.isEmpty())
                out.insert(cw.promotedName, cw);
        }
    }
    f.close();
    return out;
}

QHash<QString, QString> UiXmlIO::loadWidgetClassByName(const QString &path) {
    QHash<QString, QString> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return out;
    QXmlStreamReader xr(&f);
    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isStartElement() && xr.name() == QStringLiteral("widget")) {
            const QString cls  = xr.attributes().value("class").toString();
            const QString name = xr.attributes().value("name").toString();
            if (!name.isEmpty() && !cls.isEmpty())
                out.insert(name, cls);
        }
    }
    f.close();
    return out;
}

} // namespace exo::forms

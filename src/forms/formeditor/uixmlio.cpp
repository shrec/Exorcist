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

void writeWidget(QXmlStreamWriter &xw, QWidget *w) {
    if (!w) return;

    xw.writeStartElement("widget");
    xw.writeAttribute("class", QString::fromLatin1(w->metaObject()->className()));
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
        writeWidget(xw, cw);
    }

    xw.writeEndElement(); // </widget>
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

bool UiXmlIO::save(const QString &path, QWidget *root) {
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
    writeWidget(xw, root);
    xw.writeStartElement("resources"); xw.writeEndElement();
    xw.writeStartElement("connections"); xw.writeEndElement();
    xw.writeEndElement(); // </ui>
    xw.writeEndDocument();

    f.close();
    return true;
}

} // namespace exo::forms

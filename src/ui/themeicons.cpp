#include "themeicons.h"

#include <QFile>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

// ── Cache ─────────────────────────────────────────────────────────────────────

struct IconCacheKey {
    QString name;
    QSize   size;
    QString color;

    bool operator==(const IconCacheKey &o) const
    { return name == o.name && size == o.size && color == o.color; }
};

static size_t qHash(const IconCacheKey &k, size_t seed = 0)
{
    return qHashMulti(seed, k.name, k.size.width(), k.size.height(), k.color);
}

static QHash<IconCacheKey, QIcon> s_cache;

// ── Internal helpers ──────────────────────────────────────────────────────────

static QByteArray loadAndTint(const QString &name, const QString &color)
{
    // Icons live in :/icons/  (see resources.qrc)
    QFile f(QStringLiteral(":/icons/%1.svg").arg(name));
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QByteArray data = f.readAll();
    // Replace "currentColor" with the requested hex color.
    data.replace("currentColor", color.toUtf8());
    return data;
}

static QIcon renderSvg(const QByteArray &svg, const QSize &size)
{
    if (svg.isEmpty())
        return {};

    QSvgRenderer renderer(svg);
    if (!renderer.isValid())
        return {};

    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    renderer.render(&painter, QRectF(QPointF(0, 0), size));
    painter.end();

    return QIcon(QPixmap::fromImage(img));
}

// ── Public API ────────────────────────────────────────────────────────────────

QString ThemeIcons::defaultColor()
{
    // VS Code dark theme text: #cccccc
    return QStringLiteral("#cccccc");
}

QIcon ThemeIcons::icon(const QString &name, const QString &color)
{
    return ThemeIcons::icon(name, QSize(16, 16), color);
}

QIcon ThemeIcons::icon(const QString &name, const QSize &size, const QString &color)
{
    const QString tint = color.isEmpty() ? defaultColor() : color;
    const IconCacheKey key{name, size, tint};

    auto it = s_cache.find(key);
    if (it != s_cache.end())
        return it.value();

    const QByteArray svg = loadAndTint(name, tint);
    QIcon ic = renderSvg(svg, size);

    // If icon file not found, return a default empty icon (no crash).
    s_cache.insert(key, ic);
    return ic;
}

void ThemeIcons::clearCache()
{
    s_cache.clear();
}

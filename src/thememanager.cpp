#include "thememanager.h"

#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QStyleFactory>

// ── Palette role lookup ──────────────────────────────────────────────────────

static const struct { const char *name; QPalette::ColorRole role; } s_roleMap[] = {
    { "window",          QPalette::Window },
    { "windowText",      QPalette::WindowText },
    { "base",            QPalette::Base },
    { "alternateBase",   QPalette::AlternateBase },
    { "toolTipBase",     QPalette::ToolTipBase },
    { "toolTipText",     QPalette::ToolTipText },
    { "text",            QPalette::Text },
    { "button",          QPalette::Button },
    { "buttonText",      QPalette::ButtonText },
    { "brightText",      QPalette::BrightText },
    { "highlight",       QPalette::Highlight },
    { "highlightedText", QPalette::HighlightedText },
    { "link",            QPalette::Link },
    { "linkVisited",     QPalette::LinkVisited },
    { "placeholderText", QPalette::PlaceholderText },
};

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
}

void ThemeManager::setTheme(Theme theme)
{
    if (theme == Custom && !m_hasCustomTheme)
        return;
    if (m_theme == theme)
        return;
    m_theme = theme;
    if (m_theme == Dark)
        applyDark();
    else if (m_theme == Light)
        applyLight();
    else
        applyCustom();
    emit themeChanged(m_theme);
}

void ThemeManager::toggle()
{
    setTheme(m_theme == Dark ? Light : Dark);
}

bool ThemeManager::loadCustomTheme(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = tr("Cannot open %1").arg(path);
        return false;
    }

    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (error) *error = tr("JSON parse error: %1").arg(pe.errorString());
        return false;
    }
    if (!doc.isObject()) {
        if (error) *error = tr("Root must be a JSON object");
        return false;
    }

    const QJsonObject obj = doc.object();

    // Start from the built-in dark palette as base
    QPalette p;
    p.setColor(QPalette::Window,          QColor(30, 30, 30));
    p.setColor(QPalette::WindowText,      QColor(220, 220, 220));
    p.setColor(QPalette::Base,            QColor(20, 20, 20));
    p.setColor(QPalette::AlternateBase,   QColor(35, 35, 35));
    p.setColor(QPalette::ToolTipBase,     QColor(255, 255, 255));
    p.setColor(QPalette::ToolTipText,     QColor(0, 0, 0));
    p.setColor(QPalette::Text,            QColor(220, 220, 220));
    p.setColor(QPalette::Button,          QColor(40, 40, 40));
    p.setColor(QPalette::ButtonText,      QColor(220, 220, 220));
    p.setColor(QPalette::BrightText,      QColor(255, 0, 0));
    p.setColor(QPalette::Highlight,       QColor(42, 130, 218));
    p.setColor(QPalette::HighlightedText, QColor(0, 0, 0));
    p.setColor(QPalette::Link,            QColor(42, 130, 218));
    p.setColor(QPalette::LinkVisited,     QColor(150, 100, 200));
    p.setColor(QPalette::PlaceholderText, QColor(128, 128, 128));

    // Override with user-specified colours
    for (const auto &entry : s_roleMap) {
        if (obj.contains(QLatin1String(entry.name))) {
            const QColor c(obj.value(QLatin1String(entry.name)).toString());
            if (c.isValid())
                p.setColor(entry.role, c);
        }
    }

    m_customPalette  = p;
    m_hasCustomTheme = true;
    return true;
}

void ThemeManager::applyDark()
{
    auto *app = qApp;
    app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window,          QColor(30, 30, 30));
    p.setColor(QPalette::WindowText,      QColor(220, 220, 220));
    p.setColor(QPalette::Base,            QColor(20, 20, 20));
    p.setColor(QPalette::AlternateBase,   QColor(35, 35, 35));
    p.setColor(QPalette::ToolTipBase,     QColor(255, 255, 255));
    p.setColor(QPalette::ToolTipText,     QColor(0, 0, 0));
    p.setColor(QPalette::Text,            QColor(220, 220, 220));
    p.setColor(QPalette::Button,          QColor(40, 40, 40));
    p.setColor(QPalette::ButtonText,      QColor(220, 220, 220));
    p.setColor(QPalette::BrightText,      QColor(255, 0, 0));
    p.setColor(QPalette::Highlight,       QColor(42, 130, 218));
    p.setColor(QPalette::HighlightedText, QColor(0, 0, 0));
    p.setColor(QPalette::Link,            QColor(42, 130, 218));
    p.setColor(QPalette::LinkVisited,     QColor(150, 100, 200));
    p.setColor(QPalette::PlaceholderText, QColor(128, 128, 128));
    app->setPalette(p);
}

void ThemeManager::applyLight()
{
    auto *app = qApp;
    app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window,          QColor(240, 240, 240));
    p.setColor(QPalette::WindowText,      QColor(30, 30, 30));
    p.setColor(QPalette::Base,            QColor(255, 255, 255));
    p.setColor(QPalette::AlternateBase,   QColor(245, 245, 245));
    p.setColor(QPalette::ToolTipBase,     QColor(255, 255, 220));
    p.setColor(QPalette::ToolTipText,     QColor(0, 0, 0));
    p.setColor(QPalette::Text,            QColor(30, 30, 30));
    p.setColor(QPalette::Button,          QColor(225, 225, 225));
    p.setColor(QPalette::ButtonText,      QColor(30, 30, 30));
    p.setColor(QPalette::BrightText,      QColor(200, 0, 0));
    p.setColor(QPalette::Highlight,       QColor(0, 120, 215));
    p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    p.setColor(QPalette::Link,            QColor(0, 102, 204));
    p.setColor(QPalette::LinkVisited,     QColor(102, 45, 145));
    p.setColor(QPalette::PlaceholderText, QColor(160, 160, 160));
    app->setPalette(p);
}

void ThemeManager::applyCustom()
{
    auto *app = qApp;
    app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app->setPalette(m_customPalette);
}

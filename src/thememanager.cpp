#include "thememanager.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_theme == theme)
        return;
    m_theme = theme;
    if (m_theme == Dark)
        applyDark();
    else
        applyLight();
    emit themeChanged(m_theme);
}

void ThemeManager::toggle()
{
    setTheme(m_theme == Dark ? Light : Dark);
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

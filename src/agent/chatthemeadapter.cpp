#include "chatthemeadapter.h"

#include <QApplication>
#include <QFont>
#include <QFontInfo>
#include <QPalette>
#include <QTextBrowser>

ChatThemeAdapter::ChatThemeAdapter(QObject *parent)
    : QObject(parent)
{
    detectTheme();
}

void ChatThemeAdapter::setHighContrast(bool hc)
{
    m_mode = hc ? HighContrast : (isDarkPalette() ? Dark : Light);
    emit themeChanged();
}

QString ChatThemeAdapter::chatStyleSheet() const
{
    const auto pal = QApplication::palette();
    const QString bg = pal.color(QPalette::Base).name();
    const QString fg = pal.color(QPalette::Text).name();
    const QString highlight = pal.color(QPalette::Highlight).name();

    QString codeBg, codeBorder, userBubble, assistantBubble;
    switch (m_mode) {
    case Dark:
        codeBg = QStringLiteral("#1e1e2e");
        codeBorder = QStringLiteral("#383850");
        userBubble = QStringLiteral("#2d2d44");
        assistantBubble = QStringLiteral("#1a1a2e");
        break;
    case HighContrast:
        codeBg = QStringLiteral("#000000");
        codeBorder = QStringLiteral("#ffffff");
        userBubble = QStringLiteral("#000080");
        assistantBubble = QStringLiteral("#000000");
        break;
    case Light:
    default:
        codeBg = QStringLiteral("#f5f5f5");
        codeBorder = QStringLiteral("#e0e0e0");
        userBubble = QStringLiteral("#e3f2fd");
        assistantBubble = QStringLiteral("#ffffff");
        break;
    }

    const QString contrastBorder = (m_mode == HighContrast)
        ? QStringLiteral("border: 2px solid #ffffff;")
        : QString();

    return QStringLiteral(
        "body { background: %1; color: %2; font-family: -apple-system, 'Segoe UI', sans-serif; "
        "font-size: 13px; line-height: 1.5; }\n"
        ".user-msg { background: %3; border-radius: 8px; padding: 8px 12px; "
        "margin: 4px 0; %7 }\n"
        ".assistant-msg { background: %4; border-radius: 8px; padding: 8px 12px; "
        "margin: 4px 0; %7 }\n"
        "pre, code { background: %5; border: 1px solid %6; border-radius: 4px; "
        "font-family: 'Cascadia Code', 'Fira Code', 'Consolas', monospace; "
        "font-size: 12px; }\n"
        "pre { padding: 8px; overflow-x: auto; }\n"
        "code { padding: 1px 4px; }\n"
        "a { color: %8; }\n"
        ".tool-card { border: 1px solid %6; border-radius: 6px; padding: 6px 10px; "
        "margin: 4px 0; }\n"
        ".ref-pill { background: %5; border-radius: 3px; padding: 1px 6px; "
        "text-decoration: none; color: %8; }\n")
        .arg(bg, fg, userBubble, assistantBubble, codeBg, codeBorder,
             contrastBorder, highlight);
}

void ChatThemeAdapter::applyTo(QTextBrowser *browser)
{
    if (!browser) return;
    browser->document()->setDefaultStyleSheet(chatStyleSheet());
}

QFont ChatThemeAdapter::codeFont() const
{
    QFont f(QStringLiteral("Cascadia Code"));
    if (!QFontInfo(f).exactMatch())
        f = QFont(QStringLiteral("Consolas"));
    f.setPointSize(10);
    return f;
}

QFont ChatThemeAdapter::chatFont() const
{
    return QApplication::font();
}

void ChatThemeAdapter::refresh()
{
    detectTheme();
    emit themeChanged();
}

bool ChatThemeAdapter::isDarkPalette()
{
    const auto bg = QApplication::palette().color(QPalette::Window);
    return bg.lightnessF() < 0.5;
}

void ChatThemeAdapter::detectTheme()
{
    if (m_mode != HighContrast)
        m_mode = isDarkPalette() ? Dark : Light;
}

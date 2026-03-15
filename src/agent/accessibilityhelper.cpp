#include "accessibilityhelper.h"

#include <QApplication>
#include <QPalette>
#include <QWidget>
#include <QtMath>

AccessibilityHelper::AccessibilityHelper(QObject *parent) : QObject(parent) {}

bool AccessibilityHelper::isHighContrastMode()
{
    const auto pal = QApplication::palette();
    const QColor bg = pal.color(QPalette::Window);
    const QColor fg = pal.color(QPalette::WindowText);
    return qAbs(bg.lightnessF() - fg.lightnessF()) > 0.85;
}

double AccessibilityHelper::contrastRatio(const QColor &c1, const QColor &c2)
{
    const double l1 = relativeLuminance(c1);
    const double l2 = relativeLuminance(c2);
    const double lighter = qMax(l1, l2);
    const double darker = qMin(l1, l2);
    return (lighter + 0.05) / (darker + 0.05);
}

bool AccessibilityHelper::meetsWCAGAA(const QColor &fg, const QColor &bg)
{
    return contrastRatio(fg, bg) >= 4.5;
}

bool AccessibilityHelper::meetsWCAGAAA(const QColor &fg, const QColor &bg)
{
    return contrastRatio(fg, bg) >= 7.0;
}

void AccessibilityHelper::addFocusIndicator(QWidget *widget)
{
    if (!widget) return;
    widget->setStyleSheet(widget->styleSheet() +
        QStringLiteral("\n:focus { border: 2px solid palette(highlight); outline: none; }"));
}

void AccessibilityHelper::setAccessibleInfo(QWidget *widget, const QString &name,
                                             const QString &description)
{
    if (!widget) return;
    widget->setAccessibleName(name);
    if (!description.isEmpty())
        widget->setAccessibleDescription(description);
}

QString AccessibilityHelper::highContrastStyleSheet()
{
    return QStringLiteral(
        "QWidget { border: 1px solid #ffffff; }\n"
        "QPushButton { border: 2px solid #ffffff; padding: 4px 8px; }\n"
        "QPushButton:focus { border: 3px solid #ffff00; }\n"
        "QLineEdit, QPlainTextEdit, QTextEdit { border: 2px solid #ffffff; }\n"
        "QListWidget::item:selected { background: #0000ff; color: #ffffff; }\n"
        "QTabBar::tab:selected { border-bottom: 3px solid #ffff00; }\n"
        "a { color: #00ffff; text-decoration: underline; }\n");
}

double AccessibilityHelper::relativeLuminance(const QColor &c)
{
    auto linearize = [](double v) -> double {
        return v <= 0.03928 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4);
    };
    const double r = linearize(c.redF());
    const double g = linearize(c.greenF());
    const double b = linearize(c.blueF());
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

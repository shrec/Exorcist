#pragma once

#include <QColor>
#include <QObject>
#include <QString>

class QWidget;

// ─────────────────────────────────────────────────────────────────────────────
// AccessibilityHelper — WCAG AA compliance helpers for high contrast mode.
//
// Provides contrast checking, focus indicators, and screen reader hints.
// ─────────────────────────────────────────────────────────────────────────────

class AccessibilityHelper : public QObject
{
    Q_OBJECT

public:
    explicit AccessibilityHelper(QObject *parent = nullptr);

    static bool isHighContrastMode();
    static double contrastRatio(const QColor &c1, const QColor &c2);
    static bool meetsWCAGAA(const QColor &fg, const QColor &bg);
    static bool meetsWCAGAAA(const QColor &fg, const QColor &bg);
    static void addFocusIndicator(QWidget *widget);
    static void setAccessibleInfo(QWidget *widget, const QString &name,
                                   const QString &description = {});
    static QString highContrastStyleSheet();

private:
    static double relativeLuminance(const QColor &c);
};

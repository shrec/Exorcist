#pragma once

#include <QFont>
#include <QObject>
#include <QString>

class QTextBrowser;

// ─────────────────────────────────────────────────────────────────────────────
// ChatThemeAdapter — adapts chat panel styling to the current IDE theme.
//
// Detects dark/light mode from the application palette and generates
// appropriate CSS for chat bubbles, code blocks, and other UI elements.
// Supports high contrast mode for accessibility (WCAG AA).
// ─────────────────────────────────────────────────────────────────────────────

class ChatThemeAdapter : public QObject
{
    Q_OBJECT

public:
    explicit ChatThemeAdapter(QObject *parent = nullptr);

    enum ThemeMode { Light, Dark, HighContrast };

    ThemeMode mode() const { return m_mode; }

    void setHighContrast(bool hc);
    QString chatStyleSheet() const;
    void applyTo(QTextBrowser *browser);
    QFont codeFont() const;
    QFont chatFont() const;
    void refresh();

signals:
    void themeChanged();

private:
    static bool isDarkPalette();
    void detectTheme();

    ThemeMode m_mode = Light;
};

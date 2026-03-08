#pragma once

#include <QObject>
#include <QPalette>
#include <QString>

class QApplication;

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum Theme { Dark, Light, Custom };

    explicit ThemeManager(QObject *parent = nullptr);

    Theme currentTheme() const { return m_theme; }
    void setTheme(Theme theme);
    void toggle();

    /// Load a custom theme from a JSON file (e.g. .exorcist/theme.json).
    /// Returns true on success. On failure, sets *error if non-null.
    bool loadCustomTheme(const QString &path, QString *error = nullptr);

signals:
    void themeChanged(Theme theme);

private:
    void applyDark();
    void applyLight();
    void applyCustom();

    Theme    m_theme = Dark;
    QPalette m_customPalette;
    bool     m_hasCustomTheme = false;
};

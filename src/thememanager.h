#pragma once

#include <QObject>
#include <QPalette>

class QApplication;

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum Theme { Dark, Light };

    explicit ThemeManager(QObject *parent = nullptr);

    Theme currentTheme() const { return m_theme; }
    void setTheme(Theme theme);
    void toggle();

signals:
    void themeChanged(Theme theme);

private:
    void applyDark();
    void applyLight();

    Theme m_theme = Dark;
};

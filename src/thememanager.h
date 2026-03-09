#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QPalette>
#include <QString>
#include <QTextCharFormat>

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

    /// Editor token color accessor. Token names: keyword, type, string,
    /// comment, number, preproc, function, special.
    QTextCharFormat tokenFormat(const QString &tokenName) const;

    /// Theme metadata from JSON.
    QString themeName() const    { return m_themeName; }
    QString themeAuthor() const  { return m_themeAuthor; }
    QString themeVersion() const { return m_themeVersion; }

signals:
    void themeChanged(Theme theme);

private:
    void applyDark();
    void applyLight();
    void applyCustom();
    void buildDefaultTokenFormats();

    Theme    m_theme = Dark;
    QPalette m_customPalette;
    bool     m_hasCustomTheme = false;
    QHash<QString, QTextCharFormat> m_tokenFormats;
    QString  m_themeName;
    QString  m_themeAuthor;
    QString  m_themeVersion;
};

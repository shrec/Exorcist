#pragma once

#include <QObject>

class QApplication;

namespace Exorcist
{

/**
 * ThemeManager — runtime light/dark theme controller.
 *
 * Owns the current theme state, applies the corresponding palette+QSS to the
 * QApplication, persists the choice via QSettings, and emits themeChanged for
 * widgets that need to react (e.g. custom-painted panels, syntax highlighters).
 *
 * Use the singleton via ThemeManager::instance(). The free helpers
 * applyTheme() and setDarkTheme() below are thin wrappers preserved as a
 * stable C-style API for code that does not want to depend on QObject.
 */
class ThemeManager : public QObject
{
    Q_OBJECT
public:
    static ThemeManager &instance();

    bool isDark() const { return m_dark; }

    /// Apply the current theme to the given QApplication. If the desired mode
    /// is already active a re-apply is still performed (idempotent).
    void apply(QApplication &app);

    /// Switch theme at runtime and re-apply to the singleton qApp. Persists.
    void setDark(bool dark);

    /// Toggle between dark and light.
    void toggle() { setDark(!m_dark); }

    /// Return the QSS string for the requested mode without applying it.
    static QString stylesheet(bool dark);

signals:
    void themeChanged(bool dark);

private:
    ThemeManager();
    Q_DISABLE_COPY_MOVE(ThemeManager)

    bool m_dark = true;
    bool m_loaded = false;
};

/// Convenience: apply theme directly to an arbitrary QApplication. Used by
/// main.cpp at startup before the singleton is otherwise needed.
void applyTheme(QApplication &app, bool dark);

/// Convenience: re-apply on the live qApp and persist. Equivalent to
/// ThemeManager::instance().setDark(dark).
void setDarkTheme(bool dark);

} // namespace Exorcist

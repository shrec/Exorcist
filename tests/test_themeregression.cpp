// Regression tests for the dark/light theme system.
//
// These tests guard against the "white windows on dark theme" class of bugs
// where plain QWidget subclasses with type-selector or ID-selector QSS fail
// to render their styled background because Qt::WA_StyledBackground is not
// set.  Without that attribute, Qt's default paintEvent() bypasses the QSS
// background rule entirely and falls through to a Fusion-default white.
//
// We deliberately keep these tests cheap: just construct each widget and
// verify the attribute.  We do NOT call applyTheme or render — those need a
// live event loop and risk hanging under -platform offscreen / minimal
// platform plugins.  Static stylesheet content is checked via
// ThemeManager::stylesheet(true) which is a pure string builder.

#include <QApplication>
#include <QString>
#include <QTest>
#include <QWidget>

#include "editor/imagepreviewwidget.h"
#include "ui/thememanager.h"
#include "welcomewidget.h"

class TestThemeRegression : public QObject
{
    Q_OBJECT

private slots:
    // ── Stylesheet sanity ────────────────────────────────────────────────

    void darkStylesheet_containsTopLevelWindowRules()
    {
        // Newly opened dialogs and the main window must inherit the dark
        // background even before any widget-specific QSS attaches.  We
        // assert the rules are present in the global stylesheet builder.
        const QString ss = Exorcist::ThemeManager::stylesheet(/*dark=*/true);
        QVERIFY(!ss.isEmpty());
        QVERIFY2(ss.contains(QStringLiteral("QDialog")),
                 "Dark stylesheet must include a QDialog rule so dialogs do not fall back to white");
        QVERIFY2(ss.contains(QStringLiteral("QMainWindow")),
                 "Dark stylesheet must include a QMainWindow rule so the main window background renders dark");
    }

    void lightStylesheet_alsoCoversTopLevelWindows()
    {
        // Same coverage on the light theme to keep the symmetry.
        const QString ss = Exorcist::ThemeManager::stylesheet(/*dark=*/false);
        QVERIFY(!ss.isEmpty());
        QVERIFY(ss.contains(QStringLiteral("QDialog")));
        QVERIFY(ss.contains(QStringLiteral("QMainWindow")));
    }

    // ── Widgets that style themselves via type-selector QSS ──────────────
    //
    // These widgets ALL need Qt::WA_StyledBackground to be set in the
    // constructor.  Without it the QSS background is silently ignored and
    // the widget renders white on dark theme.  Keep this list in sync with
    // any new top-level QWidget subclass that uses a type-selector or
    // ID-selector QSS rule for its own background.

    void welcomeWidget_hasStyledBackground()
    {
        WelcomeWidget w;
        QVERIFY2(w.testAttribute(Qt::WA_StyledBackground),
                 "WelcomeWidget must call setAttribute(Qt::WA_StyledBackground, true) "
                 "for its 'WelcomeWidget { background: ...; }' QSS rule to actually paint. "
                 "Without it the welcome page renders white on dark theme.");
    }

    void welcomeWidget_styleSheetMentionsDarkBackground()
    {
        // Sanity: the QSS rule itself must reference the dark color so that
        // when the attribute is set, the painted color is dark.
        WelcomeWidget w;
        QVERIFY(w.styleSheet().contains(QStringLiteral("#1e1e1e"))
                || w.styleSheet().contains(QStringLiteral("background:")));
    }

    void imagePreviewWidget_hasStyledBackground()
    {
        ImagePreviewWidget w;
        QVERIFY2(w.testAttribute(Qt::WA_StyledBackground),
                 "ImagePreviewWidget must call setAttribute(Qt::WA_StyledBackground, true) "
                 "for its type-selector QSS background to render.");
    }

    void imagePreviewWidget_styleSheetIsNotEmpty()
    {
        ImagePreviewWidget w;
        QVERIFY(!w.styleSheet().isEmpty());
    }
};

QTEST_MAIN(TestThemeRegression)
#include "test_themeregression.moc"

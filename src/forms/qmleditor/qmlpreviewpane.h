// QmlPreviewPane — embedded live QML renderer for QmlEditorWidget.
//
// Wraps a QQuickWidget and exposes a single setQmlSource(QString) entry point.
// The QML text is written to a temporary file in QStandardPaths::TempLocation
// and loaded with QQuickWidget::setSource() (file URL form so relative imports
// like `import QtQuick` resolve through the standard import paths).
//
// On parse/runtime errors we surface the error list both via the
// errorMessage(QString) signal and a red overlay QLabel painted on top of the
// QQuickWidget so the user always sees what went wrong inline — no modal
// dialogs, per Exorcist UX principle 3 ("Inline > Modal").
#pragma once

#include <QWidget>
#include <QString>

class QQuickWidget;
class QLabel;
class QResizeEvent;

namespace exo::forms {

class QmlPreviewPane : public QWidget {
    Q_OBJECT
public:
    explicit QmlPreviewPane(QWidget *parent = nullptr);
    ~QmlPreviewPane() override;

    // Replace the QML source rendered in the preview. Empty string clears.
    void setQmlSource(const QString &qml);

    // Re-load whatever source was last set (used by F5 reload).
    void reload();

    // Clear preview and overlay.
    void clear();

signals:
    // Emitted whenever the underlying QQuickWidget reports parse/runtime
    // errors. The string is human-readable, multi-line.
    void errorMessage(const QString &msg);

    // Emitted on a successful (zero-error) reload.
    void previewReloaded();

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    void writeAndLoad(const QString &qml);
    void showError(const QString &msg);
    void hideError();

    QQuickWidget *m_quick = nullptr;
    QLabel       *m_errorOverlay = nullptr;
    QString       m_lastQml;
    QString       m_tempFilePath;
};

} // namespace exo::forms

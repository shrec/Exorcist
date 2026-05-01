// QssPreviewPane — embedded live QSS sample preview for QssEditorWidget.
//
// Hosts a representative gallery of standard Qt widgets (QPushButton,
// QLineEdit, QComboBox, QCheckBox, QRadioButton, QSlider, QProgressBar,
// QTabWidget, QListWidget, QGroupBox, QLabel, etc.) so that any QSS rules
// applied via setStyleSheet(QString) immediately flow through to all
// children and the user sees the visual effect.
//
// The pane keeps a VS dark-modern baseline palette so users can see what
// their custom QSS overrides change.
#pragma once

#include <QString>
#include <QWidget>

class QScrollArea;
class QVBoxLayout;

namespace exo::forms {

class QssPreviewPane : public QWidget {
    Q_OBJECT
public:
    explicit QssPreviewPane(QWidget *parent = nullptr);
    ~QssPreviewPane() override;

    // Apply a QSS string to the entire sample gallery. Empty string clears
    // back to the baseline VS dark theme. Returns true if applied OK (the
    // QStyle parser does not surface error info, so success is best-effort).
    bool applyStyleSheet(const QString &qss);

    // Re-apply whatever QSS was last set (used by F5 reload).
    void reload();

    // Drop user QSS, return to baseline.
    void clear();

signals:
    // Emitted on each successful apply (post-paint cycle).
    void stylesheetApplied();

private:
    void buildSampleGallery();
    void applyBaselineTheme();

    QScrollArea *m_scroll = nullptr;
    QWidget     *m_sampleHost = nullptr; // container we set the user QSS on
    QString      m_lastQss;
};

} // namespace exo::forms

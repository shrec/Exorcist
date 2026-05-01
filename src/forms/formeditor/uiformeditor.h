// uiformeditor.h — top-level Qt .ui form editor widget.
//
// This widget plugs into EditorManager as the tab body for *.ui files.  It is
// a self-contained QWidget (no MainWindow integration required beyond being
// instantiated and reparented into a QTabWidget) — the same contract the
// existing ImagePreviewWidget / HexEditorWidget / QrcEditorWidget honour.
//
// Layout (matches the spec; identical to the way Designer's ribbon → splitter
// → object-tree is laid out, but flatter and keyboard-first):
//
//   ┌──────────────────────────────────────────────────────────────┐
//   │ Toolbar: Edit/Preview · Undo · Redo · Align · Distribute · …│
//   ├────────────┬───────────────────────────────────┬────────────┤
//   │            │                                   │            │
//   │ Widget     │            Form Canvas            │  Property  │
//   │ Palette    │           (drop / select)         │  Inspector │
//   │ (200px)    │                                   │  (280px)   │
//   │            │                                   │            │
//   ├────────────┴───────────────────────────────────┴────────────┤
//   │ Object inspector breadcrumb · status                        │
//   └──────────────────────────────────────────────────────────────┘
//
// Shortcuts are wired via QAction with Qt::WidgetWithChildrenShortcut so they
// only fire when the editor has focus — important because these tabs share a
// QTabWidget with regular text editors and we don't want, say, Ctrl+S in a
// text editor to also trigger the form save handler.
#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QSplitter;
class QToolBar;
class QUndoStack;

namespace exo::forms {

class FormCanvas;
class PropertyInspector;
class WidgetPalette;

class UiFormEditor : public QWidget {
    Q_OBJECT
public:
    explicit UiFormEditor(QWidget *parent = nullptr);

    // Load a .ui file from disk and bind it to the canvas.  Returns false on
    // any failure (missing file, parse error).
    bool loadFromFile(const QString &path);

    // Save the current form to disk.  If `path` is empty, uses the path
    // remembered from loadFromFile().  Returns false on I/O error.
    bool saveToFile(const QString &path = QString());

    // Currently bound .ui file path (empty when the form has never been saved).
    QString filePath() const { return m_path; }

    // Whether the form has unsaved changes.
    bool isModified() const { return m_modified; }

signals:
    // Emitted whenever the modification state flips so the host can update
    // tab titles ("foo.ui *" suffix).
    void modificationChanged(bool modified);

private slots:
    void onCanvasModified();
    void onSelectionChanged();
    void onTogglePreview(bool on);
    void onSave();
    void onAlign(int alignmentEnum);
    void onDistributeH();
    void onDistributeV();

private:
    void buildUi();
    void buildToolbar(QToolBar *bar);
    void setModified(bool m);
    void updateBreadcrumb();

    QString            m_path;
    bool               m_modified = false;

    QToolBar          *m_toolbar    = nullptr;
    QSplitter         *m_splitter   = nullptr;
    WidgetPalette     *m_palette    = nullptr;
    FormCanvas        *m_canvas     = nullptr;
    PropertyInspector *m_inspector  = nullptr;
    QLabel            *m_breadcrumb = nullptr;
    QLabel            *m_status     = nullptr;
    QUndoStack        *m_undo       = nullptr;
};

} // namespace exo::forms

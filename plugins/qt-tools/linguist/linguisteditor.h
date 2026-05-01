// linguisteditor.h — Qt Linguist (.ts) translation file editor.
//
// Plugs into EditorManager as the tab body for *.ts files.  Self-contained
// QWidget (mirrors the contract honoured by ImagePreviewWidget /
// HexEditorWidget / QrcEditorWidget / UiFormEditor).
//
// Layout (per task spec):
//
//   ┌──────────────────────────────────────────────────────────────────────┐
//   │ Toolbar: language · search · Mark Finished · Mark Unfinished ·       │
//   │          Run lupdate · Run lrelease · Save · Reload                  │
//   ├──────────┬───────────────────────────────────────────┬───────────────┤
//   │ Contexts │ Translation table                         │ Source        │
//   │ (200px)  │  status · source · translation · location │ preview       │
//   │          │                                           │ (250px)       │
//   ├──────────┴───────────────────────────────────────────┴───────────────┤
//   │ Progress bar · status                                                 │
//   └──────────────────────────────────────────────────────────────────────┘
//
// Shortcuts (per docs/ux-principles.md):
//   Ctrl+S      → save .ts
//   Ctrl+F      → focus search
//   Ctrl+L      → focus context list
//   F5          → reload from disk
//   Ctrl+Enter  → mark finished + move to next unfinished
//   Enter       → save current translation, move to next unfinished
//   Esc         → cancel current edit (delegated to QAbstractItemView)
//   Tab/Shift+Tab → navigate cells (built into QTableView)
//
// All widget shortcuts use Qt::WidgetWithChildrenShortcut so they don't bleed
// into sibling text-editor tabs sharing the same QTabWidget.
#pragma once

#include <QHash>
#include <QString>
#include <QWidget>

#include "tsxmlio.h"

class QAbstractItemModel;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSortFilterProxyModel;
class QSplitter;
class QTableView;
class QToolBar;

namespace exo::forms::linguist {

class TranslationModel;

class LinguistEditor : public QWidget {
    Q_OBJECT
public:
    explicit LinguistEditor(QWidget *parent = nullptr);
    ~LinguistEditor() override;

    /// Parse a .ts XML file from disk and populate the table / contexts list.
    /// Returns true on success.  On failure the editor is left empty and the
    /// status label shows an error message.
    bool loadFromFile(const QString &path);

    /// Serialize the current state back to .ts XML at @p path (or the loaded
    /// path if @p path is empty).  Returns true on success.
    bool saveToFile(const QString &path = QString());

    QString filePath() const { return m_filePath; }
    bool    isModified() const { return m_modified; }

signals:
    /// Emitted when the modified flag flips so the host can render an
    /// asterisk on the tab title.
    void modificationChanged(bool modified);

public slots:
    void save();
    void reload();
    void focusSearch();
    void focusContextList();
    void markCurrentFinished();
    void markCurrentUnfinished();
    void runLupdate();
    void runLrelease();

private slots:
    void onContextSelectionChanged();
    void onTableSelectionChanged();
    void onSearchTextChanged(const QString &text);
    void onLanguageEdited();
    void onModelDataChanged();

private:
    void buildUi();
    void buildToolbar();
    void rebuildContextsList();
    void applyContextFilter();
    void updateProgressBar();
    void setModified(bool modified);
    void appendOutputLine(const QString &line);
    QString currentSourceFile() const;     // working dir for QProcess (project root or .ts dir)

    // ── State ──────────────────────────────────────────────────────────────
    QString          m_filePath;
    bool             m_modified = false;
    bool             m_loading  = false;
    TsFile           m_file;

    // Per-context indices (used for "MainWindow (12/20)" style labels).
    QHash<QString, QPair<int,int>> m_ctxStats; // name → (finished, total)

    // ── Widgets ────────────────────────────────────────────────────────────
    QToolBar              *m_toolbar         = nullptr;
    QLineEdit             *m_languageEdit    = nullptr;
    QLineEdit             *m_searchEdit      = nullptr;
    QPushButton           *m_finishedButton  = nullptr;
    QPushButton           *m_unfinishedButton= nullptr;
    QPushButton           *m_lupdateButton   = nullptr;
    QPushButton           *m_lreleaseButton  = nullptr;

    QSplitter             *m_splitter        = nullptr;
    QListWidget           *m_contextsList    = nullptr;
    QTableView            *m_table           = nullptr;
    QPlainTextEdit        *m_sourcePreview   = nullptr;

    QProgressBar          *m_progress        = nullptr;
    QLabel                *m_status          = nullptr;

    TranslationModel      *m_model           = nullptr;
    QSortFilterProxyModel *m_proxy           = nullptr;
};

} // namespace exo::forms::linguist

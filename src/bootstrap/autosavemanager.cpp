#include "autosavemanager.h"

#include "../editor/editormanager.h"
#include "../editor/editorview.h"
#include "sdk/hostservices.h"

#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QTabWidget>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>

namespace {
constexpr const char *kSettingsOrg     = "Exorcist";
constexpr const char *kSettingsApp     = "Exorcist";
constexpr const char *kKeyEnabled      = "autosave/enabled";
constexpr const char *kKeyInterval     = "autosave/interval";
constexpr const char *kKeyFormatOnSave = "editor/formatOnSave";
constexpr int         kDefaultSecs     = 30;
constexpr int         kMinSecs         = 1;

// How long to wait after invoking the formatter before writing the file. The
// LSP textDocument/formatting round-trip is asynchronous; clangd is local and
// usually responds in a few milliseconds, but we leave headroom for the
// editor to apply the resulting TextEdits.
constexpr int         kFormatGraceMs   = 80;

/// Walk up the parent chain looking for a HostServices child. AutoSaveManager
/// is parented to MainWindow, which owns HostServices as a child QObject.
HostServices *findHostServices(const QObject *self)
{
    for (const QObject *p = self ? self->parent() : nullptr; p; p = p->parent()) {
        if (auto *host = p->findChild<HostServices *>(QString(), Qt::FindDirectChildrenOnly))
            return host;
    }
    return nullptr;
}
} // namespace

AutoSaveManager::AutoSaveManager(EditorManager *editorMgr, QObject *parent)
    : QObject(parent)
    , m_editorMgr(editorMgr)
{
    QSettings s(kSettingsOrg, kSettingsApp);
    m_enabled         = s.value(kKeyEnabled, true).toBool();
    m_intervalSeconds = s.value(kKeyInterval, kDefaultSecs).toInt();
    m_formatOnSave    = s.value(kKeyFormatOnSave, false).toBool();
    if (m_intervalSeconds < kMinSecs)
        m_intervalSeconds = kDefaultSecs;

    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &AutoSaveManager::onTimerTick);
    applyTimerState();
}

void AutoSaveManager::setInterval(int seconds)
{
    if (seconds < kMinSecs)
        seconds = kMinSecs;
    if (seconds == m_intervalSeconds)
        return;
    m_intervalSeconds = seconds;

    QSettings s(kSettingsOrg, kSettingsApp);
    s.setValue(kKeyInterval, m_intervalSeconds);

    applyTimerState();
}

void AutoSaveManager::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;

    QSettings s(kSettingsOrg, kSettingsApp);
    s.setValue(kKeyEnabled, m_enabled);

    applyTimerState();
}

void AutoSaveManager::setFormatOnSave(bool on)
{
    if (on == m_formatOnSave)
        return;
    m_formatOnSave = on;

    QSettings s(kSettingsOrg, kSettingsApp);
    s.setValue(kKeyFormatOnSave, m_formatOnSave);
}

void AutoSaveManager::applyTimerState()
{
    if (m_enabled) {
        m_timer.start(m_intervalSeconds * 1000);
    } else {
        m_timer.stop();
    }
}

bool AutoSaveManager::isCppLikePath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("cpp")
        || suffix == QLatin1String("cc")
        || suffix == QLatin1String("cxx")
        || suffix == QLatin1String("c")
        || suffix == QLatin1String("h")
        || suffix == QLatin1String("hpp")
        || suffix == QLatin1String("hxx");
}

bool AutoSaveManager::saveEditorNow(EditorView *editor)
{
    if (!editor)
        return false;

    QTextDocument *doc = editor->document();
    if (!doc || !doc->isModified())
        return false;

    const QString path = editor->property("filePath").toString();
    if (path.isEmpty())
        return false;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    {
        QTextStream out(&f);
        out << editor->toPlainText();
    }
    f.close();

    if (f.error() == QFile::NoError) {
        doc->setModified(false);
        return true;
    }
    return false;
}

void AutoSaveManager::onTimerTick()
{
    if (!m_enabled || !m_editorMgr)
        return;

    QTabWidget *tabs = m_editorMgr->tabs();
    if (!tabs)
        return;

    // Resolve the command service lazily — it isn't available at construction
    // time because HostServices is created later in MainWindow's bootstrap.
    ICommandService *cmds = nullptr;
    if (m_formatOnSave) {
        if (HostServices *host = findHostServices(this))
            cmds = host->commands();
    }

    EditorView *active = m_editorMgr->currentEditor();

    const int count = tabs->count();
    for (int i = 0; i < count; ++i) {
        auto *editor = m_editorMgr->editorAt(i);
        if (!editor)
            continue;

        QTextDocument *doc = editor->document();
        if (!doc || !doc->isModified())
            continue;

        const QString path = editor->property("filePath").toString();
        if (path.isEmpty())
            continue; // Untitled buffer — skip.

        // Format-on-save path. Currently the cpp.formatDocument command is
        // bound to a single editor instance (the one captured at command
        // registration), and clangd's textDocument/formatting targets the
        // active document — so we only run formatting when this editor is the
        // active one. Other dirty editors fall through to a plain save.
        const bool wantFormat =
            m_formatOnSave
            && cmds
            && editor == active
            && isCppLikePath(path)
            && cmds->hasCommand(QStringLiteral("cpp.formatDocument"));

        if (wantFormat) {
            cmds->executeCommand(QStringLiteral("cpp.formatDocument"));
            // Format edits arrive asynchronously; defer the actual write so
            // the LSP TextEdits land in the document first. We capture by
            // QPointer-style raw pointer guarded inside the lambda by a
            // re-check against the tab widget on dispatch.
            QPointer<EditorView> guard(editor);
            QTimer::singleShot(kFormatGraceMs, this, [this, guard]() {
                if (!guard)
                    return;
                saveEditorNow(guard.data());
            });
            continue;
        }

        saveEditorNow(editor);
    }
}

#include "autosavemanager.h"

#include "../editor/editormanager.h"
#include "../editor/editorview.h"

#include <QFile>
#include <QSettings>
#include <QTabWidget>
#include <QTextDocument>
#include <QTextStream>

namespace {
constexpr const char *kSettingsOrg  = "Exorcist";
constexpr const char *kSettingsApp  = "Exorcist";
constexpr const char *kKeyEnabled   = "autosave/enabled";
constexpr const char *kKeyInterval  = "autosave/interval";
constexpr int         kDefaultSecs  = 30;
constexpr int         kMinSecs      = 1;
} // namespace

AutoSaveManager::AutoSaveManager(EditorManager *editorMgr, QObject *parent)
    : QObject(parent)
    , m_editorMgr(editorMgr)
{
    QSettings s(kSettingsOrg, kSettingsApp);
    m_enabled         = s.value(kKeyEnabled, true).toBool();
    m_intervalSeconds = s.value(kKeyInterval, kDefaultSecs).toInt();
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

void AutoSaveManager::applyTimerState()
{
    if (m_enabled) {
        m_timer.start(m_intervalSeconds * 1000);
    } else {
        m_timer.stop();
    }
}

void AutoSaveManager::onTimerTick()
{
    if (!m_enabled || !m_editorMgr)
        return;

    QTabWidget *tabs = m_editorMgr->tabs();
    if (!tabs)
        return;

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

        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            continue;

        {
            QTextStream out(&f);
            out << editor->toPlainText();
        }
        f.close();

        if (f.error() == QFile::NoError)
            doc->setModified(false);
    }
}

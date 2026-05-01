#include "qttoolsplugin.h"

#include "sdk/icommandservice.h"
#include "core/idockmanager.h"
#include "core/imenumanager.h"
#include "serviceregistry.h"
#include "editor/editormanager.h"

#include "qthelpdock.h"
#include "kitmanagerdialog.h"
#include "newqtclasswizard.h"
#include "formeditor/uiformeditor.h"
#include "qmleditor/qmleditorwidget.h"
#include "qsseditor/qsseditorwidget.h"
#include "linguist/linguisteditor.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QPointer>
#include <QProcess>
#include <QTextCursor>
#include <QPlainTextEdit>
#include <QShortcut>

QtToolsPlugin::QtToolsPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo QtToolsPlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.qt-tools"),
        QStringLiteral("Qt Tools"),
        QStringLiteral("1.1.0"),
        QStringLiteral("Qt development support: form editor, QML/QSS/Linguist editors, "
                       "help dock, kit manager, class wizard, and external tool launchers."),
        QStringLiteral("Exorcist"),
        QStringLiteral("1.0"),
        {PluginPermission::WorkspaceRead, PluginPermission::TerminalExecute}
    };
}

bool QtToolsPlugin::initializePlugin()
{
    registerCommands();
    registerFileExtensionEditors();
    registerQtHelpDock();

    // External Qt SDK tool launchers (Designer / Linguist / Assistant)
    ensureMenu(QStringLiteral("qt.tools"), tr("&Qt"));
    addMenuCommand(QStringLiteral("qt.tools"), tr("Open Qt &Designer"),
                   QStringLiteral("qt.openDesigner"), this);
    addMenuCommand(QStringLiteral("qt.tools"), tr("Open Qt &Linguist"),
                   QStringLiteral("qt.openLinguist"), this);
    addMenuCommand(QStringLiteral("qt.tools"), tr("Open Qt &Assistant"),
                   QStringLiteral("qt.openAssistant"), this);

    createToolBar(QStringLiteral("qt-tools"), tr("Qt"));
    addToolBarCommands(QStringLiteral("qt-tools"), {
        {tr("Designer"), QStringLiteral("qt.openDesigner")},
        {tr("Linguist"), QStringLiteral("qt.openLinguist")},
        {tr("Assistant"), QStringLiteral("qt.openAssistant")},
    }, this);

    // In-IDE Qt-development commands integrated into File and Tools menus.
    addMenuCommand(IMenuManager::File, tr("New Qt &Class..."),
                   QStringLiteral("qt.newClass"), this);
    addMenuCommand(IMenuManager::Tools, tr("&Manage Qt Kits..."),
                   QStringLiteral("qt.manageKits"), this);

    return true;
}

void QtToolsPlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    // External Qt SDK tools (separate processes)
    cmds->registerCommand(QStringLiteral("qt.openDesigner"),
                          tr("Qt: Open Designer"),
                          [this]() { launchTool(QStringLiteral("designer")); });
    cmds->registerCommand(QStringLiteral("qt.openLinguist"),
                          tr("Qt: Open Linguist"),
                          [this]() { launchTool(QStringLiteral("linguist")); });
    cmds->registerCommand(QStringLiteral("qt.openAssistant"),
                          tr("Qt: Open Assistant"),
                          [this]() { launchTool(QStringLiteral("assistant")); });

    // In-IDE Qt commands
    cmds->registerCommand(QStringLiteral("qt.newClass"),
                          tr("Qt: New Class..."),
                          [this]() {
                              NewQtClassWizard dlg(QApplication::activeWindow());
                              if (dlg.exec() == QDialog::Accepted) {
                                  const QString hdr = dlg.headerPath();
                                  if (!hdr.isEmpty() && QFileInfo::exists(hdr))
                                      openFile(hdr);
                              }
                          });

    cmds->registerCommand(QStringLiteral("qt.manageKits"),
                          tr("Qt: Manage Kits..."),
                          [this]() {
                              KitManagerDialog dlg(QApplication::activeWindow());
                              dlg.exec();
                          });

    cmds->registerCommand(QStringLiteral("qt.lookupHelp"),
                          tr("Qt: Look Up Documentation"),
                          [this]() {
                              if (!m_qtHelpDock) return;
                              QString word;
                              if (auto *focused = qobject_cast<QPlainTextEdit *>(
                                      QApplication::focusWidget())) {
                                  QTextCursor cur = focused->textCursor();
                                  cur.select(QTextCursor::WordUnderCursor);
                                  word = cur.selectedText().trimmed();
                              }
                              if (auto *d = docks())
                                  d->showPanel(QStringLiteral("QtHelpDock"));
                              if (!word.isEmpty())
                                  m_qtHelpDock->lookupKeyword(word);
                          });
}

void QtToolsPlugin::registerFileExtensionEditors()
{
    // Plugin claims .ui/.ts/.qml/.qss extensions. EditorManager (in src/editor/)
    // queries m_extHandlers in openFile() and routes matching files here.
    // Service is registered by MainWindow as "editorManager".
    auto *em = service<EditorManager>(QStringLiteral("editorManager"));
    if (!em) {
        showWarning(tr("qt-tools: editorManager service not found — "
                       ".ui/.ts/.qml/.qss files will not get specialized editors."));
        return;
    }

    em->registerFileExtensionHandler({QStringLiteral("ui")},
        [](const QString &path) -> QWidget * {
            auto *form = new exo::forms::UiFormEditor();
            if (!form->loadFromFile(path)) {
                delete form;
                return nullptr;
            }
            return form;
        });

    em->registerFileExtensionHandler({QStringLiteral("ts")},
        [](const QString &path) -> QWidget * {
            auto *ling = new exo::forms::linguist::LinguistEditor();
            if (!ling->loadFromFile(path)) {
                delete ling;
                return nullptr;
            }
            return ling;
        });

    em->registerFileExtensionHandler({QStringLiteral("qml")},
        [](const QString &path) -> QWidget * {
            auto *qml = new exo::forms::QmlEditorWidget();
            if (!qml->loadFromFile(path)) {
                delete qml;
                return nullptr;
            }
            return qml;
        });

    em->registerFileExtensionHandler({QStringLiteral("qss")},
        [](const QString &path) -> QWidget * {
            auto *qss = new exo::forms::QssEditorWidget();
            if (!qss->loadFromFile(path)) {
                delete qss;
                return nullptr;
            }
            return qss;
        });
}

void QtToolsPlugin::registerQtHelpDock()
{
    auto *dm = docks();
    if (!dm) return;

    m_qtHelpDock = new QtHelpDock();   // parented by dock manager when added
    dm->addPanel(QStringLiteral("QtHelpDock"),
                 tr("Qt Help"),
                 m_qtHelpDock,
                 IDockManager::Right,
                 /*startVisible=*/false);

    // F1 over a word in the active editor: open dock + lookup keyword.
    if (auto *cmds = commands()) {
        // The command itself was registered above; here we bind F1 to it.
        auto *sc = new QShortcut(QKeySequence(Qt::Key_F1), QApplication::activeWindow());
        sc->setContext(Qt::ApplicationShortcut);
        QObject::connect(sc, &QShortcut::activated, this, [this]() {
            executeCommand(QStringLiteral("qt.lookupHelp"));
        });
    }
}

bool QtToolsPlugin::launchTool(const QString &program, const QStringList &arguments)
{
    const QString workingDir = workspaceRoot();
    const bool started = QProcess::startDetached(program, arguments, workingDir);
    if (!started)
        showWarning(tr("Unable to launch %1. Ensure the Qt tool is installed and on PATH.").arg(program));
    return started;
}

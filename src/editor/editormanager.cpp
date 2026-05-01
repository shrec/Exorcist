#include "editormanager.h"

#include "editorview.h"
#include "hexeditorwidget.h"
#include "highlighterfactory.h"
#include "imagepreviewwidget.h"
#include "largefileloader.h"
#include "qrceditorwidget.h"

#include "../core/ifilesystem.h"
#include "../lsp/lspclient.h"
#include "../settings/languageprofile.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QSet>
#include <QSettings>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextOption>

EditorManager::EditorManager(QObject *parent)
    : QObject(parent)
{
}

EditorView *EditorManager::currentEditor() const
{
    return m_tabs ? qobject_cast<EditorView *>(m_tabs->currentWidget()) : nullptr;
}

EditorView *EditorManager::editorAt(int index) const
{
    return m_tabs ? qobject_cast<EditorView *>(m_tabs->widget(index)) : nullptr;
}

// ── File / tab lifecycle ───────────────────────────────────────────────────

void EditorManager::openFile(const QString &path)
{
    if (!m_tabs) return;

    if (m_fileSystem && !m_fileSystem->exists(path)) {
        emit statusMessage(tr("File not found: %1").arg(path), 4000);
        return;
    }

    // Switch from welcome page to editor view
    if (m_centralStack && m_centralStack->currentIndex() == 0)
        m_centralStack->setCurrentIndex(1);

    // Focus existing tab if already open
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->widget(i)->property("filePath").toString() == path) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }

    // ── Plugin-contributed file-extension handlers ─────────────────────────
    //    Plugins (e.g. qt-tools for .ui/.ts/.qml/.qss) register handlers
    //    via registerFileExtensionHandler().  EditorManager owns tab
    //    lifecycle and generically wires modificationChanged(bool) and
    //    statusMessage(QString,int) signals if the widget exposes them
    //    (probed via QMetaObject so the manager doesn't depend on plugin
    //    types).
    {
        const QString ext = QFileInfo(path).suffix().toLower();
        const auto it = m_extHandlers.constFind(ext);
        if (it != m_extHandlers.constEnd()) {
            QWidget *w = it.value()(path);
            if (w) {
                w->setProperty("filePath", path);
                const QString title = QFileInfo(path).fileName();
                const int index = m_tabs->addTab(w, title);
                m_tabs->setTabToolTip(index, QDir::toNativeSeparators(path));
                m_tabs->setCurrentIndex(index);

                const QMetaObject *mo = w->metaObject();
                w->setProperty("__exoTabTitle", title);

                // Generic: tab-title asterisk on modificationChanged(bool)
                if (mo->indexOfSignal(
                        QMetaObject::normalizedSignature("modificationChanged(bool)").constData()) >= 0) {
                    QObject::connect(w, SIGNAL(modificationChanged(bool)),
                                     this, SLOT(onExtensionTabModified(bool)),
                                     Qt::UniqueConnection);
                }
                // Generic: forward statusMessage(QString,int) to status bar
                if (mo->indexOfSignal(
                        QMetaObject::normalizedSignature("statusMessage(QString,int)").constData()) >= 0) {
                    QObject::connect(w, SIGNAL(statusMessage(QString,int)),
                                     this, SIGNAL(statusMessage(QString,int)),
                                     Qt::UniqueConnection);
                }
                return;
            }
            // Handler returned nullptr — fall through to default text editor.
        }
    }

    // ── Image preview path: open .png/.jpg/.jpeg/.bmp/.gif/.svg in a viewer
    //    instead of the binary text editor.  ImagePreviewWidget is a plain
    //    QWidget (not EditorView); currentEditor()/editorAt() qobject_cast to
    //    EditorView and return nullptr for these tabs, so existing flows are
    //    safe.
    {
        const QString ext = QFileInfo(path).suffix().toLower();
        static const QSet<QString> kImageExts = {
            QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
            QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("svg")
        };
        if (kImageExts.contains(ext)) {
            auto *preview = new ImagePreviewWidget();
            const bool ok = preview->loadImage(path);
            if (!ok)
                emit statusMessage(tr("Failed to open image: %1").arg(path), 4000);
            preview->setProperty("filePath", path);

            const QString title = QFileInfo(path).fileName();
            const int index = m_tabs->addTab(preview, title);
            m_tabs->setTabToolTip(index, QDir::toNativeSeparators(path));
            m_tabs->setCurrentIndex(index);
            return;
        }
    }

    // (Qt-specific .qml/.ui/.ts/.qss handlers are now contributed by the
    //  qt-tools plugin via registerFileExtensionHandler() above.)

    // ── Qt Resource Collection (.qrc) path: route to a structured tree-based
    //    editor instead of a raw XML view.  QrcEditorWidget is a plain QWidget
    //    (not an EditorView), so editorAt() / currentEditor() qobject_cast<>
    //    safely returns nullptr for these tabs and existing flows are preserved.
    {
        const QString ext = QFileInfo(path).suffix().toLower();
        if (ext == QStringLiteral("qrc")) {
            auto *qrc = new QrcEditorWidget();
            const bool ok = qrc->loadFromFile(path);
            if (!ok)
                emit statusMessage(tr("Failed to parse .qrc: %1").arg(path), 4000);
            qrc->setProperty("filePath", path);

            const QString title = QFileInfo(path).fileName();
            const int index = m_tabs->addTab(qrc, title);
            m_tabs->setTabToolTip(index, QDir::toNativeSeparators(path));
            m_tabs->setCurrentIndex(index);

            // Reflect the modified flag in the tab title (asterisk suffix).
            QTabWidget *tabsRef = m_tabs;
            connect(qrc, &QrcEditorWidget::modificationChanged, tabsRef,
                    [tabsRef, qrc, title](bool modified) {
                        const int idx = tabsRef->indexOf(qrc);
                        if (idx < 0) return;
                        tabsRef->setTabText(idx, modified ? title + QStringLiteral(" *") : title);
                    });
            return;
        }
    }

    // ── Binary path: detect by extension or null-byte sniff and route to a
    //    read-only HexEditorWidget instead of trying to render the bytes as
    //    text.  This protects the editor from huge binary blobs and gives
    //    the user a useful view (offset / hex / ASCII).  Like the image
    //    preview widget above, HexEditorWidget is a plain QWidget so the
    //    editorAt()/currentEditor() qobject_cast<EditorView*> path safely
    //    returns nullptr for these tabs.
    {
        const QString ext = QFileInfo(path).suffix().toLower();
        static const QStringList kBinaryExts = {
            QStringLiteral("bin"),  QStringLiteral("exe"),
            QStringLiteral("dll"),  QStringLiteral("so"),
            QStringLiteral("dylib"), QStringLiteral("obj"),
            QStringLiteral("o"),    QStringLiteral("a"),
            QStringLiteral("lib"),
            QStringLiteral("zip"),  QStringLiteral("tar"),
            QStringLiteral("gz"),   QStringLiteral("7z"),
            QStringLiteral("rar"),  QStringLiteral("ico")
        };
        bool isBinary = kBinaryExts.contains(ext);
        if (!isBinary) {
            // Sniff first 4KB for a NUL byte — text files almost never have
            // them.  Cheap heuristic, good enough to catch unknown binaries
            // (firmware images, .pak, .dat, etc.).
            QFile f(path);
            if (f.open(QIODevice::ReadOnly)) {
                const QByteArray sample = f.read(4096);
                f.close();
                for (int i = 0; i < sample.size(); ++i) {
                    if (sample.at(i) == '\0') { isBinary = true; break; }
                }
            }
        }
        if (isBinary) {
            auto *hex = new HexEditorWidget();
            hex->loadFile(path);
            hex->setProperty("filePath", path);

            const QString title = QFileInfo(path).fileName();
            const int index = m_tabs->addTab(hex, title);
            m_tabs->setTabToolTip(index, QDir::toNativeSeparators(path));
            m_tabs->setCurrentIndex(index);
            return;
        }
    }

    auto *editor = new EditorView();
    constexpr qint64 kLargeFileThreshold = 10 * 1024 * 1024;
    LargeFileLoader::applyToEditor(editor, path, kLargeFileThreshold);
    editor->setProperty("filePath", path);
    HighlighterFactory::create(path, editor->document());

    // Apply saved editor settings
    {
        QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        s.beginGroup(QStringLiteral("editor"));
        const QFont font(
            s.value(QStringLiteral("fontFamily"), QStringLiteral("Consolas")).toString(),
            s.value(QStringLiteral("fontSize"), 11).toInt());
        const int tabSize  = s.value(QStringLiteral("tabSize"), 4).toInt();
        const bool wrap    = s.value(QStringLiteral("wordWrap"), false).toBool();
        const bool minimap = s.value(QStringLiteral("showMinimap"), false).toBool();
        s.endGroup();

        editor->setFont(font);
        editor->setTabStopDistance(
            QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * tabSize);
        editor->setWordWrapMode(wrap ? QTextOption::WordWrap : QTextOption::NoWrap);
        editor->setMinimapVisible(minimap);
    }

    // Set language ID for inline chat / inline completion
    const QString langId = LspClient::languageIdForPath(path);
    editor->setLanguageId(langId);

    // Apply language-profile tab-size override
    if (m_langProfile) {
        QSettings gs(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        gs.beginGroup(QStringLiteral("editor"));
        const int globalTab = gs.value(QStringLiteral("tabSize"), 4).toInt();
        gs.endGroup();

        const int langTab = m_langProfile->tabSize(langId, globalTab);
        if (langTab != globalTab) {
            const QFont f = editor->font();
            editor->setTabStopDistance(
                QFontMetricsF(f).horizontalAdvance(QLatin1Char(' ')) * langTab);
        }
    }

    // Review annotation navigation (F8 / Shift+F8) — widget-scoped shortcuts
    auto *nextRevAction = new QAction(editor);
    nextRevAction->setShortcut(QKeySequence(Qt::Key_F8));
    nextRevAction->setShortcutContext(Qt::WidgetShortcut);
    connect(nextRevAction, &QAction::triggered, editor, &EditorView::nextReviewAnnotation);
    editor->addAction(nextRevAction);

    auto *prevRevAction = new QAction(editor);
    prevRevAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F8));
    prevRevAction->setShortcutContext(Qt::WidgetShortcut);
    connect(prevRevAction, &QAction::triggered, editor, &EditorView::prevReviewAnnotation);
    editor->addAction(prevRevAction);

    // Notify observers (MainWindow wires LSP, AI actions, breakpoints, etc.)
    emit editorOpened(editor, path);

    const QString title = QFileInfo(path).fileName();
    const int index = m_tabs->addTab(editor, title);
    m_tabs->setTabToolTip(index, QDir::toNativeSeparators(path));
    m_tabs->setCurrentIndex(index);

    if (editor->isLargeFilePreview()) {
        emit statusMessage(tr("Large file preview (read-only)"), 5000);
        connect(editor, &EditorView::requestMoreData, this, [editor]() {
            LargeFileLoader::appendNextChunk(editor, 2 * 1024 * 1024);
        });
    }
}

void EditorManager::closeTab(int index)
{
    if (!m_tabs) return;
    QWidget *w = m_tabs->widget(index);
    const QString path = w ? w->property("filePath").toString() : QString();
    m_tabs->removeTab(index);
    if (w) w->deleteLater();
    emit tabClosed(path);
}

void EditorManager::closeAllTabs()
{
    if (!m_tabs) return;
    while (m_tabs->count() > 0)
        closeTab(0);
}

void EditorManager::closeOtherTabs(int keepIndex)
{
    if (!m_tabs) return;
    QWidget *keep = m_tabs->widget(keepIndex);
    for (int i = m_tabs->count() - 1; i >= 0; --i) {
        if (m_tabs->widget(i) != keep)
            closeTab(i);
    }
}

// ── Plugin file-extension handlers ─────────────────────────────────────────

void EditorManager::registerFileExtensionHandler(const QStringList &extensions,
                                                 FileExtensionHandler handler)
{
    if (!handler) return;
    for (const QString &ext : extensions)
        m_extHandlers.insert(ext.toLower(), handler);
}

void EditorManager::onExtensionTabModified(bool modified)
{
    if (!m_tabs) return;
    auto *w = qobject_cast<QWidget *>(sender());
    if (!w) return;
    const int idx = m_tabs->indexOf(w);
    if (idx < 0) return;
    const QString title = w->property("__exoTabTitle").toString();
    if (title.isEmpty()) return;
    m_tabs->setTabText(idx, modified ? title + QStringLiteral(" *") : title);
}

#include "terminalpanel.h"
#include "terminalwidget.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QShortcut>
#include <QStandardPaths>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#if defined(Q_OS_WIN)
#  include <QProcessEnvironment>
#endif

namespace {

// Tab styling matched to DebugPanel's VS-2022 dark scheme.
constexpr const char *kTabStyleSheet =
    "QTabWidget::pane {"
    "  background: #1e1e1e;"
    "  border: none;"
    "  border-top: 1px solid #3e3e42;"
    "}"
    "QTabBar {"
    "  background: #252526;"
    "}"
    "QTabBar::tab {"
    "  background: #2d2d30;"
    "  color: #9d9d9d;"
    "  padding: 4px 12px;"
    "  border: none;"
    "  border-right: 1px solid #1e1e1e;"
    "  font-size: 12px;"
    "}"
    "QTabBar::tab:selected {"
    "  background: #1e1e1e;"
    "  color: #ffffff;"
    "  border-top: 1px solid #007acc;"
    "}"
    "QTabBar::tab:hover:!selected {"
    "  background: #3e3e42;"
    "  color: #cccccc;"
    "}"
    "QTabBar::close-button {"
    "  image: url(:/icons/close-tab.svg);"
    "  subcontrol-position: right;"
    "}"
    "QTabBar::close-button:hover {"
    "  background: #5a1d1d;"
    "  border-radius: 2px;"
    "}";

constexpr const char *kAddBtnStyleSheet =
    "QToolButton {"
    "  color: #cccccc;"
    "  background: #2d2d30;"
    "  border: none;"
    "  padding: 3px 10px;"
    "  font-size: 14px;"
    "}"
    "QToolButton:hover { color: #ffffff; background: #3e3e42; }";

constexpr const char *kProfileComboStyleSheet =
    "QComboBox {"
    "  color: #cccccc;"
    "  background: #252526;"
    "  border: 1px solid #3e3e42;"
    "  border-radius: 2px;"
    "  padding: 2px 22px 2px 8px;"
    "  font-size: 11px;"
    "  min-width: 90px;"
    "}"
    "QComboBox:hover { background: #2d2d30; border-color: #505054; }"
    "QComboBox::drop-down { border: none; width: 18px; }"
    "QComboBox::down-arrow { image: none; }"
    "QComboBox QAbstractItemView {"
    "  background: #252526;"
    "  color: #cccccc;"
    "  border: 1px solid #3e3e42;"
    "  selection-background-color: #094771;"
    "  selection-color: #ffffff;"
    "  outline: none;"
    "}";

bool fileExists(const QString &p)
{
    return !p.isEmpty() && QFileInfo(p).exists();
}

#if defined(Q_OS_WIN)
QString findOnPath(const QString &exe)
{
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString pathVar = env.value(QStringLiteral("PATH"));
    const auto parts = pathVar.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &dir : parts) {
        const QString cand = dir + QLatin1Char('/') + exe;
        if (QFileInfo(cand).exists())
            return cand;
    }
    return {};
}
#endif

} // namespace

// ── Construction ──────────────────────────────────────────────────────────────

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent),
      m_tabs(new QTabWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabs);

    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->setStyleSheet(QString::fromLatin1(kTabStyleSheet));

    buildProfiles();

    // ── Top-left corner: "+" new terminal button ─────────────────────────────
    auto *addBtn = new QToolButton(this);
    addBtn->setText(QStringLiteral("+"));
    addBtn->setAutoRaise(true);
    addBtn->setToolTip(tr("New Terminal (Ctrl+Shift+T)"));
    addBtn->setStyleSheet(QString::fromLatin1(kAddBtnStyleSheet));
    connect(addBtn, &QToolButton::clicked, this, [this]() { addTerminal(); });

    // Default Qt only allows one corner widget per corner; pack "+" and
    // the profile combo into a single host widget.
    auto *leftCorner = new QWidget(this);
    auto *cornerLay = new QHBoxLayout(leftCorner);
    cornerLay->setContentsMargins(2, 2, 2, 2);
    cornerLay->setSpacing(4);
    cornerLay->addWidget(addBtn);
    m_tabs->setCornerWidget(leftCorner, Qt::TopLeftCorner);

    // ── Top-right corner: profile dropdown ───────────────────────────────────
    m_profileCombo = new QComboBox(this);
    m_profileCombo->setToolTip(tr("Default shell for new terminals"));
    m_profileCombo->setStyleSheet(QString::fromLatin1(kProfileComboStyleSheet));
    for (const Profile &p : m_profiles)
        m_profileCombo->addItem(p.label, p.id);
    if (m_profileCombo->count() > 0)
        m_profileCombo->setCurrentIndex(0);

    auto *rightCorner = new QWidget(this);
    auto *rightLay = new QHBoxLayout(rightCorner);
    rightLay->setContentsMargins(2, 2, 6, 2);
    rightLay->setSpacing(4);
    rightLay->addWidget(m_profileCombo);
    m_tabs->setCornerWidget(rightCorner, Qt::TopRightCorner);

    // Double-click tab bar to rename
    m_tabs->tabBar()->installEventFilter(this);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *w = m_tabs->widget(index);
        m_tabs->removeTab(index);
        if (w)
            w->deleteLater();
        // If all tabs closed, create a fresh one
        if (m_tabs->count() == 0)
            addTerminal();
    });

    // Global panel shortcut: Ctrl+Shift+T → new terminal
    auto *newTabSc = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+T")), this);
    newTabSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(newTabSc, &QShortcut::activated, this, [this]() { addTerminal(); });

    // Start with one terminal using the default profile
    addTerminal();
}

// ── Profiles ──────────────────────────────────────────────────────────────────

void TerminalPanel::buildProfiles()
{
    m_profiles.clear();

#if defined(Q_OS_WIN)
    // cmd.exe — always available on Windows
    {
        Profile p;
        p.id      = QStringLiteral("cmd");
        p.label   = QStringLiteral("Command Prompt");
        p.program = QStringLiteral("cmd.exe");
        p.args    = QStringList{QStringLiteral("/K"), QStringLiteral("chcp 65001>nul")};
        m_profiles.append(p);
    }
    // Windows PowerShell (powershell.exe) — System32, almost always present
    {
        Profile p;
        p.id      = QStringLiteral("powershell");
        p.label   = QStringLiteral("Windows PowerShell");
        p.program = QStringLiteral("powershell.exe");
        p.args    = QStringList{QStringLiteral("-NoLogo")};
        m_profiles.append(p);
    }
    // PowerShell 7+ (pwsh.exe) — only if installed
    {
        const QString pwsh =
            QStandardPaths::findExecutable(QStringLiteral("pwsh"));
        if (!pwsh.isEmpty()) {
            Profile p;
            p.id      = QStringLiteral("pwsh");
            p.label   = QStringLiteral("PowerShell 7+");
            p.program = QStringLiteral("pwsh.exe");
            p.args    = QStringList{QStringLiteral("-NoLogo")};
            m_profiles.append(p);
        }
    }
    // Git Bash (bash.exe via Git for Windows) — only if found on PATH or
    // standard location
    {
        const QString bash =
            QStandardPaths::findExecutable(QStringLiteral("bash"));
        QString prog;
        if (!bash.isEmpty()) {
            prog = bash;
        } else if (fileExists(QStringLiteral("C:/Program Files/Git/bin/bash.exe"))) {
            prog = QStringLiteral("C:/Program Files/Git/bin/bash.exe");
        }
        if (!prog.isEmpty()) {
            Profile p;
            p.id      = QStringLiteral("bash");
            p.label   = QStringLiteral("Git Bash");
            p.program = prog;
            p.args    = QStringList{QStringLiteral("-i"), QStringLiteral("-l")};
            m_profiles.append(p);
        }
    }
    // WSL (wsl.exe) — System32, present on most Win10+ machines
    {
        const QString wsl =
            QStandardPaths::findExecutable(QStringLiteral("wsl"));
        if (!wsl.isEmpty()) {
            Profile p;
            p.id      = QStringLiteral("wsl");
            p.label   = QStringLiteral("WSL");
            p.program = QStringLiteral("wsl.exe");
            p.args    = QStringList{};
            m_profiles.append(p);
        }
    }
#else
    // POSIX: default shell first, then well-known alternatives if installed.
    const QString defaultShell =
        qEnvironmentVariable("SHELL", QStringLiteral("/bin/bash"));
    {
        Profile p;
        p.id      = QStringLiteral("default");
        p.label   = tr("Default Shell");
        p.program = defaultShell;
        p.args    = QStringList{QStringLiteral("-l")};
        m_profiles.append(p);
    }
    for (const auto &cand : {
             std::pair<const char *, const char *>("bash", "Bash"),
             std::pair<const char *, const char *>("zsh", "Zsh"),
             std::pair<const char *, const char *>("fish", "Fish"),
             std::pair<const char *, const char *>("sh", "sh"),
         }) {
        const QString exe =
            QStandardPaths::findExecutable(QString::fromLatin1(cand.first));
        if (exe.isEmpty()) continue;
        // Skip if it's the default we already added
        if (defaultShell.endsWith(QLatin1Char('/') + QString::fromLatin1(cand.first)))
            continue;
        Profile p;
        p.id      = QString::fromLatin1(cand.first);
        p.label   = QString::fromLatin1(cand.second);
        p.program = exe;
        p.args    = QStringList{QStringLiteral("-l")};
        m_profiles.append(p);
    }
#endif

    // Last-resort fallback so the panel is never empty.
    if (m_profiles.isEmpty()) {
        Profile p;
        p.id      = QStringLiteral("default");
        p.label   = tr("Default");
#if defined(Q_OS_WIN)
        p.program = QStringLiteral("cmd.exe");
        p.args    = QStringList{QStringLiteral("/K")};
#else
        p.program = QStringLiteral("/bin/sh");
#endif
        m_profiles.append(p);
    }
}

const TerminalPanel::Profile *TerminalPanel::findProfile(const QString &id) const
{
    for (const Profile &p : m_profiles) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

QString TerminalPanel::tabLabelFor(const Profile &profile) const
{
    return tr("%1 %2").arg(profile.label).arg(m_counter);
}

// ── Tabs ──────────────────────────────────────────────────────────────────────

TerminalWidget *TerminalPanel::addTerminal()
{
    QString id;
    if (m_profileCombo)
        id = m_profileCombo->currentData().toString();
    const Profile *p = findProfile(id);
    if (!p && !m_profiles.isEmpty())
        p = &m_profiles.front();
    static const Profile fallback{
        QStringLiteral("fallback"), QStringLiteral("Terminal"),
#if defined(Q_OS_WIN)
        QStringLiteral("cmd.exe"), QStringList{QStringLiteral("/K")}
#else
        QStringLiteral("/bin/sh"), QStringList{}
#endif
    };
    return addTerminal(p ? *p : fallback);
}

TerminalWidget *TerminalPanel::addTerminal(const Profile &profile)
{
    ++m_counter;
    auto *term = new TerminalWidget(this);

    // Launch the chosen profile explicitly via startProgram so the panel
    // always honours the user's selection, regardless of TerminalWidget's
    // internal default-shell logic.
    term->startProgram(profile.program, profile.args, m_workDir);

    const QString label = tabLabelFor(profile);
    const int idx = m_tabs->addTab(term, label);
    m_tabs->setTabToolTip(idx, profile.program);
    m_tabs->setCurrentIndex(idx);
    installShortcuts(term);
    connect(term, &TerminalWidget::closeRequested, this, [this, term]() {
        const int i = m_tabs->indexOf(term);
        if (i < 0) return;
        m_tabs->removeTab(i);
        term->deleteLater();
        if (m_tabs->count() == 0)
            addTerminal();
    });
    term->setFocus();
    return term;
}

void TerminalPanel::installShortcuts(TerminalWidget *term)
{
    if (!term) return;

    // Ctrl+Shift+= → zoom in.  Ctrl+= without shift is already wired inside
    // TerminalView, but the user may press shift; both should work.
    auto *zoomIn = new QShortcut(
        QKeySequence(QStringLiteral("Ctrl+Shift+=")), term);
    zoomIn->setContext(Qt::WidgetWithChildrenShortcut);
    connect(zoomIn, &QShortcut::activated, term, [term]() { term->zoomIn(); });

    // Some keyboard layouts deliver Ctrl+Shift+= as Ctrl+Shift++ — register both.
    auto *zoomInAlt = new QShortcut(
        QKeySequence(QStringLiteral("Ctrl+Shift++")), term);
    zoomInAlt->setContext(Qt::WidgetWithChildrenShortcut);
    connect(zoomInAlt, &QShortcut::activated, term, [term]() { term->zoomIn(); });

    auto *zoomOut = new QShortcut(
        QKeySequence(QStringLiteral("Ctrl+Shift+-")), term);
    zoomOut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(zoomOut, &QShortcut::activated, term, [term]() { term->zoomOut(); });

    auto *zoomReset = new QShortcut(
        QKeySequence(QStringLiteral("Ctrl+Shift+0")), term);
    zoomReset->setContext(Qt::WidgetWithChildrenShortcut);
    connect(zoomReset, &QShortcut::activated, term, [term]() { term->resetZoom(); });
}

TerminalWidget *TerminalPanel::currentTerminal() const
{
    return qobject_cast<TerminalWidget *>(m_tabs->currentWidget());
}

void TerminalPanel::setWorkingDirectory(const QString &dir)
{
    m_workDir = dir;
    // Update all existing terminals
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *term = qobject_cast<TerminalWidget *>(m_tabs->widget(i));
        if (term)
            term->setWorkingDirectory(dir);
    }
}

void TerminalPanel::sendCommand(const QString &cmd)
{
    auto *term = currentTerminal();
    if (term)
        term->sendCommand(cmd);
}

void TerminalPanel::sendInput(const QString &text)
{
    auto *term = currentTerminal();
    if (term)
        term->sendInput(text);
}

QString TerminalPanel::recentOutput(int maxLines) const
{
    auto *term = currentTerminal();
    return term ? term->recentOutput(maxLines) : QString();
}

QString TerminalPanel::selectedText() const
{
    auto *term = currentTerminal();
    return term ? term->selectedText() : QString();
}

bool TerminalPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_tabs->tabBar()) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto *me = static_cast<QMouseEvent *>(event);
            const int idx = m_tabs->tabBar()->tabAt(me->pos());
            if (idx >= 0) {
                renameTab(idx);
                return true;
            }
        } else if (event->type() == QEvent::ContextMenu) {
            auto *ce = static_cast<QContextMenuEvent *>(event);
            const int idx = m_tabs->tabBar()->tabAt(ce->pos());
            if (idx >= 0) {
                QMenu menu(this);
                QAction *renameAct = menu.addAction(tr("Rename..."));
                QAction *closeAct  = menu.addAction(tr("Close"));
                menu.addSeparator();
                QAction *closeOthersAct = menu.addAction(tr("Close Others"));
                QAction *closeAllAct    = menu.addAction(tr("Close All"));
                QAction *picked = menu.exec(ce->globalPos());
                if (picked == renameAct) {
                    renameTab(idx);
                } else if (picked == closeAct) {
                    QWidget *w = m_tabs->widget(idx);
                    m_tabs->removeTab(idx);
                    if (w) w->deleteLater();
                    if (m_tabs->count() == 0) addTerminal();
                } else if (picked == closeOthersAct) {
                    for (int i = m_tabs->count() - 1; i >= 0; --i) {
                        if (i == idx) continue;
                        QWidget *w = m_tabs->widget(i);
                        m_tabs->removeTab(i);
                        if (w) w->deleteLater();
                    }
                } else if (picked == closeAllAct) {
                    while (m_tabs->count() > 0) {
                        QWidget *w = m_tabs->widget(0);
                        m_tabs->removeTab(0);
                        if (w) w->deleteLater();
                    }
                    addTerminal();
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TerminalPanel::renameTab(int index)
{
    bool ok = false;
    const QString current = m_tabs->tabText(index);
    const QString name = QInputDialog::getText(
        this, tr("Rename Terminal"), tr("Name:"),
        QLineEdit::Normal, current, &ok);
    if (ok && !name.isEmpty())
        m_tabs->setTabText(index, name);
}

TerminalWidget *TerminalPanel::addSshTerminal(const QString &label,
                                              const QString &host, int port,
                                              const QString &user,
                                              const QString &privateKeyPath)
{
    auto *term = new TerminalWidget(this);

    // Build the ssh command and args
    const QString program = QStringLiteral("ssh");
    QStringList args;
    args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new")
         << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=10");

    if (port != 22)
        args << QStringLiteral("-p") << QString::number(port);

    if (!privateKeyPath.isEmpty())
        args << QStringLiteral("-i") << privateKeyPath;

    args << (user.isEmpty() ? host : user + QStringLiteral("@") + host);

    term->startProgram(program, args);

    const QString tabLabel = label.isEmpty()
        ? tr("SSH: %1").arg(host)
        : label;
    const int idx = m_tabs->addTab(term, tabLabel);
    m_tabs->setCurrentIndex(idx);
    installShortcuts(term);
    connect(term, &TerminalWidget::closeRequested, this, [this, term]() {
        const int i = m_tabs->indexOf(term);
        if (i < 0) return;
        m_tabs->removeTab(i);
        term->deleteLater();
        if (m_tabs->count() == 0)
            addTerminal();
    });
    term->setFocus();
    return term;
}

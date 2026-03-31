#include "cppworkspacepanel.h"

#include <QAction>
#include <QApplication>

#include <memory>
#include <QClipboard>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QScrollBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

// ── VS2022 dark theme color constants ─────────────────────────────────────────
namespace VS {
    constexpr auto BgPanel       = "#1e1e1e";
    constexpr auto BgHeader      = "#2d2d30";
    constexpr auto BgHover       = "#3e3e40";
    constexpr auto BgSelected    = "#094771";
    constexpr auto BgStrip       = "#252526";
    constexpr auto BgInfoBar     = "#1b1b1c";
    constexpr auto TxtNormal     = "#c8c8c8";
    constexpr auto TxtDim        = "#848484";
    constexpr auto TxtHeader     = "#ffffff";
    constexpr auto TxtSelected   = "#ffffff";
    constexpr auto AccentBlue    = "#007acc";
    constexpr auto Border        = "#3f3f46";
    // Status dot colors
    constexpr auto DotReady      = "#4ec9b0";   // green-teal
    constexpr auto DotError      = "#f14c4c";   // red
    constexpr auto DotWarning    = "#d7ba7d";   // yellow/amber
    constexpr auto DotInfo       = "#007acc";   // blue
    // file-type colors (match VS2022 token colors)
    constexpr auto TxtCpp        = "#9cdcfe";   // light blue — .cpp/.c
    constexpr auto TxtHeader_    = "#c586c0";   // purple     — .h/.hpp
    constexpr auto TxtCmake      = "#ce9178";   // orange     — CMakeLists.txt/.cmake
    constexpr auto TxtPython     = "#dcdcaa";   // yellow     — .py
    constexpr auto TxtJson       = "#d7ba7d";   // gold       — .json
}

// ── Helper: small flat VS-style tool button ────────────────────────────────────
static QToolButton *makeHeaderButton(const QString &text, const QString &tip,
                                     QWidget *parent)
{
    auto *btn = new QToolButton(parent);
    btn->setText(text);
    btn->setToolTip(tip);
    btn->setAutoRaise(true);
    btn->setFixedSize(22, 22);
    btn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  background: transparent;"
        "  color: %1;"
        "  border: none;"
        "  font-size: 11px;"
        "  font-family: 'Segoe UI', 'Consolas', monospace;"
        "  padding: 0;"
        "}"
        "QToolButton:hover { background: %2; border-radius: 2px; }"
        "QToolButton:pressed { background: %3; }"
    ).arg(VS::TxtNormal, VS::BgHover, VS::AccentBlue));
    return btn;
}

// ── Helper: info bar label ─────────────────────────────────────────────────────
static QLabel *makeInfoLabel(const QString &text, QWidget *parent)
{
    auto *lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: %1;"
        "  font-size: 10px;"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0 2px;"
        "}").arg(VS::TxtDim));
    return lbl;
}

// ── File helpers ───────────────────────────────────────────────────────────────
bool CppWorkspacePanel::isRelevantFile(const QString &name)
{
    const QString ext = QFileInfo(name).suffix().toLower();
    static const QSet<QString> kExts = {
        "cpp", "cxx", "cc", "c",
        "h",   "hpp", "hxx", "inl",
        "cmake", "txt",  // CMakeLists.txt
        "py", "json", "toml", "yaml", "yml",
        "rs", "go", "ts", "js",
        "md", "sh", "ps1",
    };
    if (kExts.contains(ext)) return true;
    if (name.toLower() == "cmakelists.txt") return true;
    return false;
}

bool CppWorkspacePanel::isExcludedDir(const QString &name)
{
    static const QSet<QString> kExcl = {
        ".git", ".cache", ".vs", ".vscode",
        "build", "build-llvm", "build-msvc",
        "node_modules", "__pycache__",
        "CMakeFiles", "_deps", "Testing",
        ".mypy_cache", ".pytest_cache",
    };
    return kExcl.contains(name);
}

QColor CppWorkspacePanel::fileColor(const QString &name)
{
    const QString n   = name.toLower();
    const QString ext = QFileInfo(n).suffix();
    if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "c")
        return QColor(VS::TxtCpp);
    if (ext == "h" || ext == "hpp" || ext == "hxx" || ext == "inl")
        return QColor(VS::TxtHeader_);
    if (ext == "cmake" || n == "cmakelists.txt")
        return QColor(VS::TxtCmake);
    if (ext == "py")
        return QColor(VS::TxtPython);
    if (ext == "json" || ext == "toml" || ext == "yaml" || ext == "yml")
        return QColor(VS::TxtJson);
    return QColor(VS::TxtNormal);
}

// ── Constructor ───────────────────────────────────────────────────────────────
CppWorkspacePanel::CppWorkspacePanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("cppWorkspacePanel"));
    setStyleSheet(QStringLiteral("QWidget#cppWorkspacePanel { background: %1; }")
                  .arg(VS::BgPanel));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header toolbar ────────────────────────────────────────────────────────
    auto *header = new QWidget(this);
    header->setFixedHeight(28);
    header->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-bottom: 1px solid %2; }")
        .arg(VS::BgHeader, VS::Border));

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 0, 4, 0);
    headerLayout->setSpacing(2);

    m_titleLabel = new QLabel(tr("C++ Workspace"), header);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; font-weight: 600;"
        "  background: transparent; border: none; }"
    ).arg(VS::TxtHeader));
    headerLayout->addWidget(m_titleLabel, 1);

    m_searchBtn = makeHeaderButton(QStringLiteral("\u2315"), tr("Filter (Ctrl+F)"), header);
    connect(m_searchBtn, &QToolButton::clicked, this, &CppWorkspacePanel::toggleFilterBar);
    headerLayout->addWidget(m_searchBtn);

    auto *collapseBtn = makeHeaderButton(QStringLiteral("\u229F"), tr("Collapse All"), header);
    connect(collapseBtn, &QToolButton::clicked, this, &CppWorkspacePanel::collapseAll);
    headerLayout->addWidget(collapseBtn);

    auto *refreshBtn = makeHeaderButton(QStringLiteral("\u27F3"), tr("Refresh"), header);
    connect(refreshBtn, &QToolButton::clicked, this, &CppWorkspacePanel::refreshTree);
    headerLayout->addWidget(refreshBtn);

    root->addWidget(header);

    // ── Filter bar (hidden by default) ────────────────────────────────────────
    m_filterBar = new QWidget(this);
    m_filterBar->setVisible(false);
    m_filterBar->setFixedHeight(28);
    m_filterBar->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-bottom: 1px solid %2; }")
        .arg(VS::BgHeader, VS::Border));

    auto *filterLayout = new QHBoxLayout(m_filterBar);
    filterLayout->setContentsMargins(8, 2, 4, 2);
    filterLayout->setSpacing(4);

    m_filterEdit = new QLineEdit(m_filterBar);
    m_filterEdit->setPlaceholderText(tr("Filter files..."));
    m_filterEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 2px;"
        "  padding: 2px 6px;"
        "  font-size: 11px;"
        "  selection-background-color: %4;"
        "}"
        "QLineEdit:focus { border-color: %5; }"
    ).arg(VS::BgPanel, VS::TxtNormal, VS::Border, VS::BgSelected, VS::AccentBlue));
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &CppWorkspacePanel::applyFilter);
    filterLayout->addWidget(m_filterEdit, 1);

    auto *clearFilter = makeHeaderButton(QStringLiteral("\u00D7"), tr("Clear Filter"), m_filterBar);
    connect(clearFilter, &QToolButton::clicked, this, [this]() {
        m_filterEdit->clear();
        setFilterVisible(false);
    });
    filterLayout->addWidget(clearFilter);

    root->addWidget(m_filterBar);

    // ── Project info bar ──────────────────────────────────────────────────────
    m_infoBar = new QWidget(this);
    m_infoBar->setFixedHeight(22);
    m_infoBar->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-bottom: 1px solid %2; }")
        .arg(VS::BgInfoBar, VS::Border));

    auto *infoLayout = new QHBoxLayout(m_infoBar);
    infoLayout->setContentsMargins(8, 0, 8, 0);
    infoLayout->setSpacing(8);

    m_infoRootLabel = makeInfoLabel(tr("No workspace"), m_infoBar);
    infoLayout->addWidget(m_infoRootLabel);

    auto *infoSep1 = new QLabel(QStringLiteral("\u2502"), m_infoBar);
    infoSep1->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; background: transparent;").arg(VS::Border));
    infoLayout->addWidget(infoSep1);

    m_infoBuildLabel = makeInfoLabel(QString(), m_infoBar);
    infoLayout->addWidget(m_infoBuildLabel);

    auto *infoSep2 = new QLabel(QStringLiteral("\u2502"), m_infoBar);
    infoSep2->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; background: transparent;").arg(VS::Border));
    infoLayout->addWidget(infoSep2);

    m_infoActiveLabel = makeInfoLabel(QString(), m_infoBar);
    infoLayout->addWidget(m_infoActiveLabel, 1);

    root->addWidget(m_infoBar);

    // ── Actions bar ───────────────────────────────────────────────────────────
    m_actionsBar = new QWidget(this);
    m_actionsBar->setFixedHeight(52); // 2 rows x 24px + margins
    m_actionsBar->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-bottom: 1px solid %2; }")
        .arg(VS::BgInfoBar, VS::Border));

    auto *actionsGrid = new QGridLayout(m_actionsBar);
    actionsGrid->setContentsMargins(4, 3, 4, 3);
    actionsGrid->setHorizontalSpacing(2);
    actionsGrid->setVerticalSpacing(2);

    // Row 1: Config, Build, Run, Debug, Clean
    // Row 2: Tests, Index
    const QList<std::tuple<QString, QString, int, int>> actionDefs = {
        {QStringLiteral("cpp.configureProject"), tr("\u2699 Config"), 0, 0},
        {QStringLiteral("cpp.buildProject"),     tr("\u2B1B Build"),  0, 1},
        {QStringLiteral("cpp.runProject"),        tr("\u25B6 Run"),    0, 2},
        {QStringLiteral("cpp.debugProject"),      tr("\u25CF Debug"),  0, 3},
        {QStringLiteral("cpp.cleanProject"),      tr("\u2715 Clean"),  0, 4},
        {QStringLiteral("cpp.runAllTests"),       tr("\u2B1C Tests"),  1, 0},
        {QStringLiteral("cpp.reindexWorkspace"),  tr("\u21BB Index"),  1, 1},
    };

    for (const auto &actionDef : actionDefs) {
        const QString commandId = std::get<0>(actionDef);
        const QString text      = std::get<1>(actionDef);
        const int     row       = std::get<2>(actionDef);
        const int     col       = std::get<3>(actionDef);

        auto *btn = new QToolButton(m_actionsBar);
        btn->setText(text);
        btn->setAutoRaise(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QToolButton {"
            "  background: transparent;"
            "  color: %1;"
            "  border: none;"
            "  font-size: 10px;"
            "  font-family: 'Segoe UI', 'Consolas', monospace;"
            "  padding: 1px 6px;"
            "  border-radius: 2px;"
            "}"
            "QToolButton:hover { background: %2; }"
            "QToolButton:pressed { background: %3; }"
            "QToolButton:disabled { color: #525252; }"
        ).arg(VS::TxtNormal, VS::BgHover, VS::AccentBlue));
        connect(btn, &QToolButton::clicked, this, [this, commandId]() {
            emit commandRequested(commandId);
        });
        m_actionButtonsMap.insert(commandId, btn);
        actionsGrid->addWidget(btn, row, col);
    }

    root->addWidget(m_actionsBar);

    // ── File tree ─────────────────────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setAnimated(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setAlternatingRowColors(false);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);
    m_tree->setIconSize(QSize(16, 16));
    m_tree->verticalScrollBar()->setStyleSheet(QStringLiteral(
        "QScrollBar:vertical { width: 8px; background: %1; margin: 0; }"
        "QScrollBar::handle:vertical { background: %2; min-height: 20px; border-radius: 4px; }"
        "QScrollBar::handle:vertical:hover { background: #5a5a5e; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(VS::BgPanel, VS::Border));

    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  outline: 0;"
        "  selection-background-color: transparent;"
        "  font-size: 12px;"
        "}"
        "QTreeWidget::item {"
        "  padding: 1px 0px;"
        "  min-height: 20px;"
        "  border: none;"
        "}"
        "QTreeWidget::item:hover {"
        "  background-color: %3;"
        "  color: %4;"
        "}"
        "QTreeWidget::item:selected {"
        "  background-color: %5;"
        "  color: %6;"
        "}"
        "QTreeWidget::item:selected:hover {"
        "  background-color: %5;"
        "}"
        "QTreeWidget::branch {"
        "  background: %1;"
        "}"
        "QTreeWidget::branch:has-children:!has-siblings:closed,"
        "QTreeWidget::branch:closed:has-children:has-siblings {"
        "  border-image: none;"
        "}"
        "QTreeWidget::branch:open:has-children:!has-siblings,"
        "QTreeWidget::branch:open:has-children:has-siblings {"
        "  border-image: none;"
        "}"
    ).arg(VS::BgPanel,    // 1 bg
         VS::TxtNormal,   // 2 text
         VS::BgHover,     // 3 hover bg
         VS::TxtHeader,   // 4 hover text
         VS::BgSelected,  // 5 selected bg
         VS::TxtSelected  // 6 selected text
    ));

    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &CppWorkspacePanel::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &CppWorkspacePanel::onItemContextMenu);

    root->addWidget(m_tree, 1);

    // ── CMake Targets section ─────────────────────────────────────────────────
    // Collapsible list of build targets (executables) discovered by the build system.
    m_targetsHeader = new QWidget(this);
    m_targetsHeader->setFixedHeight(24);
    m_targetsHeader->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-top: 1px solid %2; }")
        .arg(VS::BgHeader, VS::Border));

    auto *targetsHeaderLayout = new QHBoxLayout(m_targetsHeader);
    targetsHeaderLayout->setContentsMargins(8, 0, 4, 0);
    targetsHeaderLayout->setSpacing(2);

    auto *targetsTitle = new QLabel(tr("TARGETS"), m_targetsHeader);
    targetsTitle->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; font-weight: 600;"
        "  background: transparent; border: none; }"
    ).arg(VS::TxtDim));
    targetsHeaderLayout->addWidget(targetsTitle, 1);

    m_targetsToggleBtn = makeHeaderButton(QStringLiteral("\u25B4"), tr("Toggle Targets"), m_targetsHeader);
    connect(m_targetsToggleBtn, &QToolButton::clicked, this, &CppWorkspacePanel::toggleTargets);
    targetsHeaderLayout->addWidget(m_targetsToggleBtn);

    root->addWidget(m_targetsHeader);

    // Targets body: scrollable list of target names
    m_targetsBody = new QWidget(this);
    m_targetsBody->setMaximumHeight(120);
    m_targetsBody->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; }").arg(VS::BgStrip));

    auto *targetsBodyLayout = new QVBoxLayout(m_targetsBody);
    targetsBodyLayout->setContentsMargins(0, 0, 0, 0);
    targetsBodyLayout->setSpacing(0);

    m_targetsList = new QListWidget(m_targetsBody);
    m_targetsList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_targetsList->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  outline: 0;"
        "  font-size: 11px;"
        "  font-family: 'Consolas', 'Segoe UI', monospace;"
        "}"
        "QListWidget::item {"
        "  padding: 2px 8px;"
        "  min-height: 20px;"
        "  border: none;"
        "}"
        "QListWidget::item:hover { background: %3; }"
        "QListWidget::item:selected { background: %4; color: %5; }"
        "QListWidget::item:selected:hover { background: %4; }"
    ).arg(VS::BgStrip, VS::TxtNormal, VS::BgHover, VS::BgSelected, VS::TxtSelected));
    m_targetsList->verticalScrollBar()->setStyleSheet(QStringLiteral(
        "QScrollBar:vertical { width: 6px; background: %1; margin: 0; }"
        "QScrollBar::handle:vertical { background: %2; min-height: 12px; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(VS::BgStrip, VS::Border));
    connect(m_targetsList, &QListWidget::itemDoubleClicked,
            this, &CppWorkspacePanel::onTargetDoubleClicked);
    connect(m_targetsList, &QListWidget::customContextMenuRequested,
            this, &CppWorkspacePanel::onTargetContextMenu);

    // "No targets" placeholder
    auto *noTargetsItem = new QListWidgetItem(
        QStringLiteral("  \u2014  Configure to discover targets"), m_targetsList);
    noTargetsItem->setFlags(Qt::NoItemFlags);
    noTargetsItem->setForeground(QColor(VS::TxtDim));

    targetsBodyLayout->addWidget(m_targetsList);
    root->addWidget(m_targetsBody);

    // ── Status cards section ──────────────────────────────────────────────────
    // Header bar with "Status" label and expand/collapse toggle
    m_statusCardsHeader = new QWidget(this);
    m_statusCardsHeader->setFixedHeight(24);
    m_statusCardsHeader->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-top: 1px solid %2; }")
        .arg(VS::BgHeader, VS::Border));

    auto *statusHeaderLayout = new QHBoxLayout(m_statusCardsHeader);
    statusHeaderLayout->setContentsMargins(8, 0, 4, 0);
    statusHeaderLayout->setSpacing(2);

    auto *statusTitle = new QLabel(tr("Status"), m_statusCardsHeader);
    statusTitle->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; font-weight: 600;"
        "  background: transparent; border: none; text-transform: uppercase; }"
    ).arg(VS::TxtDim));
    statusHeaderLayout->addWidget(statusTitle, 1);

    m_statusToggleBtn = makeHeaderButton(QStringLiteral("\u25B4"), tr("Toggle Status"), m_statusCardsHeader);
    connect(m_statusToggleBtn, &QToolButton::clicked, this, &CppWorkspacePanel::toggleStatusCards);
    statusHeaderLayout->addWidget(m_statusToggleBtn);

    root->addWidget(m_statusCardsHeader);

    // Status cards body: 2-column grid with 5 status indicators
    m_statusCardsBody = new QWidget(this);
    m_statusCardsBody->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; }")
        .arg(VS::BgStrip));

    auto *cardsGrid = new QGridLayout(m_statusCardsBody);
    cardsGrid->setContentsMargins(8, 4, 8, 4);
    cardsGrid->setHorizontalSpacing(12);
    cardsGrid->setVerticalSpacing(2);

    const QList<QPair<QString, QString>> statusDefs = {
        {QStringLiteral("language"), tr("clangd")},
        {QStringLiteral("build"),    tr("Build")},
        {QStringLiteral("debug"),    tr("Debug")},
        {QStringLiteral("tests"),    tr("Tests")},
        {QStringLiteral("search"),   tr("Search")},
    };

    int row = 0, col = 0;
    for (const auto &[id, name] : statusDefs) {
        auto *lbl = new QLabel(
            QStringLiteral("<span style='color:%1'>\u25CF</span> %2: \u2014")
                .arg(VS::DotInfo, name),
            m_statusCardsBody);
        lbl->setTextFormat(Qt::RichText);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; background: transparent;"
            "  border: none; padding: 1px 0; }")
            .arg(VS::TxtDim));
        m_statusLabels.insert(id, lbl);
        cardsGrid->addWidget(lbl, row, col);

        ++col;
        if (col > 1) { col = 0; ++row; }
    }
    // stretch the last column
    cardsGrid->setColumnStretch(0, 1);
    cardsGrid->setColumnStretch(1, 1);

    root->addWidget(m_statusCardsBody);
}

// ── Public API ────────────────────────────────────────────────────────────────

void CppWorkspacePanel::setWorkspaceSummary(const QString &workspaceRoot,
                                            const QString &activeFile,
                                            const QString &buildDirectory,
                                            const QString &compileCommandsPath)
{
    m_activeFile = activeFile;
    m_buildDirectory = buildDirectory;
    m_compileCommandsPath = compileCommandsPath;

    if (workspaceRoot != m_workspaceRoot) {
        m_workspaceRoot = workspaceRoot;
        buildTree();

        const QString name = workspaceRoot.isEmpty()
            ? tr("(no workspace)")
            : QFileInfo(workspaceRoot).fileName();
        m_titleLabel->setText(tr("C++ Workspace \u2014 %1").arg(name));
    }

    updateProjectInfoBar();
}

void CppWorkspacePanel::setCardStatus(const QString &cardId,
                                      const QString &title,
                                      const QString &detail)
{
    auto *lbl = m_statusLabels.value(cardId, nullptr);
    if (!lbl) return;

    // Pick a dot color based on title heuristic
    const char *dotColor = VS::DotInfo;
    const QString tl = title.toLower();
    if (tl.contains("ready") || tl.contains("ok") || tl.contains("success")
        || tl.contains("succeeded") || tl.contains("completed") || tl.contains("discovered"))
        dotColor = VS::DotReady;
    else if (tl.contains("error") || tl.contains("fail") || tl.contains("degraded")
             || tl.contains("unavailable") || tl.contains("attention"))
        dotColor = VS::DotError;
    else if (tl.contains("build") || tl.contains("index") || tl.contains("start")
             || tl.contains("restart") || tl.contains("running") || tl.contains("launch"))
        dotColor = VS::DotWarning;

    const QString label = cardId == QLatin1String("language") ? tr("clangd")
                        : cardId == QLatin1String("build")    ? tr("Build")
                        : cardId == QLatin1String("debug")    ? tr("Debug")
                        : cardId == QLatin1String("tests")    ? tr("Tests")
                        : cardId == QLatin1String("search")   ? tr("Search")
                        : cardId;

    lbl->setText(QStringLiteral("<span style='color:%1'>\u25CF</span> %2: %3")
                     .arg(QLatin1String(dotColor), label, title));
    lbl->setToolTip(detail);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; background: transparent;"
        "  border: none; padding: 1px 0; }")
        .arg(VS::TxtNormal));
}

void CppWorkspacePanel::setActionEnabled(const QString &actionId, bool enabled)
{
    if (enabled)
        m_disabledActions.remove(actionId);
    else
        m_disabledActions.insert(actionId);

    if (auto *btn = m_actionButtonsMap.value(actionId, nullptr))
        btn->setEnabled(enabled);
}

void CppWorkspacePanel::setTargets(const QStringList &targets, const QString &activeTarget)
{
    m_activeTarget = activeTarget;
    updateProjectInfoBar();
    m_targetsList->clear();

    if (targets.isEmpty()) {
        auto *placeholder = new QListWidgetItem(
            QStringLiteral("  \u2014  Configure to discover targets"), m_targetsList);
        placeholder->setFlags(Qt::NoItemFlags);
        placeholder->setForeground(QColor(VS::TxtDim));
        m_targetsBody->setMaximumHeight(28);
        return;
    }

    // Fit the list to content (up to 5 rows visible, then scroll)
    const int rowH = 22;
    const int maxH = rowH * 5 + 4;
    m_targetsBody->setMaximumHeight(qMin(static_cast<int>(targets.size()) * rowH + 4, maxH));

    for (const QString &t : targets) {
        const bool isActive = (t == activeTarget);
        // Show active target with a run indicator and bold font
        const QString displayText = isActive
            ? QStringLiteral("\u25B6 %1").arg(t)   // ▶ active
            : QStringLiteral("    %1").arg(t);

        auto *item = new QListWidgetItem(displayText, m_targetsList);
        item->setData(Qt::UserRole, t);  // store plain name
        if (isActive) {
            QFont f = m_targetsList->font();
            f.setBold(true);
            item->setFont(f);
            item->setForeground(QColor(VS::AccentBlue));
            m_targetsList->setCurrentItem(item);
        } else {
            item->setForeground(QColor(VS::TxtNormal));
        }
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void CppWorkspacePanel::toggleFilterBar()
{
    setFilterVisible(!m_filterBar->isVisible());
}

void CppWorkspacePanel::setFilterVisible(bool visible)
{
    m_filterBar->setVisible(visible);
    if (visible) {
        m_filterEdit->setFocus();
        m_filterEdit->selectAll();
    } else {
        m_filterEdit->clear();
        // Show all items
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
            filterItems(m_tree->topLevelItem(i), {});
    }
}

void CppWorkspacePanel::applyFilter(const QString &text)
{
    const QString lower = text.trimmed().toLower();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        filterItems(m_tree->topLevelItem(i), lower);
}

void CppWorkspacePanel::filterItems(QTreeWidgetItem *item, const QString &lower)
{
    if (!item) return;
    bool anyChildVisible = false;
    for (int i = 0; i < item->childCount(); ++i) {
        filterItems(item->child(i), lower);
        if (!item->child(i)->isHidden())
            anyChildVisible = true;
    }

    if (item->childCount() > 0) {
        // Folder: visible if any child visible or filter empty
        item->setHidden(!lower.isEmpty() && !anyChildVisible);
        if (anyChildVisible && !lower.isEmpty())
            item->setExpanded(true);
    } else {
        // Leaf (file)
        const bool match = lower.isEmpty()
                        || item->text(0).toLower().contains(lower);
        item->setHidden(!match);
    }
}

void CppWorkspacePanel::collapseAll()
{
    m_tree->collapseAll();
    // Keep root expanded
    if (m_tree->topLevelItemCount() > 0)
        m_tree->topLevelItem(0)->setExpanded(true);
}

void CppWorkspacePanel::refreshTree()
{
    buildTree();
}

void CppWorkspacePanel::toggleStatusCards()
{
    m_statusExpanded = !m_statusExpanded;
    m_statusCardsBody->setVisible(m_statusExpanded);
    m_statusToggleBtn->setText(m_statusExpanded ? QStringLiteral("\u25B4") : QStringLiteral("\u25BE"));
}

void CppWorkspacePanel::toggleTargets()
{
    m_targetsExpanded = !m_targetsExpanded;
    m_targetsBody->setVisible(m_targetsExpanded);
    m_targetsToggleBtn->setText(m_targetsExpanded ? QStringLiteral("\u25B4") : QStringLiteral("\u25BE"));
}

void CppWorkspacePanel::onTargetDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;
    const QString targetName = item->data(Qt::UserRole).toString();
    if (targetName.isEmpty()) return;
    // Refresh the list to update the active indicator, then notify plugin
    const QStringList all = [this]() {
        QStringList names;
        for (int i = 0; i < m_targetsList->count(); ++i) {
            const QString n = m_targetsList->item(i)->data(Qt::UserRole).toString();
            if (!n.isEmpty()) names << n;
        }
        return names;
    }();
    setTargets(all, targetName);
    emit activeTargetChanged(targetName);
}

void CppWorkspacePanel::onTargetContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_targetsList->itemAt(pos);
    const QString targetName = item ? item->data(Qt::UserRole).toString() : QString();
    if (targetName.isEmpty()) return;

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  padding: 2px;"
        "}"
        "QMenu::item { padding: 4px 24px 4px 16px; }"
        "QMenu::item:selected { background: %4; color: #fff; }"
        "QMenu::separator { background: %3; height: 1px; margin: 2px 8px; }"
    ).arg(VS::BgHeader, VS::TxtNormal, VS::Border, VS::BgSelected));

    menu.addAction(tr("Set as Active Target"), this, [this, targetName]() {
        const QStringList all = [this]() {
            QStringList names;
            for (int i = 0; i < m_targetsList->count(); ++i) {
                const QString n = m_targetsList->item(i)->data(Qt::UserRole).toString();
                if (!n.isEmpty()) names << n;
            }
            return names;
        }();
        setTargets(all, targetName);
        emit activeTargetChanged(targetName);
    });

    menu.addAction(tr("Build This Target"), this, [this, targetName]() {
        emit commandRequested(QStringLiteral("cpp.buildTarget:") + targetName);
    });

    menu.addSeparator();
    menu.addAction(tr("Run This Target"), this, [this, targetName]() {
        emit commandRequested(QStringLiteral("cpp.runTarget:") + targetName);
    });
    menu.addAction(tr("Debug This Target"), this, [this, targetName]() {
        emit commandRequested(QStringLiteral("cpp.debugTarget:") + targetName);
    });

    menu.exec(m_targetsList->viewport()->mapToGlobal(pos));
}

void CppWorkspacePanel::updateProjectInfoBar()
{
    if (!m_infoBar) return;

    if (m_workspaceRoot.isEmpty()) {
        m_infoRootLabel->setText(tr("No workspace"));
        m_infoBuildLabel->setText(QString());
        m_infoActiveLabel->setText(QString());
        return;
    }

    m_infoRootLabel->setText(QFileInfo(m_workspaceRoot).fileName());
    m_infoRootLabel->setToolTip(QDir::toNativeSeparators(m_workspaceRoot));

    if (m_buildDirectory.isEmpty()) {
        m_infoBuildLabel->setText(tr("No build dir"));
    } else {
        const QString buildName = QFileInfo(m_buildDirectory).fileName();
        m_infoBuildLabel->setText(buildName);
        m_infoBuildLabel->setToolTip(QDir::toNativeSeparators(m_buildDirectory));
    }

    // Prefer active target over active file in the info bar
    if (!m_activeTarget.isEmpty()) {
        m_infoActiveLabel->setText(QStringLiteral("\u25B6 %1").arg(m_activeTarget));
        m_infoActiveLabel->setToolTip(tr("Active run target: %1").arg(m_activeTarget));
    } else if (!m_activeFile.isEmpty()) {
        m_infoActiveLabel->setText(QFileInfo(m_activeFile).fileName());
        m_infoActiveLabel->setToolTip(QDir::toNativeSeparators(m_activeFile));
    } else {
        m_infoActiveLabel->setText(QString());
    }
}

void CppWorkspacePanel::onItemDoubleClicked(QTreeWidgetItem *item, int /*col*/)
{
    if (!item) return;
    const QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;
    if (QFileInfo(path).isFile()) {
        emit fileOpenRequested(path);
        emit commandRequested(QStringLiteral("cpp.openFile:") + path);
    }
}

void CppWorkspacePanel::onItemContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item) return;

    const QString path  = item->data(0, Qt::UserRole).toString();
    const bool isFile   = !path.isEmpty() && QFileInfo(path).isFile();
    const bool isDir    = !path.isEmpty() && QFileInfo(path).isDir();

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  padding: 2px;"
        "}"
        "QMenu::item { padding: 4px 24px 4px 16px; }"
        "QMenu::item:selected { background: %4; color: #fff; }"
        "QMenu::separator { background: %3; height: 1px; margin: 2px 8px; }"
    ).arg(VS::BgHeader, VS::TxtNormal, VS::Border, VS::BgSelected));

    if (isFile) {
        menu.addAction(tr("Open"), this, [this, path]() {
            emit fileOpenRequested(path);
        });

        const QString ext = QFileInfo(path).suffix().toLower();
        if (ext == "cpp" || ext == "h" || ext == "cxx" || ext == "hpp" || ext == "c") {
            menu.addSeparator();
            auto *def = menu.addAction(tr("Go to Definition  F12"), this, [this]() {
                emit commandRequested(QStringLiteral("cpp.goToDefinition"));
            });
            auto *refs = menu.addAction(tr("Find All References  Shift+F12"), this, [this]() {
                emit commandRequested(QStringLiteral("cpp.findReferences"));
            });
            auto *sw = menu.addAction(tr("Switch Header/Source  Alt+O"), this, [this]() {
                emit commandRequested(QStringLiteral("cpp.switchHeaderSource"));
            });
            if (m_disabledActions.contains("cpp.goToDefinition"))  def->setEnabled(false);
            if (m_disabledActions.contains("cpp.findReferences"))  refs->setEnabled(false);
            if (m_disabledActions.contains("cpp.switchHeaderSource")) sw->setEnabled(false);
        }

        menu.addSeparator();
        menu.addAction(tr("Copy Full Path"), this, [path]() {
            QApplication::clipboard()->setText(path);
        });
        menu.addAction(tr("Copy Relative Path"), this, [this, path]() {
            const QString rel = QDir(m_workspaceRoot).relativeFilePath(path);
            QApplication::clipboard()->setText(rel);
        });
    }

    if (isDir) {
        menu.addAction(tr("Find in Files…"), this, [this]() {
            emit commandRequested(QStringLiteral("cpp.focusSearch"));
        });
        menu.addAction(tr("Open Terminal Here"), this, [this]() {
            emit buildTerminalRequested();
        });
        menu.addSeparator();
        menu.addAction(tr("Refresh"), this, [this]() { buildTree(); });
    }

    if (!menu.isEmpty())
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

// ── Tree building ─────────────────────────────────────────────────────────────

void CppWorkspacePanel::buildTree()
{
    m_tree->clear();
    if (m_workspaceRoot.isEmpty()) return;

    QFileIconProvider iconProvider;

    // Root item — "Solution 'ProjectName'"
    const QString solutionName = QFileInfo(m_workspaceRoot).fileName();
    auto *root = new QTreeWidgetItem(m_tree);
    root->setText(0, QStringLiteral("Solution \u2018%1\u2019").arg(solutionName));
    root->setIcon(0, iconProvider.icon(QFileIconProvider::Folder));
    root->setForeground(0, QColor(VS::TxtHeader));
    root->setData(0, Qt::UserRole, m_workspaceRoot);
    root->setFont(0, [&] {
        QFont f = m_tree->font();
        f.setWeight(QFont::DemiBold);
        return f;
    }());

    populateDirectory(root, m_workspaceRoot, 0);
    root->setExpanded(true);

    // Auto-expand the first level
    for (int i = 0; i < root->childCount(); ++i)
        root->child(i)->setExpanded(false);
}

void CppWorkspacePanel::populateDirectory(QTreeWidgetItem *parent,
                                           const QString   &dirPath,
                                           int              depth)
{
    if (depth > 6) return;

    QFileIconProvider iconProvider;
    QDir dir(dirPath);
    dir.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    const auto entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot);

    for (const QFileInfo &fi : entries) {
        const QString name = fi.fileName();

        if (fi.isDir()) {
            if (isExcludedDir(name)) continue;

            auto *item = new QTreeWidgetItem(parent);
            item->setText(0, name);
            item->setIcon(0, iconProvider.icon(QFileIconProvider::Folder));
            item->setForeground(0, QColor(VS::TxtNormal));
            item->setData(0, Qt::UserRole, fi.absoluteFilePath());
            populateDirectory(item, fi.absoluteFilePath(), depth + 1);

            // Prune empty directory items
            if (item->childCount() == 0) {
                std::unique_ptr<QTreeWidgetItem> removed(parent->takeChild(parent->indexOfChild(item)));
            }
        } else {
            if (!isRelevantFile(name)) continue;

            auto *item = new QTreeWidgetItem(parent);
            item->setText(0, name);
            item->setIcon(0, iconProvider.icon(fi));
            item->setForeground(0, fileColor(name));
            item->setData(0, Qt::UserRole, fi.absoluteFilePath());
            item->setToolTip(0, fi.absoluteFilePath());
        }
    }
}

#include "quickopendialog.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QtConcurrent/QtConcurrent>
#include <QVBoxLayout>

namespace {

constexpr int kMaxFiles      = 5000;
constexpr int kMaxResults    = 50;

// Folder names to skip during workspace scan.
bool isSkippedFolder(const QString &name)
{
    static const QStringList kSkipped = {
        QStringLiteral(".git"),
        QStringLiteral(".hg"),
        QStringLiteral(".svn"),
        QStringLiteral("node_modules"),
        QStringLiteral(".cache"),
        QStringLiteral("__pycache__"),
        QStringLiteral(".venv"),
        QStringLiteral("venv"),
        QStringLiteral(".idea"),
        QStringLiteral(".vs"),
        QStringLiteral(".vscode"),
        QStringLiteral("CMakeFiles"),
    };
    if (kSkipped.contains(name, Qt::CaseInsensitive))
        return true;
    // Build dirs: build, build-llvm, build-msvc, etc.
    if (name.compare(QStringLiteral("build"), Qt::CaseInsensitive) == 0)
        return true;
    if (name.startsWith(QStringLiteral("build-"), Qt::CaseInsensitive))
        return true;
    if (name.startsWith(QStringLiteral("cmake-build"), Qt::CaseInsensitive))
        return true;
    return false;
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

QuickOpenDialog::QuickOpenDialog(const QString &workspaceRoot, QWidget *parent)
    : QDialog(parent), m_root(workspaceRoot)
{
    setWindowTitle(tr("Quick Open"));
    setModal(true);
    setMinimumSize(560, 420);

    // VS dark theme styling — consistent with GoToLineDialog.
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #252526; }"
        "QLabel  { color: #d4d4d4; }"
        "QLabel#statusLabel { color: #9cdcfe; font-size: 11px; }"
        "QLineEdit {"
        "  background-color: #1e1e1e; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; padding: 6px 8px; border-radius: 2px;"
        "  selection-background-color: #264f78;"
        "}"
        "QLineEdit:focus { border: 1px solid #007acc; }"
        "QListWidget {"
        "  background-color: #1e1e1e; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; outline: 0;"
        "}"
        "QListWidget::item { padding: 4px 8px; border: 0; }"
        "QListWidget::item:hover    { background-color: #2a2d2e; }"
        "QListWidget::item:selected { background-color: #094771; color: #ffffff; }"
        "QPushButton {"
        "  background-color: #2d2d30; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; padding: 5px 16px; border-radius: 2px;"
        "}"
        "QPushButton:hover    { background-color: #3e3e42; }"
        "QPushButton:default  { border: 1px solid #007acc; }"
        "QPushButton:disabled { color: #6d6d6d; border-color: #2d2d30; }"
    ));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(8);

    m_queryEdit = new QLineEdit(this);
    m_queryEdit->setPlaceholderText(tr("Search files by name..."));
    m_queryEdit->setClearButtonEnabled(true);
    root->addWidget(m_queryEdit);

    m_results = new QListWidget(this);
    m_results->setUniformItemSizes(true);
    m_results->setAlternatingRowColors(false);
    root->addWidget(m_results, 1);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("statusLabel"));
    m_status->setText(tr("Scanning workspace..."));
    root->addWidget(m_status);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_openBtn   = new QPushButton(tr("Open"),   this);
    m_openBtn->setDefault(true);
    m_openBtn->setAutoDefault(true);
    m_openBtn->setEnabled(false);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_openBtn);
    root->addLayout(btnRow);

    // Connections.
    connect(m_queryEdit, &QLineEdit::textChanged,
            this, &QuickOpenDialog::onQueryChanged);
    connect(m_queryEdit, &QLineEdit::returnPressed,
            this, &QuickOpenDialog::onAcceptCurrent);
    connect(m_results, &QListWidget::itemActivated,
            this, &QuickOpenDialog::onItemActivated);
    connect(m_results, &QListWidget::itemDoubleClicked,
            this, &QuickOpenDialog::onItemActivated);
    connect(m_results, &QListWidget::currentRowChanged,
            this, [this](int row) { m_openBtn->setEnabled(row >= 0); });
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_openBtn,   &QPushButton::clicked, this, &QuickOpenDialog::onAcceptCurrent);

    // Kick off background scan so the UI never blocks.
    m_watcher = new QFutureWatcher<QStringList>(this);
    connect(m_watcher, &QFutureWatcher<QStringList>::finished,
            this, &QuickOpenDialog::onScanFinished);
    const QString rootCopy = m_root;
    m_watcher->setFuture(QtConcurrent::run([rootCopy]() {
        return scanWorkspace(rootCopy, kMaxFiles);
    }));

    // Forward Up/Down/PageUp/PageDown from the line edit to the result list,
    // so the user can keep typing while navigating results.
    m_queryEdit->installEventFilter(this);

    m_queryEdit->setFocus();
}

bool QuickOpenDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_queryEdit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        switch (ke->key()) {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            QCoreApplication::sendEvent(m_results, ke);
            return true;
        default:
            break;
        }
    }
    return QDialog::eventFilter(watched, event);
}

QuickOpenDialog::~QuickOpenDialog()
{
    if (m_watcher) {
        // Keep ownership chain simple — let the worker finish; QFutureWatcher
        // cancels its signal hook through Qt parent ownership.
        m_watcher->disconnect(this);
    }
}

// ── Workspace scan (worker thread) ───────────────────────────────────────────

QStringList QuickOpenDialog::scanWorkspace(const QString &root, int cap)
{
    QStringList out;
    if (root.isEmpty() || !QFileInfo(root).isDir())
        return out;

    out.reserve(qMin(cap, 4096));

    QDirIterator it(
        root,
        QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        // QDirIterator with Subdirectories will recurse on its own; we only
        // need to filter results. To skip whole subtrees we would need a
        // manual recursion — but for typical workspaces, filename filtering
        // here is sufficient and bounded by the 5000-file cap.
        if (fi.isDir())
            continue;

        // Skip files inside ignored directories.
        const QString rel = QDir(root).relativeFilePath(fi.absoluteFilePath());
        bool skip = false;
        for (const QString &part : rel.split(QLatin1Char('/'))) {
            if (isSkippedFolder(part)) {
                skip = true;
                break;
            }
        }
        if (skip)
            continue;

        out << fi.absoluteFilePath();
        if (out.size() >= cap)
            break;
    }
    return out;
}

void QuickOpenDialog::onScanFinished()
{
    m_scanning = false;
    m_allFiles = m_watcher ? m_watcher->result() : QStringList();
    m_status->setText(tr("Indexed %1 file(s)%2")
                          .arg(m_allFiles.size())
                          .arg(m_allFiles.size() >= kMaxFiles
                                   ? tr(" (capped)") : QString()));
    rebuildResults();
}

// ── Fuzzy matching ───────────────────────────────────────────────────────────

int QuickOpenDialog::fuzzyScore(const QString &query,
                                const QString &path,
                                const QString &basename) const
{
    if (query.isEmpty())
        return 1; // every entry passes when query is empty

    const QString q  = query.toLower();
    const QString lp = path.toLower();
    const QString lb = basename.toLower();

    // 1. Exact basename prefix match → highest score.
    if (lb.startsWith(q))
        return 10000 + (q.length() * 100) - (lb.length() - q.length());

    // 2. Walk the path; every char of query must appear in order. Track the
    //    longest run of consecutive matches as a quality signal.
    int score = 0;
    int qi = 0;
    int consec = 0;
    int bestConsec = 0;
    int basenameStart = path.length() - basename.length();

    for (int i = 0; i < lp.length() && qi < q.length(); ++i) {
        if (lp[i] == q[qi]) {
            // Bonus for matching inside basename portion.
            if (i >= basenameStart)
                score += 8;
            else
                score += 2;
            consec += 1;
            bestConsec = qMax(bestConsec, consec);
            ++qi;
        } else {
            consec = 0;
        }
    }
    if (qi < q.length())
        return -1; // not all query chars matched in order

    score += bestConsec * 12;
    score -= (lp.length() / 8); // mild penalty for very long paths
    if (score < 1) score = 1;
    return score;
}

// ── Result list rebuild ──────────────────────────────────────────────────────

void QuickOpenDialog::rebuildResults()
{
    m_results->clear();
    if (m_scanning) {
        m_openBtn->setEnabled(false);
        return;
    }

    const QString query = m_queryEdit->text().trimmed();
    QDir rootDir(m_root);

    struct Scored {
        int score;
        QString path;
        QString basename;
        QString relPath;
    };

    QVector<Scored> matches;
    matches.reserve(qMin(m_allFiles.size(), 1024));

    for (const QString &p : m_allFiles) {
        const QString basename = QFileInfo(p).fileName();
        const int s = fuzzyScore(query, p, basename);
        if (s < 0) continue;
        Scored entry{s, p, basename, rootDir.relativeFilePath(p)};
        matches.push_back(std::move(entry));
    }

    std::sort(matches.begin(), matches.end(),
              [](const Scored &a, const Scored &b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.basename.compare(b.basename, Qt::CaseInsensitive) < 0;
              });

    const int shown = qMin<int>(matches.size(), kMaxResults);
    for (int i = 0; i < shown; ++i) {
        const Scored &m = matches[i];
        // Render as: "basename" + dim relative dir on a second line-ish suffix.
        // We use a simple two-tone display: "<basename>  —  <relDir>".
        const QString relDir = QFileInfo(m.relPath).path();
        const QString display = relDir.isEmpty() || relDir == QStringLiteral(".")
            ? m.basename
            : QStringLiteral("%1   —   %2").arg(m.basename, relDir);

        auto *item = new QListWidgetItem(display, m_results);
        item->setData(Qt::UserRole, m.path);
        item->setToolTip(QDir::toNativeSeparators(m.path));
        // Dim the path portion via a per-item foreground hint: keep the entry
        // simple — full row uses default fg; QSS already styles selection.
    }

    if (m_results->count() > 0)
        m_results->setCurrentRow(0);

    m_openBtn->setEnabled(m_results->currentRow() >= 0);

    if (query.isEmpty()) {
        m_status->setText(tr("Indexed %1 file(s) — type to filter")
                              .arg(m_allFiles.size()));
    } else {
        m_status->setText(tr("%1 of %2 match(es)")
                              .arg(matches.size())
                              .arg(m_allFiles.size()));
    }
}

// ── Slot wiring ──────────────────────────────────────────────────────────────

void QuickOpenDialog::onQueryChanged(const QString &)
{
    rebuildResults();
}

void QuickOpenDialog::onItemActivated(QListWidgetItem *item)
{
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;
    emit fileSelected(path);
    accept();
}

void QuickOpenDialog::onAcceptCurrent()
{
    QListWidgetItem *cur = m_results->currentItem();
    if (!cur) return;
    onItemActivated(cur);
}


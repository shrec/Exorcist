#include "mergeeditor.h"
#include "gitservice.h"

#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

MergeEditor::MergeEditor(GitService *git, QWidget *parent)
    : QWidget(parent)
    , m_git(git)
{
    auto *mainLayout = std::make_unique<QVBoxLayout>().release();
    setLayout(mainLayout);

    // ── Toolbar ─────────────────────────────────────────────────────────
    auto *toolRow = std::make_unique<QHBoxLayout>().release();
    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    m_statusLabel = new QLabel(this);
    toolRow->addWidget(m_refreshBtn);
    toolRow->addWidget(m_statusLabel);
    toolRow->addStretch();
    mainLayout->addLayout(toolRow);

    // ── Main splitter: file list | merge view ───────────────────────────
    auto *mainSplit = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(mainSplit, 1);

    // File list
    m_fileList = new QListWidget(this);
    mainSplit->addWidget(m_fileList);

    // Merge view
    auto *mergeWidget = new QWidget(this);
    auto *mergeLayout = std::make_unique<QVBoxLayout>().release();
    mergeLayout->setContentsMargins(0, 0, 0, 0);
    mergeWidget->setLayout(mergeLayout);

    m_conflictTitle = new QLabel(this);
    m_conflictTitle->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px;"));
    mergeLayout->addWidget(m_conflictTitle);

    // ── 3-pane splitter: ours | theirs ──────────────────────────────────
    auto *topSplit = new QSplitter(Qt::Horizontal, this);
    QFont monoFont(QStringLiteral("Consolas"), 10);
    monoFont.setStyleHint(QFont::Monospace);

    auto *oursBox = new QWidget(this);
    auto *oursLay = std::make_unique<QVBoxLayout>().release();
    oursLay->setContentsMargins(0, 0, 0, 0);
    oursLay->addWidget(new QLabel(tr("Ours (HEAD)"), this));
    m_oursPane = new QPlainTextEdit(this);
    m_oursPane->setReadOnly(true);
    m_oursPane->setFont(monoFont);
    m_oursPane->setLineWrapMode(QPlainTextEdit::NoWrap);
    oursLay->addWidget(m_oursPane);
    oursBox->setLayout(oursLay);
    topSplit->addWidget(oursBox);

    auto *theirsBox = new QWidget(this);
    auto *theirsLay = std::make_unique<QVBoxLayout>().release();
    theirsLay->setContentsMargins(0, 0, 0, 0);
    theirsLay->addWidget(new QLabel(tr("Theirs (incoming)"), this));
    m_theirsPane = new QPlainTextEdit(this);
    m_theirsPane->setReadOnly(true);
    m_theirsPane->setFont(monoFont);
    m_theirsPane->setLineWrapMode(QPlainTextEdit::NoWrap);
    theirsLay->addWidget(m_theirsPane);
    theirsBox->setLayout(theirsLay);
    topSplit->addWidget(theirsBox);

    mergeLayout->addWidget(topSplit, 1);

    // ── Resolution buttons ──────────────────────────────────────────────
    auto *btnRow = std::make_unique<QHBoxLayout>().release();
    m_acceptOursBtn = new QPushButton(tr("Accept Ours"), this);
    m_acceptTheirsBtn = new QPushButton(tr("Accept Theirs"), this);
    m_acceptBothBtn = new QPushButton(tr("Accept Both"), this);
    btnRow->addWidget(m_acceptOursBtn);
    btnRow->addWidget(m_acceptTheirsBtn);
    btnRow->addWidget(m_acceptBothBtn);
    btnRow->addStretch();
    mergeLayout->addLayout(btnRow);

    // ── Result pane ─────────────────────────────────────────────────────
    mergeLayout->addWidget(new QLabel(tr("Result (editable):"), this));
    m_resultPane = new QPlainTextEdit(this);
    m_resultPane->setFont(monoFont);
    m_resultPane->setLineWrapMode(QPlainTextEdit::NoWrap);
    mergeLayout->addWidget(m_resultPane, 1);

    // ── Save button ─────────────────────────────────────────────────────
    auto *saveRow = std::make_unique<QHBoxLayout>().release();
    auto *saveBtn = new QPushButton(tr("Save Resolution"), this);
    saveBtn->setStyleSheet(QStringLiteral("background: #2a6;"));
    saveRow->addStretch();
    saveRow->addWidget(saveBtn);
    mergeLayout->addLayout(saveRow);

    mainSplit->addWidget(mergeWidget);
    mainSplit->setSizes({200, 600});

    // ── Connections ─────────────────────────────────────────────────────
    connect(m_refreshBtn, &QPushButton::clicked, this, &MergeEditor::refresh);
    connect(m_fileList, &QListWidget::currentRowChanged, this, &MergeEditor::onFileSelected);
    connect(m_acceptOursBtn, &QPushButton::clicked, this, &MergeEditor::acceptOurs);
    connect(m_acceptTheirsBtn, &QPushButton::clicked, this, &MergeEditor::acceptTheirs);
    connect(m_acceptBothBtn, &QPushButton::clicked, this, &MergeEditor::acceptBoth);
    connect(saveBtn, &QPushButton::clicked, this, &MergeEditor::acceptCustom);
}

void MergeEditor::refresh()
{
    m_fileList->clear();
    m_conflictPaths.clear();
    m_currentFile.clear();

    const QStringList conflicts = m_git->conflictFiles();
    for (const QString &path : conflicts) {
        m_fileList->addItem(QStringLiteral("⚡ ") + path);
        const QString gitRoot = m_git->workingDirectory();
        m_conflictPaths.append(gitRoot + QLatin1Char('/') + path);
    }

    m_statusLabel->setText(tr("%1 conflicted files").arg(conflicts.size()));

    if (m_conflictPaths.isEmpty()) {
        m_oursPane->clear();
        m_theirsPane->clear();
        m_resultPane->clear();
        m_conflictTitle->setText(tr("No merge conflicts"));
        emit allResolved();
    } else {
        m_fileList->setCurrentRow(0);
    }
}

void MergeEditor::onFileSelected(int row)
{
    if (row < 0 || row >= m_conflictPaths.size())
        return;
    loadConflict(m_conflictPaths.at(row));
}

void MergeEditor::loadConflict(const QString &absPath)
{
    m_currentFile = absPath;
    const QString relPath = QDir(m_git->workingDirectory()).relativeFilePath(absPath);
    m_conflictTitle->setText(tr("Conflict: %1").arg(relPath));

    // Read the raw file content with conflict markers
    QFile file(absPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_oursPane->setPlainText(tr("Cannot read file"));
        m_theirsPane->clear();
        m_resultPane->clear();
        return;
    }

    const QString content = QString::fromUtf8(file.readAll());

    // Parse conflict markers to extract ours/theirs sections
    QString oursText;
    QString theirsText;
    QString cleanText; // non-conflict parts

    enum Section { Normal, Ours, Theirs };
    Section section = Normal;

    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("<<<<<<<"))) {
            section = Ours;
            continue;
        }
        if (line.startsWith(QStringLiteral("======="))) {
            section = Theirs;
            continue;
        }
        if (line.startsWith(QStringLiteral(">>>>>>>"))) {
            section = Normal;
            oursText += QStringLiteral("─── conflict end ───\n");
            theirsText += QStringLiteral("─── conflict end ───\n");
            continue;
        }

        switch (section) {
        case Normal:
            oursText += line + QLatin1Char('\n');
            theirsText += line + QLatin1Char('\n');
            cleanText += line + QLatin1Char('\n');
            break;
        case Ours:
            oursText += line + QLatin1Char('\n');
            cleanText += line + QLatin1Char('\n'); // default: keep ours
            break;
        case Theirs:
            theirsText += line + QLatin1Char('\n');
            break;
        }
    }

    m_oursPane->setPlainText(oursText);
    m_theirsPane->setPlainText(theirsText);
    m_resultPane->setPlainText(cleanText); // default to ours
}

void MergeEditor::acceptOurs()
{
    if (m_currentFile.isEmpty()) return;

    QFile file(m_currentFile);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Strip conflict markers, keep "ours" content
    QString result;
    enum Section { Normal, Ours, Theirs };
    Section section = Normal;
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("<<<<<<<"))) { section = Ours; continue; }
        if (line.startsWith(QStringLiteral("======="))) { section = Theirs; continue; }
        if (line.startsWith(QStringLiteral(">>>>>>>"))) { section = Normal; continue; }
        if (section != Theirs)
            result += line + QLatin1Char('\n');
    }

    m_resultPane->setPlainText(result);
}

void MergeEditor::acceptTheirs()
{
    if (m_currentFile.isEmpty()) return;

    QFile file(m_currentFile);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    QString result;
    enum Section { Normal, Ours, Theirs };
    Section section = Normal;
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("<<<<<<<"))) { section = Ours; continue; }
        if (line.startsWith(QStringLiteral("======="))) { section = Theirs; continue; }
        if (line.startsWith(QStringLiteral(">>>>>>>"))) { section = Normal; continue; }
        if (section != Ours)
            result += line + QLatin1Char('\n');
    }

    m_resultPane->setPlainText(result);
}

void MergeEditor::acceptBoth()
{
    if (m_currentFile.isEmpty()) return;

    QFile file(m_currentFile);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    QString result;
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("<<<<<<< ")) ||
            line.startsWith(QStringLiteral("=======")) ||
            line.startsWith(QStringLiteral(">>>>>>> ")))
            continue;
        result += line + QLatin1Char('\n');
    }

    m_resultPane->setPlainText(result);
}

void MergeEditor::acceptCustom()
{
    writeResolution(m_resultPane->toPlainText());
}

void MergeEditor::writeResolution(const QString &content)
{
    if (m_currentFile.isEmpty()) return;

    // Write resolved content to file
    QFile file(m_currentFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(content.toUtf8());
    file.close();

    // Stage the resolved file
    m_git->stageFile(m_currentFile);

    emit fileResolved(m_currentFile);

    // Remove from list and load next conflict
    const int idx = m_conflictPaths.indexOf(m_currentFile);
    if (idx >= 0) {
        m_conflictPaths.removeAt(idx);
        m_fileList->takeItem(idx);
    }

    m_statusLabel->setText(tr("%1 conflicted files").arg(m_conflictPaths.size()));

    if (m_conflictPaths.isEmpty()) {
        m_currentFile.clear();
        m_oursPane->clear();
        m_theirsPane->clear();
        m_resultPane->clear();
        m_conflictTitle->setText(tr("All conflicts resolved!"));
        emit allResolved();
    } else {
        m_fileList->setCurrentRow(0);
    }
}

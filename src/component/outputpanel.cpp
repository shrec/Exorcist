#include "outputpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextBlock>
#include <QFont>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QTimer>
#include <QFrame>

OutputPanel::OutputPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // VS2022-style toolbar
    auto *toolbarWidget = new QWidget(this);
    toolbarWidget->setFixedHeight(30);
    toolbarWidget->setStyleSheet(QStringLiteral(
        "QWidget { background: #2d2d30; }"
        "QComboBox { background: #3f3f46; color: #d4d4d4; border: 1px solid #555558;"
        "  padding: 1px 6px; font-size: 12px; }"
        "QComboBox::drop-down { border: none; width: 16px; }"
        "QComboBox QAbstractItemView { background: #252526; color: #d4d4d4;"
        "  selection-background-color: #094771; border: 1px solid #555558; }"
        "QPushButton { color: #d4d4d4; background: transparent; border: 1px solid transparent;"
        "  padding: 2px 10px; font-size: 12px; }"
        "QPushButton:hover { border-color: #555558; background: #3e3e42; }"
        "QPushButton:pressed { background: #094771; }"
        "QPushButton:disabled { color: #555558; }"
        "QToolButton { color: #d4d4d4; background: transparent; border: 1px solid transparent;"
        "  padding: 2px 8px; font-size: 11px; }"
        "QToolButton:hover { border-color: #555558; background: #3e3e42; }"
        "QToolButton:checked { background: #094771; border-color: #1f6feb; color: #ffffff; }"
        "QLineEdit { background: #1e1e1e; color: #d4d4d4; border: 1px solid #555558;"
        "  padding: 1px 6px; font-size: 12px; selection-background-color: #094771; }"
        "QLineEdit:focus { border-color: #1f6feb; }"
        "QLabel { color: #858585; font-size: 11px; background: transparent; }"
    ));
    auto *toolbar = new QHBoxLayout(toolbarWidget);
    toolbar->setContentsMargins(6, 2, 6, 2);
    toolbar->setSpacing(4);

    m_profileCombo = new QComboBox;
    m_profileCombo->setMinimumWidth(180);
    m_profileCombo->setEditable(true);
    toolbar->addWidget(m_profileCombo);

    m_runBtn = new QPushButton(tr("▶ Run"));
    m_stopBtn = new QPushButton(tr("■ Stop"));
    m_clearBtn = new QPushButton(tr("✕ Clear"));
    m_stopBtn->setEnabled(false);

    toolbar->addWidget(m_runBtn);
    toolbar->addWidget(m_stopBtn);
    toolbar->addWidget(m_clearBtn);

    m_elapsedLabel = new QLabel;
    toolbar->addWidget(m_elapsedLabel);

    toolbar->addStretch();

    // ── Category filter buttons ────────────────────────────────────────────
    auto *sep1 = new QFrame;
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet(QStringLiteral("color: #3e3e42;"));
    toolbar->addWidget(sep1);

    auto makeFilterBtn = [&](const QString &label, const QString &tip) {
        auto *b = new QToolButton(toolbarWidget);
        b->setText(label);
        b->setCheckable(true);
        b->setChecked(true);
        b->setToolTip(tip);
        b->setFocusPolicy(Qt::NoFocus);
        toolbar->addWidget(b);
        return b;
    };

    m_filterBuildBtn = makeFilterBtn(tr("Build"),
        tr("Show build/compiler output (lines without bracketed prefix)"));
    m_filterDebugBtn = makeFilterBtn(tr("Debug"),
        tr("Show [DEBUG] flow logs"));
    m_filterGdbBtn   = makeFilterBtn(tr("GDB"),
        tr("Show [GDB] and [CMD] lines"));
    m_filterAllBtn   = makeFilterBtn(tr("All"),
        tr("Show every category"));
    m_filterAllBtn->setCheckable(false); // acts as a one-shot reset

    m_filterEdit = new QLineEdit(toolbarWidget);
    m_filterEdit->setPlaceholderText(tr("Filter..."));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setFixedWidth(180);
    toolbar->addWidget(m_filterEdit);

    vbox->addWidget(toolbarWidget);

    // Output text
    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setUndoRedoEnabled(false);
    m_output->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_output->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: #1a1a1a; color: #ccc; border: none; }"));
    m_output->setWordWrapMode(QTextOption::NoWrap);
    vbox->addWidget(m_output);

    // Elapsed-time ticker
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(500);
    connect(m_elapsedTimer, &QTimer::timeout, this, &OutputPanel::onElapsedTick);

    // Signals
    connect(m_runBtn, &QPushButton::clicked, this, [this] {
        const int idx = m_profileCombo->currentIndex();
        if (idx >= 0 && idx < m_tasks.size()) {
            runTask(m_tasks.at(idx));
        } else {
            // Free-form command typed into editable combo
            const QString cmd = m_profileCombo->currentText();
            if (!cmd.isEmpty())
                runCommand(cmd);
        }
    });
    connect(m_stopBtn, &QPushButton::clicked, this, [this] {
        if (m_process && m_process->state() != QProcess::NotRunning)
            m_process->kill();
    });
    connect(m_clearBtn, &QPushButton::clicked, this, &OutputPanel::clear);

    // Click on output navigates to error
    connect(m_output, &QPlainTextEdit::cursorPositionChanged, this, &OutputPanel::onTextClicked);

    // ── Filter UI signals ──────────────────────────────────────────────────
    connect(m_filterBuildBtn, &QToolButton::toggled, this, [this](bool on) {
        setCategoryFilter(CategoryBuild, on);
    });
    connect(m_filterDebugBtn, &QToolButton::toggled, this, [this](bool on) {
        setCategoryFilter(CategoryDebug, on);
    });
    connect(m_filterGdbBtn, &QToolButton::toggled, this, [this](bool on) {
        setCategoryFilter(CategoryGdb, on);
    });
    connect(m_filterAllBtn, &QToolButton::clicked, this, [this] {
        // Re-enable everything and clear the search box.
        m_suspendRender = true;
        m_filterBuildBtn->setChecked(true);
        m_filterDebugBtn->setChecked(true);
        m_filterGdbBtn->setChecked(true);
        m_filterEdit->clear();
        m_suspendRender = false;
        m_showBuild = m_showDebug = m_showGdb = true;
        m_searchFilter.clear();
        rebuildVisibleText();
    });
    connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString &t) {
        setSearchFilter(t);
    });

    setupMatchers();
}

void OutputPanel::setWorkingDirectory(const QString &dir)
{
    m_workDir = dir;
    // Auto-detect tasks if combo is empty
    if (m_tasks.isEmpty())
        autoDetectTasks();
}

// ── Task profile management ──────────────────────────────────────────────────

void OutputPanel::setTaskProfiles(const QList<TaskProfile> &profiles)
{
    m_tasks = profiles;
    populateCombo();
}

void OutputPanel::loadTasksFromJson(const QString &jsonPath)
{
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;

    const QJsonArray arr = doc.object().value(QLatin1String("tasks")).toArray();
    QList<TaskProfile> tasks;
    for (const QJsonValue &v : arr) {
        const QJsonObject obj = v.toObject();
        TaskProfile t;
        t.label   = obj.value(QLatin1String("label")).toString();
        t.command = obj.value(QLatin1String("command")).toString();
        const QJsonArray argsArr = obj.value(QLatin1String("args")).toArray();
        for (const QJsonValue &a : argsArr)
            t.args << a.toString();
        t.workDir   = obj.value(QLatin1String("workDir")).toString();
        t.isDefault = obj.value(QLatin1String("isDefault")).toBool();
        const QJsonObject envObj = obj.value(QLatin1String("env")).toObject();
        for (auto it = envObj.begin(); it != envObj.end(); ++it)
            t.env.insert(it.key(), it.value().toString());
        if (!t.command.isEmpty())
            tasks.append(t);
    }
    setTaskProfiles(tasks);
}

void OutputPanel::saveTasksToJson(const QString &jsonPath) const
{
    QJsonArray arr;
    for (const TaskProfile &t : m_tasks) {
        QJsonObject obj;
        obj[QLatin1String("label")]     = t.label;
        obj[QLatin1String("command")]   = t.command;
        if (!t.args.isEmpty())
            obj[QLatin1String("args")]  = QJsonArray::fromStringList(t.args);
        if (!t.workDir.isEmpty())
            obj[QLatin1String("workDir")] = t.workDir;
        if (t.isDefault)
            obj[QLatin1String("isDefault")] = true;
        if (!t.env.isEmpty()) {
            QJsonObject envObj;
            for (auto it = t.env.constBegin(); it != t.env.constEnd(); ++it)
                envObj[it.key()] = it.value();
            obj[QLatin1String("env")] = envObj;
        }
        arr.append(obj);
    }
    QJsonObject root;
    root[QLatin1String("tasks")] = arr;

    QFile f(jsonPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void OutputPanel::autoDetectTasks()
{
    if (m_workDir.isEmpty()) return;
    const QDir dir(m_workDir);

    QList<TaskProfile> detected;

    // CMake
    if (dir.exists(QStringLiteral("CMakeLists.txt"))) {
        detected.append({tr("CMake: Build"), QStringLiteral("cmake"),
                         {QStringLiteral("--build"), QStringLiteral("${workspaceFolder}/build")},
                         {}, {}, true});
        detected.append({tr("CMake: Configure"), QStringLiteral("cmake"),
                         {QStringLiteral("-S"), QStringLiteral("${workspaceFolder}"),
                          QStringLiteral("-B"), QStringLiteral("${workspaceFolder}/build")},
                         {}, {}, false});
    }
    // Cargo
    if (dir.exists(QStringLiteral("Cargo.toml"))) {
        detected.append({tr("Cargo: Build"), QStringLiteral("cargo"),
                         {QStringLiteral("build")}, {}, {}, true});
        detected.append({tr("Cargo: Test"), QStringLiteral("cargo"),
                         {QStringLiteral("test")}, {}, {}, false});
    }
    // Makefile
    if (dir.exists(QStringLiteral("Makefile")) || dir.exists(QStringLiteral("makefile"))) {
        detected.append({tr("Make: Build"), QStringLiteral("make"),
                         {}, {}, {}, true});
        detected.append({tr("Make: Clean"), QStringLiteral("make"),
                         {QStringLiteral("clean")}, {}, {}, false});
    }
    // Node.js
    if (dir.exists(QStringLiteral("package.json"))) {
        detected.append({tr("npm: Build"), QStringLiteral("npm"),
                         {QStringLiteral("run"), QStringLiteral("build")}, {}, {}, true});
        detected.append({tr("npm: Test"), QStringLiteral("npm"),
                         {QStringLiteral("test")}, {}, {}, false});
    }
    // Python
    if (dir.exists(QStringLiteral("setup.py")) || dir.exists(QStringLiteral("pyproject.toml"))) {
        detected.append({tr("Python: Test"), QStringLiteral("python"),
                         {QStringLiteral("-m"), QStringLiteral("pytest")}, {}, {}, false});
    }
    // .NET
    if (dir.exists(QStringLiteral("*.sln")) || !dir.entryList({QStringLiteral("*.csproj")}).isEmpty()) {
        detected.append({tr("dotnet: Build"), QStringLiteral("dotnet"),
                         {QStringLiteral("build")}, {}, {}, true});
    }

    if (!detected.isEmpty())
        setTaskProfiles(detected);
}

void OutputPanel::populateCombo()
{
    m_profileCombo->clear();
    int defaultIdx = 0;
    for (int i = 0; i < m_tasks.size(); ++i) {
        const TaskProfile &t = m_tasks.at(i);
        const QString display = t.label.isEmpty()
            ? t.command + QLatin1Char(' ') + t.args.join(QLatin1Char(' '))
            : t.label;
        m_profileCombo->addItem(display);
        if (t.isDefault)
            defaultIdx = i;
    }
    if (!m_tasks.isEmpty())
        m_profileCombo->setCurrentIndex(defaultIdx);
}

QString OutputPanel::substituteVars(const QString &input) const
{
    QString result = input;
    result.replace(QLatin1String("${workspaceFolder}"), m_workDir);
    result.replace(QLatin1String("${workspaceRoot}"),   m_workDir);

    // ${env:VAR} — read from process environment
    static const QRegularExpression envRx(QStringLiteral(R"(\$\{env:(\w+)\})"));
    auto it = envRx.globalMatch(result);
    while (it.hasNext()) {
        auto match = it.next();
        const QString val = qEnvironmentVariable(match.captured(1).toUtf8().constData());
        result.replace(match.captured(0), val);
    }
    return result;
}

void OutputPanel::runTask(const TaskProfile &task)
{
    // Substitute variables in command and args
    QString cmd = substituteVars(task.command);
    QStringList args;
    for (const QString &a : task.args)
        args << substituteVars(a);
    QString workDir = task.workDir.isEmpty() ? m_workDir : substituteVars(task.workDir);

    clear();

    if (m_process) {
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(workDir);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    // Set extra env vars
    if (!task.env.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (auto it = task.env.constBegin(); it != task.env.constEnd(); ++it)
            env.insert(it.key(), substituteVars(it.value()));
        m_process->setProcessEnvironment(env);
    }

    connect(m_process, &QProcess::readyReadStandardOutput, this, &OutputPanel::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError,  this, &OutputPanel::onReadyReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OutputPanel::onProcessFinished);

    const QString display = task.label.isEmpty()
        ? cmd + QLatin1Char(' ') + args.join(QLatin1Char(' '))
        : task.label;
    appendText(tr("▶ Running: %1\n").arg(display), QColor(100, 180, 255));

    m_runBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_elapsed.start();
    m_elapsedTimer->start();
    m_elapsedLabel->setText(tr("0.0 s"));

    m_process->start(cmd, args);
}

void OutputPanel::runCommand(const QString &cmd, const QStringList &args)
{
    clear();

    if (m_process) {
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(m_workDir);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &OutputPanel::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError,  this, &OutputPanel::onReadyReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OutputPanel::onProcessFinished);

    appendText(tr("▶ Running: %1\n").arg(cmd), QColor(100, 180, 255));

    m_runBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_elapsed.start();
    m_elapsedTimer->start();
    m_elapsedLabel->setText(tr("0.0 s"));

    // Parse command line
    QStringList parts;
    if (args.isEmpty()) {
        // Simple shell-style splitting
        bool inQuote = false;
        QString cur;
        for (const QChar &ch : cmd) {
            if (ch == QLatin1Char('"')) {
                inQuote = !inQuote;
            } else if (ch == QLatin1Char(' ') && !inQuote) {
                if (!cur.isEmpty()) { parts << cur; cur.clear(); }
            } else {
                cur += ch;
            }
        }
        if (!cur.isEmpty()) parts << cur;
    } else {
        parts << cmd;
        parts.append(args);
    }

    if (parts.isEmpty()) return;

    const QString program = parts.takeFirst();
    m_process->start(program, parts);
}

void OutputPanel::runRemoteCommand(const QString &host, int port,
                                   const QString &user, const QString &privateKeyPath,
                                   const QString &remoteCmd,
                                   const QString &remoteWorkDir)
{
    clear();

    if (m_process) {
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &OutputPanel::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError,  this, &OutputPanel::onReadyReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OutputPanel::onProcessFinished);

    QStringList args;
    args << QStringLiteral("-o") << QStringLiteral("BatchMode=yes")
         << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new")
         << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=10");

    if (port != 22 && port > 0)
        args << QStringLiteral("-p") << QString::number(port);
    if (!privateKeyPath.isEmpty())
        args << QStringLiteral("-i") << privateKeyPath;

    args << QStringLiteral("%1@%2").arg(user, host);

    // Build the remote command with optional cd
    QString fullCmd = remoteCmd;
    if (!remoteWorkDir.isEmpty())
        fullCmd = QStringLiteral("cd %1 && %2").arg(remoteWorkDir, remoteCmd);
    args << fullCmd;

    const QString display = QStringLiteral("[%1@%2] %3").arg(user, host, fullCmd);
    appendText(tr("\u25B6 Remote: %1\n").arg(display), QColor(100, 180, 255));

    m_runBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_elapsed.start();
    m_elapsedTimer->start();
    m_elapsedLabel->setText(tr("0.0 s"));

    m_process->start(QStringLiteral("ssh"), args);
}

void OutputPanel::appendText(const QString &text, const QColor &color)
{
    // Split into individual lines so each has its own category record.
    // We preserve the trailing newline behavior by treating "\n" as a
    // line terminator. An empty trailing element (after a final '\n')
    // becomes a blank line entry.
    if (text.isEmpty())
        return;

    QStringList parts = text.split(QLatin1Char('\n'));
    // If the input ends with '\n', split produces a trailing empty element.
    // We keep it so the visual blank line is recorded and rendered.
    const bool endsWithNl = text.endsWith(QLatin1Char('\n'));
    if (endsWithNl)
        parts.removeLast(); // we'll append "\n" explicitly per stored line

    for (const QString &p : parts) {
        Line ln;
        ln.text       = p;
        ln.color      = color;
        ln.category   = categorize(p);
        ln.problemIdx = -1;
        m_lines.append(ln);
        appendVisibleLine(m_lines.last());
    }
    // If the original text was just "\n" or ended with one we still want the
    // physical newline reflected — appendVisibleLine already appends '\n'
    // per stored Line, so a trailing empty Line above (when input is "\n"
    // or "abc\n") is unnecessary; the final newline is implicit.
    Q_UNUSED(endsWithNl);
}

void OutputPanel::appendBuildLine(const QString &line, bool isError)
{
    const int probsBefore = m_problems.size();
    parseLine(line);
    Line ln;
    ln.text       = line;
    ln.color      = isError ? QColor(Qt::red) : QColor();
    ln.category   = categorize(line);
    ln.problemIdx = (m_problems.size() > probsBefore) ? probsBefore : -1;
    m_lines.append(ln);
    appendVisibleLine(m_lines.last());
}

void OutputPanel::clear()
{
    m_output->clear();
    m_problems.clear();
    m_lineToProb.clear();
    m_lines.clear();
    emit problemsChanged();
}

// ── Filter API ─────────────────────────────────────────────────────────────

OutputPanel::LineCategory OutputPanel::categorize(const QString &line)
{
    // Inspect the first non-whitespace token: a bracketed prefix decides the
    // bucket. [GDB] / [CMD] -> GDB, [DEBUG] -> Debug, anything else -> Build.
    int i = 0;
    while (i < line.size() && line.at(i).isSpace())
        ++i;
    if (i >= line.size() || line.at(i) != QLatin1Char('['))
        return CategoryBuild;

    // Find closing bracket, bounded so we don't scan a whole 4KB stderr line.
    const int maxScan = qMin(line.size(), i + 16);
    int j = i + 1;
    while (j < maxScan && line.at(j) != QLatin1Char(']'))
        ++j;
    if (j >= maxScan || line.at(j) != QLatin1Char(']'))
        return CategoryBuild;

    const QStringView tag = QStringView(line).mid(i + 1, j - i - 1);
    if (tag.compare(QLatin1String("GDB"), Qt::CaseInsensitive) == 0)
        return CategoryGdb;
    if (tag.compare(QLatin1String("CMD"), Qt::CaseInsensitive) == 0)
        return CategoryGdb;
    if (tag.compare(QLatin1String("DEBUG"), Qt::CaseInsensitive) == 0)
        return CategoryDebug;
    return CategoryBuild;
}

bool OutputPanel::isLineVisible(const Line &ln) const
{
    switch (ln.category) {
    case CategoryBuild: if (!m_showBuild) return false; break;
    case CategoryDebug: if (!m_showDebug) return false; break;
    case CategoryGdb:   if (!m_showGdb)   return false; break;
    }
    if (!m_searchFilter.isEmpty()
        && !ln.text.contains(m_searchFilter, Qt::CaseInsensitive))
        return false;
    return true;
}

void OutputPanel::appendVisibleLine(const Line &ln)
{
    if (m_suspendRender)
        return;
    if (!isLineVisible(ln))
        return;

    QTextCharFormat fmt;
    if (ln.color.isValid())
        fmt.setForeground(ln.color);

    QTextCursor cur = m_output->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(ln.text + QLatin1Char('\n'), fmt);
    m_output->setTextCursor(cur);
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void OutputPanel::rebuildVisibleText()
{
    if (m_suspendRender)
        return;

    m_output->clear();
    QTextCursor cur = m_output->textCursor();
    cur.movePosition(QTextCursor::End);

    for (const Line &ln : std::as_const(m_lines)) {
        if (!isLineVisible(ln))
            continue;
        QTextCharFormat fmt;
        if (ln.color.isValid())
            fmt.setForeground(ln.color);
        cur.insertText(ln.text + QLatin1Char('\n'), fmt);
    }
    m_output->setTextCursor(cur);
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void OutputPanel::setCategoryFilter(LineCategory cat, bool visible)
{
    switch (cat) {
    case CategoryBuild: m_showBuild = visible; break;
    case CategoryDebug: m_showDebug = visible; break;
    case CategoryGdb:   m_showGdb   = visible; break;
    }
    rebuildVisibleText();
}

bool OutputPanel::categoryFilter(LineCategory cat) const
{
    switch (cat) {
    case CategoryBuild: return m_showBuild;
    case CategoryDebug: return m_showDebug;
    case CategoryGdb:   return m_showGdb;
    }
    return true;
}

void OutputPanel::setSearchFilter(const QString &needle)
{
    if (m_searchFilter == needle)
        return;
    m_searchFilter = needle;
    rebuildVisibleText();
}

void OutputPanel::onReadyReadStdout()
{
    const QString text = QString::fromUtf8(m_process->readAllStandardOutput());
    const auto lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const int probsBefore = m_problems.size();
        parseLine(line);
        Line ln;
        ln.text       = line;
        ln.color      = QColor();
        ln.category   = categorize(line);
        ln.problemIdx = (m_problems.size() > probsBefore) ? probsBefore : -1;
        m_lines.append(ln);
        appendVisibleLine(m_lines.last());
    }
}

void OutputPanel::onReadyReadStderr()
{
    const QString text = QString::fromUtf8(m_process->readAllStandardError());
    const auto lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const int probsBefore = m_problems.size();
        parseLine(line);
        // Color stderr lines: errors in red, warnings in yellow
        QColor c(220, 100, 100);
        if (line.contains(QLatin1String("warning"), Qt::CaseInsensitive))
            c = QColor(220, 200, 80);
        Line ln;
        ln.text       = line;
        ln.color      = c;
        ln.category   = categorize(line);
        ln.problemIdx = (m_problems.size() > probsBefore) ? probsBefore : -1;
        m_lines.append(ln);
        appendVisibleLine(m_lines.last());
    }
}

void OutputPanel::onProcessFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    m_elapsedTimer->stop();
    const double secs = m_elapsed.elapsed() / 1000.0;
    m_elapsedLabel->setText(tr("%1 s").arg(secs, 0, 'f', 1));

    const QColor c = (exitCode == 0) ? QColor(80, 200, 80) : QColor(220, 80, 80);
    appendText(tr("\n✔ Process exited with code %1  (%2 problem(s), %3 s)\n")
               .arg(exitCode).arg(m_problems.size()).arg(secs, 0, 'f', 1), c);
    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    emit buildFinished(exitCode);
}

void OutputPanel::onTextClicked()
{
    const int blockNum = m_output->textCursor().blockNumber();
    // Walk visible lines until we hit blockNum'th one, then read its problemIdx.
    int visibleSeen = 0;
    for (const Line &ln : std::as_const(m_lines)) {
        if (!isLineVisible(ln))
            continue;
        if (visibleSeen == blockNum) {
            if (ln.problemIdx >= 0 && ln.problemIdx < m_problems.size()) {
                const ProblemMatch &p = m_problems.at(ln.problemIdx);
                emit problemClicked(p.file, p.line, p.column);
            }
            return;
        }
        ++visibleSeen;
    }
}

void OutputPanel::onElapsedTick()
{
    const double secs = m_elapsed.elapsed() / 1000.0;
    m_elapsedLabel->setText(tr("%1 s").arg(secs, 0, 'f', 1));
}

void OutputPanel::parseLine(const QString &line)
{
    for (const Matcher &m : std::as_const(m_matchers)) {
        auto match = m.regex.match(line);
        if (match.hasMatch()) {
            ProblemMatch p;
            p.file     = match.captured(m.fileGroup);
            p.line     = match.captured(m.lineGroup).toInt();
            p.column   = (m.columnGroup > 0) ? match.captured(m.columnGroup).toInt() : 1;
            p.severity = m.severity;
            p.message  = match.captured(m.msgGroup);

            // Resolve relative paths
            if (!m_workDir.isEmpty() && QDir::isRelativePath(p.file))
                p.file = QDir(m_workDir).absoluteFilePath(p.file);

            const int outLine = m_output->document()->blockCount();
            m_lineToProb.insert(outLine, m_problems.size());
            m_problems.append(p);
            emit problemsChanged();
            return;
        }
    }
}

void OutputPanel::setupMatchers()
{
    // GCC/Clang:  file.cpp:10:5: error: ...
    m_matchers.append({
        QRegularExpression(QStringLiteral(R"(^(.+?):(\d+):(\d+):\s*(error|fatal error):\s*(.+)$)")),
        1, 2, 3, 5, ProblemMatch::Error
    });
    m_matchers.append({
        QRegularExpression(QStringLiteral(R"(^(.+?):(\d+):(\d+):\s*warning:\s*(.+)$)")),
        1, 2, 3, 4, ProblemMatch::Warning
    });
    // GCC/Clang no column:  file.cpp:10: error: ...
    m_matchers.append({
        QRegularExpression(QStringLiteral(R"(^(.+?):(\d+):\s*(error|fatal error):\s*(.+)$)")),
        1, 2, 0, 4, ProblemMatch::Error
    });

    // MSVC: file.cpp(10): error C2065: ...
    m_matchers.append({
        QRegularExpression(QStringLiteral(R"(^(.+?)\((\d+)\):\s*(error)\s+\w+:\s*(.+)$)")),
        1, 2, 0, 4, ProblemMatch::Error
    });
    m_matchers.append({
        QRegularExpression(QStringLiteral(R"(^(.+?)\((\d+)\):\s*(warning)\s+\w+:\s*(.+)$)")),
        1, 2, 0, 4, ProblemMatch::Warning
    });

    // Python:  File "file.py", line 10
    m_matchers.append({
        QRegularExpression(QStringLiteral(R"py(^\s*File "(.+?)", line (\d+))py")),
        1, 2, 0, 0, ProblemMatch::Error
    });

    // Rust/Cargo:  --> src/main.rs:10:5
    m_matchers.append({
        QRegularExpression(QStringLiteral(R"(^\s*-->\s*(.+?):(\d+):(\d+))")),
        1, 2, 3, 0, ProblemMatch::Error
    });

    // TypeScript/ESLint: file.ts(10,5): error TS2304: ...
    m_matchers.append({
        QRegularExpression(QStringLiteral(R"(^(.+?)\((\d+),(\d+)\):\s*(error)\s+\w+:\s*(.+)$)")),
        1, 2, 3, 5, ProblemMatch::Error
    });

    // Generic: file:line: message
    m_matchers.append({
        QRegularExpression(QStringLiteral(R"(^(.+?):(\d+):\s*(.+)$)")),
        1, 2, 0, 3, ProblemMatch::Info
    });
}

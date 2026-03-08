#include "runlaunchpanel.h"

#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

RunLaunchPanel::RunLaunchPanel(QWidget *parent)
    : QWidget(parent),
      m_elapsed(new QElapsedTimer),
      m_elapsedTimer(new QTimer(this))
{
    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 2, 4, 2);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(200);
    toolbar->addWidget(m_profileCombo);

    m_runBtn  = new QPushButton(tr("▶ Run"), this);
    m_stopBtn = new QPushButton(tr("■ Stop"), this);
    m_clearBtn = new QPushButton(tr("✕ Clear"), this);
    m_stopBtn->setEnabled(false);
    toolbar->addWidget(m_runBtn);
    toolbar->addWidget(m_stopBtn);
    toolbar->addWidget(m_clearBtn);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #888; font-size: 11px; padding-left: 8px; }"));
    toolbar->addWidget(m_statusLabel);
    toolbar->addStretch();

    vbox->addLayout(toolbar);

    // Output
    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setUndoRedoEnabled(false);
    m_output->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_output->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: #1a1a1a; color: #ccc; border: none; }"));
    m_output->setWordWrapMode(QTextOption::NoWrap);
    vbox->addWidget(m_output);

    // Elapsed timer
    m_elapsedTimer->setInterval(500);
    connect(m_elapsedTimer, &QTimer::timeout, this, &RunLaunchPanel::onElapsedTick);

    // Button signals
    connect(m_runBtn, &QPushButton::clicked, this, &RunLaunchPanel::launch);
    connect(m_stopBtn, &QPushButton::clicked, this, &RunLaunchPanel::stopProcess);
    connect(m_clearBtn, &QPushButton::clicked, this, [this] {
        m_output->clear();
        m_statusLabel->clear();
    });
}

// ── Public API ────────────────────────────────────────────────────────────────

void RunLaunchPanel::setWorkingDirectory(const QString &dir)
{
    m_workDir = dir;
    if (m_profiles.isEmpty())
        autoDetectTargets();
}

void RunLaunchPanel::loadLaunchJson(const QString &jsonPath)
{
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonArray configs = doc.object()[QStringLiteral("configurations")].toArray();
    QList<LaunchProfile> profiles;

    for (const QJsonValue &v : configs) {
        const QJsonObject obj = v.toObject();
        LaunchProfile p;
        p.name    = obj[QStringLiteral("name")].toString();
        p.program = obj[QStringLiteral("program")].toString();
        for (const QJsonValue &a : obj[QStringLiteral("args")].toArray())
            p.args << a.toString();
        p.workDir   = obj[QStringLiteral("cwd")].toString();
        p.isDefault = obj[QStringLiteral("default")].toBool(false);

        const QJsonObject envObj = obj[QStringLiteral("env")].toObject();
        for (auto it = envObj.begin(); it != envObj.end(); ++it)
            p.env.insert(it.key(), it.value().toString());

        if (!p.name.isEmpty() && !p.program.isEmpty())
            profiles.append(p);
    }

    if (!profiles.isEmpty()) {
        m_profiles = profiles;
        populateCombo();
    }
}

void RunLaunchPanel::saveLaunchJson(const QString &jsonPath) const
{
    QJsonArray configs;
    for (const LaunchProfile &p : m_profiles) {
        QJsonObject obj;
        obj[QStringLiteral("name")]    = p.name;
        obj[QStringLiteral("program")] = p.program;
        if (!p.args.isEmpty()) {
            QJsonArray args;
            for (const QString &a : p.args)
                args.append(a);
            obj[QStringLiteral("args")] = args;
        }
        if (!p.workDir.isEmpty())
            obj[QStringLiteral("cwd")] = p.workDir;
        if (p.isDefault)
            obj[QStringLiteral("default")] = true;
        if (!p.env.isEmpty()) {
            QJsonObject envObj;
            for (auto it = p.env.begin(); it != p.env.end(); ++it)
                envObj.insert(it.key(), it.value());
            obj[QStringLiteral("env")] = envObj;
        }
        configs.append(obj);
    }

    QJsonObject root;
    root[QStringLiteral("configurations")] = configs;

    QFile f(jsonPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void RunLaunchPanel::autoDetectTargets()
{
    if (m_workDir.isEmpty()) return;
    QDir dir(m_workDir);
    QList<LaunchProfile> detected;

    // Detect build output from CMake
    if (dir.exists(QStringLiteral("CMakeLists.txt"))) {
        // Look for build directories with executables
        for (const QString &buildDir : {
                 QStringLiteral("build"), QStringLiteral("build-llvm"),
                 QStringLiteral("build-debug"), QStringLiteral("cmake-build-debug")}) {
            const QDir bd(dir.filePath(buildDir));
            if (!bd.exists()) continue;

            // Search for .exe on Windows, executables on Unix
            const QStringList exeFilters =
#ifdef Q_OS_WIN
                {QStringLiteral("*.exe")};
#else
                {};  // no filter — check executable bit
#endif
            const QFileInfoList exes = bd.entryInfoList(
                exeFilters, QDir::Files | QDir::Executable, QDir::Name);

            for (const QFileInfo &exe : exes) {
                LaunchProfile p;
                p.name    = tr("Run %1").arg(exe.fileName());
                p.program = exe.absoluteFilePath();
                p.workDir = dir.absolutePath();
                if (detected.isEmpty()) p.isDefault = true;
                detected.append(p);
            }

            // Also search one level deeper (src/, bin/)
            for (const QString &sub : {QStringLiteral("src"), QStringLiteral("bin")}) {
                const QDir sd(bd.filePath(sub));
                if (!sd.exists()) continue;
                const QFileInfoList subExes = sd.entryInfoList(
                    exeFilters, QDir::Files | QDir::Executable, QDir::Name);
                for (const QFileInfo &exe : subExes) {
                    LaunchProfile p;
                    p.name    = tr("Run %1").arg(exe.fileName());
                    p.program = exe.absoluteFilePath();
                    p.workDir = dir.absolutePath();
                    detected.append(p);
                }
            }
        }
    }

    // Cargo (Rust)
    if (dir.exists(QStringLiteral("Cargo.toml"))) {
        LaunchProfile p;
        p.name    = tr("Cargo Run");
        p.program = QStringLiteral("cargo");
        p.args    = {QStringLiteral("run")};
        p.workDir = dir.absolutePath();
        detected.append(p);
    }

    // Python
    if (dir.exists(QStringLiteral("main.py"))) {
        LaunchProfile p;
        p.name    = tr("Python main.py");
        p.program = QStringLiteral("python");
        p.args    = {QStringLiteral("main.py")};
        p.workDir = dir.absolutePath();
        detected.append(p);
    } else if (dir.exists(QStringLiteral("app.py"))) {
        LaunchProfile p;
        p.name    = tr("Python app.py");
        p.program = QStringLiteral("python");
        p.args    = {QStringLiteral("app.py")};
        p.workDir = dir.absolutePath();
        detected.append(p);
    }

    // npm
    if (dir.exists(QStringLiteral("package.json"))) {
        LaunchProfile p;
        p.name    = tr("npm start");
        p.program = QStringLiteral("npm");
        p.args    = {QStringLiteral("start")};
        p.workDir = dir.absolutePath();
        detected.append(p);
    }

    // .NET
    const QStringList csproj = dir.entryList({QStringLiteral("*.csproj")}, QDir::Files);
    if (!csproj.isEmpty()) {
        LaunchProfile p;
        p.name    = tr("dotnet run");
        p.program = QStringLiteral("dotnet");
        p.args    = {QStringLiteral("run")};
        p.workDir = dir.absolutePath();
        detected.append(p);
    }

    // Go
    if (dir.exists(QStringLiteral("go.mod"))) {
        LaunchProfile p;
        p.name    = tr("Go Run");
        p.program = QStringLiteral("go");
        p.args    = {QStringLiteral("run"), QStringLiteral(".")};
        p.workDir = dir.absolutePath();
        detected.append(p);
    }

    if (!detected.isEmpty()) {
        m_profiles = detected;
        populateCombo();
    }
}

void RunLaunchPanel::launch()
{
    const int idx = m_profileCombo->currentIndex();
    if (idx >= 0 && idx < m_profiles.size())
        runProfile(m_profiles.at(idx));
}

void RunLaunchPanel::stopProcess()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
        m_process->kill();
}

// ── Internal ──────────────────────────────────────────────────────────────────

void RunLaunchPanel::populateCombo()
{
    m_profileCombo->clear();
    int defaultIdx = -1;
    for (int i = 0; i < m_profiles.size(); ++i) {
        m_profileCombo->addItem(m_profiles.at(i).name);
        if (m_profiles.at(i).isDefault)
            defaultIdx = i;
    }
    if (defaultIdx >= 0)
        m_profileCombo->setCurrentIndex(defaultIdx);
}

QString RunLaunchPanel::substituteVars(const QString &input) const
{
    QString s = input;
    s.replace(QStringLiteral("${workspaceFolder}"), m_workDir);
    s.replace(QStringLiteral("${workspaceRoot}"), m_workDir);

    // ${env:VAR} substitution
    static const QRegularExpression reEnv(QStringLiteral("\\$\\{env:([^}]+)\\}"));
    QRegularExpressionMatchIterator it = reEnv.globalMatch(s);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString val = QProcessEnvironment::systemEnvironment().value(m.captured(1));
        s.replace(m.captured(0), val);
    }
    return s;
}

void RunLaunchPanel::runProfile(const LaunchProfile &profile)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }

    m_output->clear();
    m_output->appendPlainText(tr("▶ Running: %1\n").arg(profile.name));

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::readyReadStandardOutput,
                this, &RunLaunchPanel::onReadyReadStdout);
        connect(m_process, &QProcess::readyReadStandardError,
                this, &RunLaunchPanel::onReadyReadStderr);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &RunLaunchPanel::onProcessFinished);
    }

    const QString program = substituteVars(profile.program);
    QStringList args;
    for (const QString &a : profile.args)
        args << substituteVars(a);

    const QString workDir = profile.workDir.isEmpty()
                                ? m_workDir
                                : substituteVars(profile.workDir);

    if (!workDir.isEmpty())
        m_process->setWorkingDirectory(workDir);

    // Environment
    if (!profile.env.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (auto it = profile.env.begin(); it != profile.env.end(); ++it)
            env.insert(it.key(), substituteVars(it.value()));
        m_process->setProcessEnvironment(env);
    }

    m_process->start(program, args);
    m_runBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_elapsed->start();
    m_elapsedTimer->start();
    m_statusLabel->setText(tr("Running…"));

    emit processStarted(profile.name);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void RunLaunchPanel::onReadyReadStdout()
{
    const QString text = QString::fromUtf8(m_process->readAllStandardOutput());
    QTextCharFormat fmt;
    fmt.setForeground(QColor(0xcc, 0xcc, 0xcc));
    QTextCursor cur = m_output->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(text, fmt);
    m_output->verticalScrollBar()->setValue(
        m_output->verticalScrollBar()->maximum());
}

void RunLaunchPanel::onReadyReadStderr()
{
    const QString text = QString::fromUtf8(m_process->readAllStandardError());
    QTextCharFormat fmt;
    fmt.setForeground(QColor(0xf4, 0x47, 0x47));
    QTextCursor cur = m_output->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(text, fmt);
    m_output->verticalScrollBar()->setValue(
        m_output->verticalScrollBar()->maximum());
}

void RunLaunchPanel::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_elapsedTimer->stop();
    const qint64 ms = m_elapsed->elapsed();
    const double secs = ms / 1000.0;

    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);

    const QString exitMsg = (status == QProcess::CrashExit)
        ? tr("\n✕ Process crashed (exit code %1) [%2s]").arg(exitCode).arg(secs, 0, 'f', 1)
        : tr("\n✓ Process exited with code %1 [%2s]").arg(exitCode).arg(secs, 0, 'f', 1);

    QTextCharFormat fmt;
    fmt.setForeground(exitCode == 0 ? QColor(0x4e, 0xc9, 0xb0) : QColor(0xf4, 0x47, 0x47));
    QTextCursor cur = m_output->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(exitMsg, fmt);

    m_statusLabel->setText(tr("Exited (%1) %2s").arg(exitCode).arg(secs, 0, 'f', 1));

    emit processFinished(m_profileCombo->currentText(), exitCode);
}

void RunLaunchPanel::onElapsedTick()
{
    const qint64 ms = m_elapsed->elapsed();
    const int secs = static_cast<int>(ms / 1000);
    const int mins = secs / 60;
    if (mins > 0)
        m_statusLabel->setText(tr("Running %1:%2")
                                   .arg(mins)
                                   .arg(secs % 60, 2, 10, QLatin1Char('0')));
    else
        m_statusLabel->setText(tr("Running %1s").arg(secs));
}

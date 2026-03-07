#include "outputpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextBlock>
#include <QFont>
#include <QDir>

OutputPanel::OutputPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 2, 4, 2);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(180);
    m_profileCombo->addItem(tr("cmake --build build"));
    m_profileCombo->addItem(tr("make"));
    m_profileCombo->addItem(tr("cargo build"));
    m_profileCombo->addItem(tr("python -m pytest"));
    m_profileCombo->addItem(tr("npm run build"));
    m_profileCombo->setEditable(true);
    toolbar->addWidget(m_profileCombo);

    m_runBtn = new QPushButton(tr("▶ Run"), this);
    m_stopBtn = new QPushButton(tr("■ Stop"), this);
    m_clearBtn = new QPushButton(tr("✕ Clear"), this);
    m_stopBtn->setEnabled(false);

    toolbar->addWidget(m_runBtn);
    toolbar->addWidget(m_stopBtn);
    toolbar->addWidget(m_clearBtn);
    toolbar->addStretch();

    vbox->addLayout(toolbar);

    // Output text
    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setUndoRedoEnabled(false);
    m_output->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_output->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: #1a1a1a; color: #ccc; border: none; }"));
    m_output->setWordWrapMode(QTextOption::NoWrap);
    vbox->addWidget(m_output);

    // Signals
    connect(m_runBtn, &QPushButton::clicked, this, [this] {
        const QString cmd = m_profileCombo->currentText();
        if (!cmd.isEmpty())
            runCommand(cmd);
    });
    connect(m_stopBtn, &QPushButton::clicked, this, [this] {
        if (m_process && m_process->state() != QProcess::NotRunning)
            m_process->kill();
    });
    connect(m_clearBtn, &QPushButton::clicked, this, &OutputPanel::clear);

    // Click on output navigates to error
    connect(m_output, &QPlainTextEdit::cursorPositionChanged, this, &OutputPanel::onTextClicked);

    setupMatchers();
}

void OutputPanel::setWorkingDirectory(const QString &dir)
{
    m_workDir = dir;
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
    m_process->setWorkingDirectory(m_workDir.isEmpty() ? QDir::currentPath() : m_workDir);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &OutputPanel::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError,  this, &OutputPanel::onReadyReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OutputPanel::onProcessFinished);

    appendText(tr("▶ Running: %1\n").arg(cmd), QColor(100, 180, 255));

    m_runBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

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

void OutputPanel::appendText(const QString &text, const QColor &color)
{
    QTextCharFormat fmt;
    if (color.isValid())
        fmt.setForeground(color);

    QTextCursor cur = m_output->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(text, fmt);
    m_output->setTextCursor(cur);
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void OutputPanel::clear()
{
    m_output->clear();
    m_problems.clear();
    m_lineToProb.clear();
    emit problemsChanged();
}

void OutputPanel::onReadyReadStdout()
{
    const QString text = QString::fromUtf8(m_process->readAllStandardOutput());
    const auto lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        parseLine(line);
        appendText(line + QLatin1Char('\n'));
    }
}

void OutputPanel::onReadyReadStderr()
{
    const QString text = QString::fromUtf8(m_process->readAllStandardError());
    const auto lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        parseLine(line);
        // Color stderr lines: errors in red, warnings in yellow
        QColor c(220, 100, 100);
        if (line.contains(QLatin1String("warning"), Qt::CaseInsensitive))
            c = QColor(220, 200, 80);
        appendText(line + QLatin1Char('\n'), c);
    }
}

void OutputPanel::onProcessFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    const QColor c = (exitCode == 0) ? QColor(80, 200, 80) : QColor(220, 80, 80);
    appendText(tr("\n✔ Process exited with code %1  (%2 problem(s))\n")
               .arg(exitCode).arg(m_problems.size()), c);
    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    emit buildFinished(exitCode);
}

void OutputPanel::onTextClicked()
{
    const int blockNum = m_output->textCursor().blockNumber();
    auto it = m_lineToProb.constFind(blockNum);
    if (it != m_lineToProb.constEnd()) {
        const ProblemMatch &p = m_problems.at(it.value());
        emit problemClicked(p.file, p.line, p.column);
    }
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

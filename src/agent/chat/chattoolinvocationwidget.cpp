#include "chattoolinvocationwidget.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QTimer>

ChatToolInvocationWidget::ChatToolInvocationWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(0);

    m_card = new QWidget(this);
    m_card->setObjectName(QStringLiteral("toolCard"));
    m_card->setStyleSheet(
        QStringLiteral("QWidget#toolCard { background:%1; border-radius:4px;"
                      " padding:8px 10px; border-left:3px solid %2; }")
            .arg(ChatTheme::pick(ChatTheme::ToolBg, ChatTheme::L_ToolBg),
                 ChatTheme::pick(ChatTheme::ToolBorderDefault,
                                 ChatTheme::L_ToolBorderDefault)));

    auto *cardLayout = new QVBoxLayout(m_card);
    cardLayout->setContentsMargins(10, 6, 10, 6);
    cardLayout->setSpacing(4);

    // Title row: icon + name + state badge
    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(6);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:13px;")
            .arg(ChatTheme::pick(ChatTheme::FgSecondary, ChatTheme::L_FgSecondary)));
    m_iconLabel->setFixedWidth(18);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet(
        QStringLiteral("color:%1; font-weight:600; font-size:12px;")
            .arg(ChatTheme::pick(ChatTheme::FgPrimary, ChatTheme::L_FgPrimary)));

    m_stateBadge = new QLabel(this);
    m_stateBadge->setStyleSheet(
        QStringLiteral("color:%1; font-size:11px;")
            .arg(ChatTheme::pick(ChatTheme::FgDimmed, ChatTheme::L_FgDimmed)));

    m_ioToggle = new QToolButton(this);
    m_ioToggle->setText(QStringLiteral("\u25B6"));
    m_ioToggle->setStyleSheet(
        QStringLiteral("QToolButton { color:%1; background:transparent;"
                      " border:none; font-size:10px; padding:2px; }")
            .arg(ChatTheme::pick(ChatTheme::FgDimmed, ChatTheme::L_FgDimmed)));
    m_ioToggle->setCheckable(true);
    connect(m_ioToggle, &QToolButton::toggled, this, [this](bool on) {
        m_ioPanel->setVisible(on);
        m_ioToggle->setText(on ? QStringLiteral("\u25BC") : QStringLiteral("\u25B6"));
        updateGeometry();
    });

    titleRow->addWidget(m_iconLabel);
    titleRow->addWidget(m_titleLabel, 1);
    titleRow->addWidget(m_stateBadge);
    titleRow->addWidget(m_ioToggle);
    cardLayout->addLayout(titleRow);

    // Description
    m_descLabel = new QLabel(this);
    m_descLabel->setWordWrap(true);
    m_descLabel->setTextFormat(Qt::RichText);
    m_descLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:12px;")
            .arg(ChatTheme::pick(ChatTheme::FgSecondary, ChatTheme::L_FgSecondary)));
    cardLayout->addWidget(m_descLabel);

    // Collapsible IO panel
    m_ioPanel = new QWidget(this);
    m_ioPanel->hide();
    auto *ioLayout = new QVBoxLayout(m_ioPanel);
    ioLayout->setContentsMargins(0, 6, 0, 0);
    ioLayout->setSpacing(6);

    m_inputLabel = new QLabel(this);
    m_inputLabel->setWordWrap(true);
    m_inputLabel->setTextFormat(Qt::RichText);
    m_inputLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_inputLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:11px; font-family:%2;"
                      " background:%3; padding:6px 8px; border-radius:3px;")
            .arg(ChatTheme::pick(ChatTheme::FgSecondary, ChatTheme::L_FgSecondary),
                 ChatTheme::MonoFamily,
                 ChatTheme::pick(ChatTheme::CodeBg, ChatTheme::L_CodeBg)));

    m_outputLabel = new QLabel(this);
    m_outputLabel->setWordWrap(true);
    m_outputLabel->setTextFormat(Qt::RichText);
    m_outputLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_outputLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:11px; font-family:%2;"
                      " background:%3; padding:6px 8px; border-radius:3px;")
            .arg(ChatTheme::pick(ChatTheme::FgSecondary, ChatTheme::L_FgSecondary),
                 ChatTheme::MonoFamily,
                 ChatTheme::pick(ChatTheme::CodeBg, ChatTheme::L_CodeBg)));

    ioLayout->addWidget(m_inputLabel);
    ioLayout->addWidget(m_outputLabel);
    cardLayout->addWidget(m_ioPanel);

    // Confirmation buttons (hidden unless state == ConfirmationNeeded)
    m_confirmRow = new QWidget(this);
    m_confirmRow->hide();
    auto *confirmLayout = new QHBoxLayout(m_confirmRow);
    confirmLayout->setContentsMargins(0, 6, 0, 0);
    confirmLayout->setSpacing(6);

    // Allow button with dropdown menu for permission scope
    const QString primaryBtnStyle = QStringLiteral(
        "QToolButton { background:%1; color:%2; padding:4px 12px;"
        " border-radius:4px; font-size:12px; border:none; }"
        "QToolButton:hover { background:%3; }"
        "QToolButton:disabled { background:%4; color:%5; }"
        "QToolButton::menu-indicator { image:none; width:0; }")
            .arg(ChatTheme::ButtonBg, ChatTheme::ButtonFg, ChatTheme::ButtonHover,
                 ChatTheme::DisabledBg, ChatTheme::DisabledFg);

    m_allowBtn = new QToolButton(this);
    m_allowBtn->setText(tr("Allow"));
    m_allowBtn->setStyleSheet(primaryBtnStyle);
    connect(m_allowBtn, &QToolButton::clicked, this, [this]() {
        m_allowBtn->setEnabled(false);
        m_denyBtn->setEnabled(false);
        m_allowMenuBtn->setEnabled(false);
        emit confirmed(m_callId, 1); // AllowOnce
    });

    // Dropdown arrow for more options
    m_allowMenuBtn = new QToolButton(this);
    m_allowMenuBtn->setText(QStringLiteral("\u25BC"));
    m_allowMenuBtn->setStyleSheet(QStringLiteral(
        "QToolButton { background:%1; color:%2; padding:4px 4px;"
        " border-radius:0 4px 4px 0; font-size:8px; border:none;"
        " border-left:1px solid %3; }"
        "QToolButton:hover { background:%4; }"
        "QToolButton:disabled { background:%5; color:%6; }")
            .arg(ChatTheme::ButtonBg, ChatTheme::ButtonFg,
                 ChatTheme::ButtonHover, ChatTheme::ButtonHover,
                 ChatTheme::DisabledBg, ChatTheme::DisabledFg));
    m_allowMenuBtn->setFixedWidth(20);

    auto *allowMenu = new QMenu(this);
    allowMenu->setStyleSheet(QStringLiteral(
        "QMenu { background:%1; border:1px solid %2; padding:4px 0;"
        " color:%3; font-size:12px; }"
        "QMenu::item { padding:5px 16px; }"
        "QMenu::item:selected { background:%4; }")
            .arg(ChatTheme::ScrollTrack, ChatTheme::Border,
                 ChatTheme::FgPrimary, ChatTheme::ListSelection));

    auto *allowOnceAct = allowMenu->addAction(tr("Allow Once"));
    auto *allowAlwaysAct = allowMenu->addAction(tr("Always Allow This Tool"));
    allowMenu->addSeparator();
    auto *allowSessionAct = allowMenu->addAction(tr("Allow for This Session"));

    connect(allowOnceAct, &QAction::triggered, this, [this]() {
        m_allowBtn->setEnabled(false);
        m_denyBtn->setEnabled(false);
        m_allowMenuBtn->setEnabled(false);
        emit confirmed(m_callId, 1); // AllowOnce
    });
    connect(allowAlwaysAct, &QAction::triggered, this, [this]() {
        m_allowBtn->setEnabled(false);
        m_denyBtn->setEnabled(false);
        m_allowMenuBtn->setEnabled(false);
        emit confirmed(m_callId, 2); // AllowAlways
    });
    connect(allowSessionAct, &QAction::triggered, this, [this]() {
        m_allowBtn->setEnabled(false);
        m_denyBtn->setEnabled(false);
        m_allowMenuBtn->setEnabled(false);
        emit confirmed(m_callId, 2); // AllowAlways (session-scoped)
    });
    m_allowMenuBtn->setMenu(allowMenu);
    m_allowMenuBtn->setPopupMode(QToolButton::InstantPopup);

    m_denyBtn = new QToolButton(this);
    m_denyBtn->setText(tr("Deny"));
    m_denyBtn->setStyleSheet(
        QStringLiteral("QToolButton { background:%1; color:%2; padding:4px 12px;"
                      " border-radius:4px; font-size:12px; border:none; }"
                      "QToolButton:hover { background:%3; }"
                      "QToolButton:disabled { background:%4; color:%5; }")
            .arg(ChatTheme::SecondaryBtnBg, ChatTheme::SecondaryBtnFg,
                 ChatTheme::SecondaryBtnHover,
                 ChatTheme::DisabledBg, ChatTheme::DisabledFg));
    connect(m_denyBtn, &QToolButton::clicked, this, [this]() {
        m_allowBtn->setEnabled(false);
        m_denyBtn->setEnabled(false);
        m_allowMenuBtn->setEnabled(false);
        emit confirmed(m_callId, 0); // Deny
    });

    confirmLayout->addWidget(m_allowBtn);
    confirmLayout->addWidget(m_allowMenuBtn);
    confirmLayout->addSpacing(4);
    confirmLayout->addWidget(m_denyBtn);
    confirmLayout->addStretch();
    cardLayout->addWidget(m_confirmRow);

    layout->addWidget(m_card);
}

void ChatToolInvocationWidget::setToolData(const ChatContentPart &part)
{
    m_callId = part.toolCallId;
    m_toolName = part.toolName;
    m_titleLabel->setText(part.toolFriendlyTitle.isEmpty()
        ? part.toolName : part.toolFriendlyTitle);
    m_iconLabel->setText(iconForTool(part.toolName));

    if (!part.toolInput.isEmpty()) {
        m_inputLabel->setText(
            QStringLiteral("<span style='color:%1;font-weight:600;'>"
                          "Input</span><br>%2")
                .arg(ChatTheme::pick(ChatTheme::FgPrimary, ChatTheme::L_FgPrimary),
                     formatToolInput(part.toolName, part.toolInput)));
        m_inputLabel->show();
    } else {
        m_inputLabel->hide();
    }

    if (!part.toolOutput.isEmpty()) {
        m_outputLabel->setText(
            QStringLiteral("<span style='color:%1;font-weight:600;'>"
                          "Output</span><br>%2")
                .arg(ChatTheme::pick(ChatTheme::FgPrimary, ChatTheme::L_FgPrimary),
                     formatToolOutput(part.toolName, part.toolOutput)));
        m_outputLabel->show();
    } else {
        m_outputLabel->hide();
    }

    updateState(part);
}

void ChatToolInvocationWidget::updateState(const ChatContentPart &part)
{
    const auto state = part.toolState;
    QString borderColor;
    QString stateText;

    // Stop spinner if running
    if (m_spinTimer) {
        m_spinTimer->stop();
        m_spinTimer->deleteLater();
        m_spinTimer = nullptr;
    }

    // Always hide confirm row first — only ConfirmationNeeded re-shows it
    m_confirmRow->hide();

    switch (state) {
    case ChatContentPart::ToolState::Queued:
        borderColor = ChatTheme::pick(ChatTheme::ToolBorderDefault,
                                      ChatTheme::L_ToolBorderDefault);
        stateText = QStringLiteral("\u25CB queued");
        m_descLabel->setText(part.toolInvocationMsg);
        break;
    case ChatContentPart::ToolState::Streaming:
        borderColor = ChatTheme::ToolBorderActive;
        stateText = spinnerFrame() + QStringLiteral(" running\u2026");
        m_descLabel->setText(part.toolInvocationMsg);
        startSpinner();
        break;
    case ChatContentPart::ToolState::ConfirmationNeeded:
        borderColor = ChatTheme::WarningFg;
        stateText = QStringLiteral("\u26A0 confirmation needed");
        m_descLabel->setText(part.toolInvocationMsg);
        m_confirmRow->show();
        m_allowBtn->setEnabled(true);
        m_denyBtn->setEnabled(true);
        m_allowMenuBtn->setEnabled(true);
        break;
    case ChatContentPart::ToolState::CompleteSuccess:
        borderColor = ChatTheme::ToolBorderOk;
        stateText = QStringLiteral("\u2713");
        if (isTerminalTool() && !part.toolInvocationMsg.isEmpty())
            m_descLabel->setText(part.toolInvocationMsg);
        else
            m_descLabel->setText(part.toolPastTenseMsg.isEmpty()
                ? part.toolInvocationMsg : part.toolPastTenseMsg);
        // Auto-expand output for terminal commands
        if (isTerminalTool() && !part.toolOutput.isEmpty()
            && !m_ioToggle->isChecked()) {
            m_ioToggle->setChecked(true);
        }
        break;
    case ChatContentPart::ToolState::CompleteError:
        borderColor = ChatTheme::ToolBorderFail;
        stateText = QStringLiteral("\u2717 failed");
        m_descLabel->setText(part.toolOutput.isEmpty()
            ? part.toolInvocationMsg : part.toolOutput);
        // Auto-expand output on error for all tools
        if (!part.toolOutput.isEmpty() && !m_ioToggle->isChecked())
            m_ioToggle->setChecked(true);
        break;
    }

    m_card->setStyleSheet(
        QStringLiteral("QWidget#toolCard { background:%1; border-radius:4px;"
                      " padding:8px 10px; border-left:3px solid %2; }")
            .arg(ChatTheme::pick(ChatTheme::ToolBg, ChatTheme::L_ToolBg),
                 borderColor));
    m_stateBadge->setText(stateText);

    if (!part.toolOutput.isEmpty()) {
        m_outputLabel->setText(
            QStringLiteral("<span style='color:%1;font-weight:600;'>"
                          "Output</span><br>%2")
                .arg(ChatTheme::pick(ChatTheme::FgPrimary, ChatTheme::L_FgPrimary),
                     formatToolOutput(part.toolName, part.toolOutput)));
        m_outputLabel->show();
    }
}

QString ChatToolInvocationWidget::formatToolInput(const QString &toolName, const QString &text)
{
    const QString trimmed = text.trimmed();
    if (!trimmed.startsWith(QLatin1Char('{')))
        return escapeAndLimit(trimmed, 300);

    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(trimmed.toUtf8(), &err);
    if (err.error || !doc.isObject())
        return escapeAndLimit(trimmed, 300);

    const QJsonObject obj = doc.object();

    // Tool-specific smart summaries
    if (toolName == QLatin1String("read_file")) {
        return keyValueLine(QStringLiteral("file"),
                            obj.value(QLatin1String("filePath")).toString());
    }
    if (toolName == QLatin1String("create_file")) {
        const QString fp = obj.value(QLatin1String("filePath")).toString();
        const QString content = obj.value(QLatin1String("content")).toString();
        return keyValueLine(QStringLiteral("file"), fp)
             + keyValueLine(QStringLiteral("content"),
                           limitText(content, 200) + QStringLiteral(" ..."));
    }
    if (toolName == QLatin1String("replace_string_in_file")
        || toolName == QLatin1String("edit_file")) {
        const QString fp = obj.value(QLatin1String("filePath")).toString();
        const QString old = obj.value(QLatin1String("oldString")).toString();
        const QString nw  = obj.value(QLatin1String("newString")).toString();
        QString html = keyValueLine(QStringLiteral("file"), fp);
        if (!old.isEmpty())
            html += diffLine(limitText(old, 150), false);
        if (!nw.isEmpty())
            html += diffLine(limitText(nw, 150), true);
        return html;
    }
    if (toolName == QLatin1String("run_in_terminal")
        || toolName == QLatin1String("run_command")) {
        const QString cmd = obj.value(QLatin1String("command")).toString();
        const QString expl = obj.value(QLatin1String("explanation")).toString();
        QString html;
        if (!expl.isEmpty())
            html += keyValueLine(QStringLiteral("purpose"), expl);
        if (!cmd.isEmpty())
            html += QStringLiteral("<code>%1</code>")
                        .arg(limitText(cmd, 200).toHtmlEscaped());
        return html;
    }
    if (toolName == QLatin1String("grep_search")
        || toolName == QLatin1String("semantic_search")
        || toolName == QLatin1String("codebase_search")) {
        const QString q = obj.value(QLatin1String("query")).toString();
        return keyValueLine(QStringLiteral("query"),
                           QStringLiteral("\u201C") + q + QStringLiteral("\u201D"));
    }
    if (toolName == QLatin1String("list_dir")) {
        return keyValueLine(QStringLiteral("path"),
                           obj.value(QLatin1String("path")).toString());
    }
    if (toolName == QLatin1String("file_search")) {
        return keyValueLine(QStringLiteral("query"),
                           obj.value(QLatin1String("query")).toString());
    }

    // Generic JSON: show formatted key-value pairs
    return formatJsonObject(obj, 0, 400);
}

QString ChatToolInvocationWidget::formatToolOutput(const QString &toolName, const QString &text)
{
    const QString trimmed = text.trimmed();

    // Try JSON
    if (trimmed.startsWith(QLatin1Char('{')) || trimmed.startsWith(QLatin1Char('['))) {
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(trimmed.toUtf8(), &err);
        if (!err.error) {
            if (doc.isObject())
                return formatJsonObject(doc.object(), 0, 800);
            if (doc.isArray())
                return formatJsonArray(doc.array(), 0, 800);
        }
    }

    // For terminal output, preserve line breaks
    if (toolName == QLatin1String("run_in_terminal")
        || toolName == QLatin1String("run_command")
        || toolName == QLatin1String("get_terminal_output")) {
        return formatTerminalOutput(trimmed);
    }

    // Plain text with line-break preservation
    return escapeAndLimit(trimmed, 800);
}

QString ChatToolInvocationWidget::formatJsonObject(const QJsonObject &obj, int depth, int maxLen)
{
    if (obj.isEmpty())
        return QStringLiteral("<span style='color:%1;'>{}</span>")
                   .arg(ChatTheme::FgDimmed);

    QStringList lines;
    int totalLen = 0;
    const QString innerIndent = QString((depth + 1) * 2, QLatin1Char(' '))
                                    .replace(QLatin1Char(' '), QStringLiteral("&nbsp;"));

    for (auto it = obj.begin(); it != obj.end() && totalLen < maxLen; ++it) {
        const QString key = it.key();
        const QString keyHtml = QStringLiteral(
            "<span style='color:%1;'>%2</span>")
                .arg(ChatTheme::KeywordBlue, key.toHtmlEscaped());

        QString valHtml = formatJsonValue(it.value(), depth + 1, maxLen - totalLen);
        QString line = innerIndent + keyHtml
                     + QStringLiteral("<span style='color:%1;'>: </span>")
                           .arg(ChatTheme::FgDimmed)
                     + valHtml;
        totalLen += line.size();
        lines << line;
    }

    if (totalLen >= maxLen && lines.size() < obj.size())
        lines << innerIndent + QStringLiteral("<span style='color:%1;'>\u2026 "
                                              "%2 more</span>")
                     .arg(ChatTheme::FgDimmed)
                     .arg(obj.size() - lines.size());

    return lines.join(QStringLiteral("<br>"));
}

QString ChatToolInvocationWidget::formatJsonArray(const QJsonArray &arr, int depth, int maxLen)
{
    if (arr.isEmpty())
        return QStringLiteral("<span style='color:%1;'>[]</span>")
                   .arg(ChatTheme::FgDimmed);

    // For short arrays of primitives, show inline
    if (arr.size() <= 5 && depth > 0) {
        bool allPrimitive = true;
        for (const auto &v : arr) {
            if (v.isObject() || v.isArray()) { allPrimitive = false; break; }
        }
        if (allPrimitive) {
            QStringList items;
            for (const auto &v : arr)
                items << formatJsonValue(v, depth + 1, 100);
            return QStringLiteral("[") + items.join(QStringLiteral(", "))
                 + QStringLiteral("]");
        }
    }

    QStringList lines;
    int totalLen = 0;
    const QString innerIndent = QString((depth + 1) * 2, QLatin1Char(' '))
                                    .replace(QLatin1Char(' '), QStringLiteral("&nbsp;"));
    int shown = 0;
    for (const auto &val : arr) {
        if (totalLen >= maxLen) break;
        QString valHtml = formatJsonValue(val, depth + 1, maxLen - totalLen);
        QString line = innerIndent + valHtml;
        totalLen += line.size();
        lines << line;
        ++shown;
    }

    if (shown < arr.size())
        lines << innerIndent + QStringLiteral("<span style='color:%1;'>\u2026 "
                                              "%2 more items</span>")
                     .arg(ChatTheme::FgDimmed)
                     .arg(arr.size() - shown);

    return lines.join(QStringLiteral("<br>"));
}

QString ChatToolInvocationWidget::formatJsonValue(const QJsonValue &val, int depth, int maxLen)
{
    if (val.isString()) {
        QString s = val.toString();
        if (s.length() > 200)
            s = s.left(197) + QStringLiteral("\u2026");
        return QStringLiteral("<span style='color:%1;'>\"%2\"</span>")
                   .arg(ChatTheme::FgCode, s.toHtmlEscaped());
    }
    if (val.isBool()) {
        return QStringLiteral("<span style='color:%1;'>%2</span>")
                   .arg(ChatTheme::KeywordBlue,
                        val.toBool() ? QStringLiteral("true")
                                     : QStringLiteral("false"));
    }
    if (val.isDouble()) {
        double d = val.toDouble();
        QString num = (d == static_cast<int>(d))
            ? QString::number(static_cast<int>(d))
            : QString::number(d);
        return QStringLiteral("<span style='color:%1;'>%2</span>")
                   .arg(ChatTheme::SuccessFg, num);
    }
    if (val.isNull()) {
        return QStringLiteral("<span style='color:%1;'>null</span>")
                   .arg(ChatTheme::FgDimmed);
    }
    if (val.isObject()) {
        if (depth > 3)
            return QStringLiteral("<span style='color:%1;'>{...}</span>")
                       .arg(ChatTheme::FgDimmed);
        return formatJsonObject(val.toObject(), depth, maxLen);
    }
    if (val.isArray()) {
        if (depth > 3)
            return QStringLiteral("<span style='color:%1;'>[...]</span>")
                       .arg(ChatTheme::FgDimmed);
        return formatJsonArray(val.toArray(), depth, maxLen);
    }
    return QStringLiteral("?");
}

QString ChatToolInvocationWidget::formatTerminalOutput(const QString &text)
{
    // Limit to ~40 lines of output
    QStringList lines = text.split(QLatin1Char('\n'));
    const int maxLines = 40;
    bool truncated = false;
    if (lines.size() > maxLines) {
        truncated = true;
        lines = lines.mid(0, maxLines);
    }

    QStringList htmlLines;
    for (const QString &line : lines) {
        const QString escaped = line.toHtmlEscaped().left(500);
        // Highlight error-like lines
        if (line.contains(QLatin1String("error"), Qt::CaseInsensitive)
            || line.contains(QLatin1String("Error"))
            || line.startsWith(QLatin1String("E "))) {
            htmlLines << QStringLiteral("<span style='color:%1;'>%2</span>")
                             .arg(ChatTheme::ErrorFg, escaped);
        } else if (line.contains(QLatin1String("warning"), Qt::CaseInsensitive)
                   || line.contains(QLatin1String("Warning"))) {
            htmlLines << QStringLiteral("<span style='color:%1;'>%2</span>")
                             .arg(ChatTheme::WarningFg, escaped);
        } else if (line.startsWith(QLatin1Char('+'))) {
            htmlLines << QStringLiteral("<span style='color:%1;'>%2</span>")
                             .arg(ChatTheme::DiffInsertFg, escaped);
        } else if (line.startsWith(QLatin1Char('-'))) {
            htmlLines << QStringLiteral("<span style='color:%1;'>%2</span>")
                             .arg(ChatTheme::DiffDeleteFg, escaped);
        } else {
            htmlLines << escaped;
        }
    }

    if (truncated) {
        htmlLines << QStringLiteral("<span style='color:%1;'>\u2026 "
                                    "(%2 more lines)</span>")
                         .arg(ChatTheme::FgDimmed)
                         .arg(text.split(QLatin1Char('\n')).size() - maxLines);
    }

    return htmlLines.join(QStringLiteral("<br>"));
}

QString ChatToolInvocationWidget::keyValueLine(const QString &key, const QString &value)
{
    if (value.isEmpty()) return {};
    return QStringLiteral("<span style='color:%1;'>%2:</span> %3<br>")
               .arg(ChatTheme::FgDimmed, key.toHtmlEscaped(),
                    value.toHtmlEscaped());
}

QString ChatToolInvocationWidget::diffLine(const QString &text, bool isInsert)
{
    const QString color = isInsert ? ChatTheme::DiffInsertFg
                                   : ChatTheme::DiffDeleteFg;
    const QString prefix = isInsert ? QStringLiteral("+")
                                    : QStringLiteral("-");
    return QStringLiteral("<span style='color:%1;'>%2 %3</span><br>")
               .arg(color, prefix, text.toHtmlEscaped());
}

QString ChatToolInvocationWidget::limitText(const QString &text, int max)
{
    if (text.length() <= max) return text;
    return text.left(max - 1) + QStringLiteral("\u2026");
}

QString ChatToolInvocationWidget::escapeAndLimit(const QString &text, int maxChars)
{
    QStringList lines = text.left(maxChars).split(QLatin1Char('\n'));
    QStringList escaped;
    for (const auto &l : lines)
        escaped << l.toHtmlEscaped();
    QString result = escaped.join(QStringLiteral("<br>"));
    if (text.length() > maxChars)
        result += QStringLiteral("<br><span style='color:%1;'>\u2026</span>")
                      .arg(ChatTheme::FgDimmed);
    return result;
}

QString ChatToolInvocationWidget::iconForTool(const QString &toolName)
{
    if (toolName.contains(QLatin1String("grep"))
        || toolName.contains(QLatin1String("search")))
        return QStringLiteral("\U0001F50D");
    if (toolName.contains(QLatin1String("read")))
        return QStringLiteral("\U0001F4C4");
    if (toolName.contains(QLatin1String("list")))
        return QStringLiteral("\U0001F4C2");
    if (toolName.contains(QLatin1String("create")))
        return QStringLiteral("\u2795");
    if (toolName.contains(QLatin1String("replace"))
        || toolName.contains(QLatin1String("edit")))
        return QStringLiteral("\u270F\uFE0F");
    if (toolName.contains(QLatin1String("terminal"))
        || toolName.contains(QLatin1String("run")))
        return QStringLiteral("\u25B8");
    if (toolName.contains(QLatin1String("think")))
        return QStringLiteral("\U0001F4A1");
    if (toolName.contains(QLatin1String("git")))
        return QStringLiteral("\u2442");
    return QStringLiteral("\u2699");
}

bool ChatToolInvocationWidget::isTerminalTool() const
{
    return m_toolName == QLatin1String("run_in_terminal")
        || m_toolName == QLatin1String("run_command")
        || m_toolName == QLatin1String("get_terminal_output");
}

void ChatToolInvocationWidget::startSpinner()
{
    m_spinIndex = 0;
    m_spinTimer = new QTimer(this);
    connect(m_spinTimer, &QTimer::timeout, this, [this]() {
        m_spinIndex = (m_spinIndex + 1) % 4;
        static const QString frames[] = {
            QStringLiteral("\u25DC"), QStringLiteral("\u25DD"),
            QStringLiteral("\u25DE"), QStringLiteral("\u25DF")
        };
        m_stateBadge->setText(frames[m_spinIndex] + QStringLiteral(" running\u2026"));
    });
    m_spinTimer->start(150);
}

QString ChatToolInvocationWidget::spinnerFrame()
{
    return QStringLiteral("\u25DC");
}

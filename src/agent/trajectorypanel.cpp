#include "trajectorypanel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

TrajectoryPanel::TrajectoryPanel(QWidget *parent)
    : QWidget(parent)
    , m_headerLabel(new QLabel(this))
    , m_timeline(new QTextBrowser(this))
    , m_clearBtn(new QToolButton(this))
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    // Header
    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(4, 4, 4, 0);
    m_headerLabel->setText(tr("Agent Trajectory"));
    m_headerLabel->setStyleSheet(QStringLiteral(
        "font-weight: bold; color: #569cd6; font-size: 13px;"));
    topRow->addWidget(m_headerLabel, 1);

    m_clearBtn->setText(tr("Clear"));
    m_clearBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_clearBtn, &QToolButton::clicked, this, &TrajectoryPanel::clear);
    topRow->addWidget(m_clearBtn);
    lay->addLayout(topRow);

    // Timeline browser
    m_timeline->setOpenLinks(false);
    m_timeline->setReadOnly(true);
    m_timeline->document()->setDefaultStyleSheet(QStringLiteral(
        "body { font-family: 'Consolas','Courier New',monospace; font-size: 12px; "
        "       color: #d4d4d4; background: #1e1e1e; margin: 0; }"
        ".step { margin: 4px 8px; padding: 6px 10px; border-left: 3px solid #333; "
        "        border-radius: 2px; }"
        ".step-model  { border-left-color: #569cd6; background: #1a2a3a; }"
        ".step-tool   { border-left-color: #dcdcaa; background: #2a2a1a; }"
        ".step-answer { border-left-color: #4ec9b0; background: #1a2a2a; }"
        ".step-error  { border-left-color: #f44747; background: #2a1a1a; }"
        ".step-num    { color: #666; font-size: 10px; }"
        ".step-type   { font-weight: bold; margin-bottom: 2px; }"
        ".model-type  { color: #569cd6; }"
        ".tool-type   { color: #dcdcaa; }"
        ".answer-type { color: #4ec9b0; }"
        ".error-type  { color: #f44747; }"
        ".detail      { color: #888; font-size: 11px; white-space: pre-wrap; "
        "               max-height: 80px; overflow: hidden; }"
        ".ts          { color: #555; font-size: 10px; float: right; }"
        ".connector   { text-align: center; color: #444; font-size: 10px; margin: 0; }"
    ));
    lay->addWidget(m_timeline);
}

void TrajectoryPanel::clear()
{
    m_timeline->clear();
    m_stepCount = 0;
    m_headerLabel->setText(tr("Agent Trajectory"));
}

void TrajectoryPanel::setTurn(const AgentTurn &turn)
{
    clear();

    if (!turn.userMessage.isEmpty()) {
        m_timeline->append(QStringLiteral(
            "<div class='step' style='border-left-color:#ce9178; background:#2a1e1a;'>"
            "<div class='step-type' style='color:#ce9178;'>&#128100; User</div>"
            "<div class='detail'>%1</div>"
            "</div>").arg(escHtml(turn.userMessage.left(200))));
    }

    for (const auto &step : turn.steps) {
        if (m_stepCount > 0) {
            m_timeline->append(QStringLiteral(
                "<div class='connector'>&#9474;</div>"));
        }
        m_timeline->append(stepToHtml(step, m_stepCount));
        ++m_stepCount;
    }

    m_headerLabel->setText(tr("Trajectory — %1 step(s)").arg(m_stepCount));
}

void TrajectoryPanel::appendStep(const AgentStep &step)
{
    if (m_stepCount > 0) {
        m_timeline->append(QStringLiteral(
            "<div class='connector'>&#9474;</div>"));
    }
    m_timeline->append(stepToHtml(step, m_stepCount));
    ++m_stepCount;
    m_headerLabel->setText(tr("Trajectory — %1 step(s)").arg(m_stepCount));
}

QString TrajectoryPanel::stepToHtml(const AgentStep &step, int index) const
{
    QString cssClass, typeIcon, typeName, detail;

    switch (step.type) {
    case AgentStep::Type::ModelCall:
        cssClass  = QStringLiteral("step-model");
        typeIcon  = QStringLiteral("&#129302;");  // robot
        typeName  = QStringLiteral("Model Call");
        detail    = QStringLiteral("Request: %1\nResponse: %2")
            .arg(step.modelRequest.left(150),
                 step.modelResponse.left(150));
        break;

    case AgentStep::Type::ToolCall:
        cssClass  = QStringLiteral("step-tool");
        typeIcon  = QStringLiteral("&#9881;");   // gear
        typeName  = QStringLiteral("Tool: %1").arg(step.toolCall.name);
        {
            QJsonDocument argsDoc = QJsonDocument::fromJson(step.toolCall.arguments.toUtf8());
            QString argsPreview = argsDoc.isNull()
                ? step.toolCall.arguments.left(120)
                : QString::fromUtf8(argsDoc.toJson(QJsonDocument::Compact)).left(120);
            detail = QStringLiteral("Args: %1\nResult: %2 %3")
                .arg(argsPreview,
                     step.toolSuccess ? QStringLiteral("✓") : QStringLiteral("✗"),
                     step.toolResult.left(120));
        }
        break;

    case AgentStep::Type::FinalAnswer:
        cssClass  = QStringLiteral("step-answer");
        typeIcon  = QStringLiteral("&#10003;");  // checkmark
        typeName  = QStringLiteral("Final Answer");
        detail    = step.finalText.left(200);
        if (!step.patches.isEmpty())
            detail += QStringLiteral("\n[%1 patch(es)]").arg(step.patches.size());
        break;

    case AgentStep::Type::Error:
        cssClass  = QStringLiteral("step-error");
        typeIcon  = QStringLiteral("&#10007;");  // X mark
        typeName  = QStringLiteral("Error");
        detail    = step.errorMessage;
        break;
    }

    const QString typeClass = cssClass.mid(5);  // "model", "tool", "answer", "error"

    return QStringLiteral(
        "<div class='step %1'>"
        "<span class='step-num'>#%2</span>"
        "<span class='ts'>%3</span>"
        "<div class='step-type %4-type'>%5 %6</div>"
        "<div class='detail'>%7</div>"
        "</div>")
        .arg(cssClass)
        .arg(index + 1)
        .arg(step.timestamp.toHtmlEscaped())
        .arg(typeClass)
        .arg(typeIcon, typeName.toHtmlEscaped())
        .arg(escHtml(detail));
}

QString TrajectoryPanel::escHtml(const QString &s) const
{
    return s.toHtmlEscaped();
}

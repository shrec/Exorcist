#include "nexteditengine.h"
#include "editorview.h"
#include "../aiinterface.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTextBlock>
#include <QTextCursor>
#include <QUuid>

NextEditEngine::NextEditEngine(QObject *parent)
    : QObject(parent)
{
    m_cooldown.setSingleShot(true);
    m_cooldown.setInterval(300);
    connect(&m_cooldown, &QTimer::timeout, this, [this] {
        if (!m_lastAcceptedText.isEmpty())
            requestPrediction(m_lastAcceptedText);
    });
}

void NextEditEngine::setProvider(IAgentProvider *provider)
{
    disconnect(m_deltaConn);
    disconnect(m_finishConn);
    disconnect(m_errorConn);
    m_provider = provider;
}

void NextEditEngine::attachEditor(EditorView *editor, const QString &filePath)
{
    detachEditor();
    m_editor   = editor;
    m_filePath = filePath;
}

void NextEditEngine::detachEditor()
{
    dismissSuggestion();
    m_cooldown.stop();
    m_editor = nullptr;
    m_filePath.clear();
}

void NextEditEngine::onEditAccepted(const QString &acceptedText,
                                    int atLine, int atColumn)
{
    if (!m_provider || !m_editor || acceptedText.trimmed().isEmpty())
        return;

    m_lastAcceptedText   = acceptedText;
    m_lastAcceptedLine   = atLine;
    m_lastAcceptedColumn = atColumn;

    // Brief cooldown to avoid flooding
    m_cooldown.start();
}

void NextEditEngine::acceptSuggestion()
{
    if (!hasSuggestion() || !m_editor)
        return;

    if (!m_suggestion.filePath.isEmpty() && m_suggestion.filePath != m_filePath) {
        // Multi-file suggestion — navigate to the other file
        emit navigateToFile(m_suggestion.filePath,
                            m_suggestion.line, m_suggestion.column);
        dismissSuggestion();
        return;
    }

    // Jump cursor to the predicted location
    QTextBlock block = m_editor->document()->findBlockByNumber(m_suggestion.line);
    if (!block.isValid()) {
        dismissSuggestion();
        return;
    }

    QTextCursor cur(block);
    int col = qMin(m_suggestion.column, block.length() - 1);
    cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, col);
    m_editor->setTextCursor(cur);
    m_editor->centerCursor();

    // Show the suggestion as ghost text
    if (!m_suggestion.fullSuggestion.isEmpty())
        m_editor->setGhostText(m_suggestion.fullSuggestion);

    dismissSuggestion();
}

void NextEditEngine::dismissSuggestion()
{
    if (m_suggestion.line >= 0) {
        m_suggestion = {};
        emit suggestionDismissed();
    }
    m_streamBuffer.clear();

    // Cancel any in-flight request
    if (!m_pendingRequestId.isEmpty() && m_provider) {
        m_provider->cancelRequest(m_pendingRequestId);
        m_pendingRequestId.clear();
    }
    disconnect(m_deltaConn);
    disconnect(m_finishConn);
    disconnect(m_errorConn);
}

void NextEditEngine::requestPrediction(const QString &editContext)
{
    if (!m_provider || !m_editor)
        return;

    // Cancel any previous NES request
    if (!m_pendingRequestId.isEmpty())
        m_provider->cancelRequest(m_pendingRequestId);

    disconnect(m_deltaConn);
    disconnect(m_finishConn);
    disconnect(m_errorConn);

    // Gather surrounding context (±30 lines around where the edit happened)
    const QString allText = m_editor->toPlainText();
    const QStringList lines = allText.split(QLatin1Char('\n'));
    const int startLine = qMax(0, m_lastAcceptedLine - 30);
    const int endLine   = qMin(lines.size() - 1, m_lastAcceptedLine + 30);
    QStringList contextLines;
    for (int i = startLine; i <= endLine; ++i)
        contextLines << lines[i];

    const QString surroundingCode = contextLines.join(QLatin1Char('\n'));

    // Build a lightweight NES prompt
    const QString prompt = QStringLiteral(
        "The user just accepted an inline completion in their code editor.\n"
        "File: %1 (line %2, column %3)\n"
        "Accepted text: \"%4\"\n\n"
        "Surrounding code (lines %5-%6):\n```\n%7\n```\n\n"
        "Based on this edit, predict the NEXT logical edit the user needs to make "
        "in the same file. Respond in this exact format:\n"
        "LINE: <0-based line number>\n"
        "COLUMN: <0-based column>\n"
        "SUGGESTION: <the code to insert or replace>\n\n"
        "If no logical next edit exists, respond with:\nNONE\n"
    ).arg(m_filePath)
     .arg(m_lastAcceptedLine)
     .arg(m_lastAcceptedColumn)
     .arg(editContext.left(200))
     .arg(startLine)
     .arg(endLine)
     .arg(surroundingCode);

    AgentRequest req;
    req.requestId = QUuid::createUuid().toString();
    req.intent    = AgentIntent::Chat;
    req.userPrompt = prompt;
    req.conversationHistory.append({AgentMessage::Role::System,
        QStringLiteral("You are a predictive edit assistant. "
                       "Analyze the user's last edit and predict the next "
                       "logical edit location and content. Be concise.")});
    req.conversationHistory.append({AgentMessage::Role::User, prompt});

    m_pendingRequestId = req.requestId;
    m_streamBuffer.clear();

    // Connect response signals for this request
    m_deltaConn = connect(m_provider, &IAgentProvider::responseDelta,
                          this, [this](const QString &id, const QString &chunk) {
        if (id == m_pendingRequestId)
            m_streamBuffer += chunk;
    });

    m_finishConn = connect(m_provider, &IAgentProvider::responseFinished,
                           this, [this](const QString &id, const AgentResponse &resp) {
        if (id != m_pendingRequestId)
            return;
        const QString text = m_streamBuffer.isEmpty() ? resp.text : m_streamBuffer;
        onPredictionResponse(id, text);
    });

    m_errorConn = connect(m_provider, &IAgentProvider::responseError,
                          this, [this](const QString &id, const AgentError &) {
        if (id == m_pendingRequestId) {
            m_pendingRequestId.clear();
            m_streamBuffer.clear();
            disconnect(m_deltaConn);
            disconnect(m_finishConn);
            disconnect(m_errorConn);
        }
    });

    m_provider->sendRequest(req);
}

void NextEditEngine::onPredictionResponse(const QString &requestId,
                                          const QString &text)
{
    if (requestId != m_pendingRequestId)
        return;

    m_pendingRequestId.clear();
    disconnect(m_deltaConn);
    disconnect(m_finishConn);
    disconnect(m_errorConn);

    // Parse response
    const QString trimmed = text.trimmed();
    if (trimmed.contains(QStringLiteral("NONE"), Qt::CaseInsensitive))
        return;

    // Extract LINE, COLUMN, SUGGESTION
    static const QRegularExpression lineRx(QStringLiteral(R"(LINE:\s*(\d+))"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression colRx(QStringLiteral(R"(COLUMN:\s*(\d+))"),
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression sugRx(QStringLiteral(R"(SUGGESTION:\s*(.+))"),
                                          QRegularExpression::CaseInsensitiveOption |
                                          QRegularExpression::DotMatchesEverythingOption);

    auto lineMatch = lineRx.match(trimmed);
    auto colMatch  = colRx.match(trimmed);
    auto sugMatch  = sugRx.match(trimmed);

    if (!lineMatch.hasMatch() || !sugMatch.hasMatch())
        return;

    int line = lineMatch.captured(1).toInt();
    int col  = colMatch.hasMatch() ? colMatch.captured(1).toInt() : 0;
    QString suggestion = sugMatch.captured(1).trimmed();

    // Remove enclosing backticks/code fences from suggestion
    if (suggestion.startsWith(QLatin1Char('`')))
        suggestion.remove(0, suggestion.startsWith(QStringLiteral("```")) ? 3 : 1);
    if (suggestion.endsWith(QLatin1Char('`')))
        suggestion.chop(suggestion.endsWith(QStringLiteral("```")) ? 3 : 1);
    suggestion = suggestion.trimmed();

    if (suggestion.isEmpty())
        return;

    // Validate line number
    if (!m_editor || line < 0 || line >= m_editor->document()->blockCount())
        return;

    m_suggestion.line           = line;
    m_suggestion.column         = col;
    m_suggestion.filePath       = {};   // same file
    m_suggestion.preview        = suggestion.left(60);
    m_suggestion.fullSuggestion = suggestion;

    emit suggestionReady(line);
}

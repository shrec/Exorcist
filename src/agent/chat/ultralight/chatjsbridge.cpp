#ifdef EXORCIST_HAS_ULTRALIGHT

#include "chatjsbridge.h"
#include "ultralightwidget.h"

#include <QJsonDocument>

namespace exorcist {

ChatJSBridge::ChatJSBridge(UltralightWidget *view, QObject *parent)
    : QObject(parent), m_view(view)
{
    // Register JS→C++ message handlers
    auto reg = [this](const QString &type, auto handler) {
        m_view->registerJSCallback(type, [this, handler](const QJsonValue &v) {
            (this->*handler)(v);
        });
    };

    // Each JS message type maps to a signal emission
    m_view->registerJSCallback(QStringLiteral("followupClicked"),
        [this](const QJsonValue &v) {
            emit followupClicked(v.toObject().value(QStringLiteral("message")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("feedbackGiven"),
        [this](const QJsonValue &v) {
            const auto obj = v.toObject();
            emit feedbackGiven(obj.value(QStringLiteral("turnId")).toString(),
                               obj.value(QStringLiteral("helpful")).toBool());
        });

    m_view->registerJSCallback(QStringLiteral("toolConfirmed"),
        [this](const QJsonValue &v) {
            const auto obj = v.toObject();
            emit toolConfirmed(obj.value(QStringLiteral("callId")).toString(),
                               obj.value(QStringLiteral("approval")).toInt());
        });

    m_view->registerJSCallback(QStringLiteral("fileClicked"),
        [this](const QJsonValue &v) {
            emit fileClicked(v.toObject().value(QStringLiteral("path")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("insertCode"),
        [this](const QJsonValue &v) {
            emit insertCodeRequested(v.toObject().value(QStringLiteral("code")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("retryRequested"),
        [this](const QJsonValue &v) {
            emit retryRequested(v.toObject().value(QStringLiteral("turnId")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("suggestionClicked"),
        [this](const QJsonValue &v) {
            emit suggestionClicked(v.toObject().value(QStringLiteral("message")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("signInRequested"),
        [this](const QJsonValue &) { emit signInRequested(); });

    m_view->registerJSCallback(QStringLiteral("keepFile"),
        [this](const QJsonValue &v) {
            const auto obj = v.toObject();
            emit keepFileRequested(obj.value(QStringLiteral("index")).toInt(),
                                   obj.value(QStringLiteral("path")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("undoFile"),
        [this](const QJsonValue &v) {
            const auto obj = v.toObject();
            emit undoFileRequested(obj.value(QStringLiteral("index")).toInt(),
                                   obj.value(QStringLiteral("path")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("keepAll"),
        [this](const QJsonValue &) { emit keepAllRequested(); });

    m_view->registerJSCallback(QStringLiteral("undoAll"),
        [this](const QJsonValue &) { emit undoAllRequested(); });

    m_view->registerJSCallback(QStringLiteral("copyCode"),
        [this](const QJsonValue &v) {
            emit copyCodeRequested(v.toObject().value(QStringLiteral("code")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("copyText"),
        [this](const QJsonValue &v) {
            emit copyTextRequested(v.toObject().value(QStringLiteral("text")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("applyCode"),
        [this](const QJsonValue &v) {
            const auto obj = v.toObject();
            emit applyCodeRequested(obj.value(QStringLiteral("code")).toString(),
                                    obj.value(QStringLiteral("language")).toString(),
                                    obj.value(QStringLiteral("filePath")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("runCode"),
        [this](const QJsonValue &v) {
            const auto obj = v.toObject();
            emit runCodeRequested(obj.value(QStringLiteral("code")).toString(),
                                  obj.value(QStringLiteral("language")).toString());
        });

    // ── Input area signals ──────────────────────────────────────────────────

    m_view->registerJSCallback(QStringLiteral("sendRequested"),
        [this](const QJsonValue &v) {
            const auto obj = v.toObject();
            emit sendRequested(obj.value(QStringLiteral("text")).toString(),
                               obj.value(QStringLiteral("mode")).toInt());
        });

    m_view->registerJSCallback(QStringLiteral("cancelRequested"),
        [this](const QJsonValue &) { emit cancelRequested(); });

    m_view->registerJSCallback(QStringLiteral("modeChanged"),
        [this](const QJsonValue &v) {
            emit modeChanged(v.toObject().value(QStringLiteral("mode")).toInt());
        });

    m_view->registerJSCallback(QStringLiteral("modelSelected"),
        [this](const QJsonValue &v) {
            emit modelSelected(v.toObject().value(QStringLiteral("modelId")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("newSessionRequested"),
        [this](const QJsonValue &) { emit newSessionRequested(); });

    m_view->registerJSCallback(QStringLiteral("historyRequested"),
        [this](const QJsonValue &) { emit historyRequested(); });

    m_view->registerJSCallback(QStringLiteral("settingsRequested"),
        [this](const QJsonValue &) { emit settingsRequested(); });

    m_view->registerJSCallback(QStringLiteral("attachFileRequested"),
        [this](const QJsonValue &) { emit attachFileRequested(); });

    m_view->registerJSCallback(QStringLiteral("removeAttachment"),
        [this](const QJsonValue &v) {
            emit removeAttachmentRequested(
                v.toObject().value(QStringLiteral("index")).toInt());
        });

    m_view->registerJSCallback(QStringLiteral("thinkingToggled"),
        [this](const QJsonValue &v) {
            emit thinkingToggled(
                v.toObject().value(QStringLiteral("enabled")).toBool());
        });

    m_view->registerJSCallback(QStringLiteral("mentionQuery"),
        [this](const QJsonValue &v) {
            const auto obj = v.toObject();
            emit mentionQueryRequested(
                obj.value(QStringLiteral("trigger")).toString(),
                obj.value(QStringLiteral("filter")).toString());
        });

    m_view->registerJSCallback(QStringLiteral("openExternalUrl"),
        [this](const QJsonValue &v) {
            const QString url = v.toObject().value(QStringLiteral("url")).toString();
            if (!url.isEmpty())
                emit openExternalUrlRequested(url);
        });
}

// ── C++ → JS ─────────────────────────────────────────────────────────────────

void ChatJSBridge::addTurn(int turnIndex, const QJsonObject &turnJson)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(turnJson).toJson(QJsonDocument::Compact));
    callJS(QStringLiteral("ChatApp.addTurn(%1, %2)").arg(turnIndex).arg(json));
}

void ChatJSBridge::appendMarkdownDelta(int turnIndex, const QString &delta)
{
    callJS(QStringLiteral("ChatApp.appendMarkdownDelta(%1, %2)")
               .arg(turnIndex)
               .arg(jsonEscape(delta)));
}

void ChatJSBridge::appendThinkingDelta(int turnIndex, const QString &delta)
{
    callJS(QStringLiteral("ChatApp.appendThinkingDelta(%1, %2)")
               .arg(turnIndex)
               .arg(jsonEscape(delta)));
}

void ChatJSBridge::addContentPart(int turnIndex, const QJsonObject &partJson)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(partJson).toJson(QJsonDocument::Compact));
    callJS(QStringLiteral("ChatApp.addContentPart(%1, %2)").arg(turnIndex).arg(json));
}

void ChatJSBridge::updateToolState(int turnIndex, const QString &callId,
                                   const QJsonObject &stateJson)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(stateJson).toJson(QJsonDocument::Compact));
    callJS(QStringLiteral("ChatApp.updateToolState(%1, %2, %3)")
               .arg(turnIndex)
               .arg(jsonEscape(callId))
               .arg(json));
}

void ChatJSBridge::finishTurn(int turnIndex, int state)
{
    callJS(QStringLiteral("ChatApp.finishTurn(%1, %2)").arg(turnIndex).arg(state));
}

void ChatJSBridge::setTokenUsage(int turnIndex, int promptTokens,
                                 int completionTokens, int totalTokens)
{
    callJS(QStringLiteral("ChatApp.setTokenUsage(%1, %2, %3, %4)")
               .arg(turnIndex).arg(promptTokens)
               .arg(completionTokens).arg(totalTokens));
}

void ChatJSBridge::clearSession()
{
    callJS(QStringLiteral("ChatApp.clearSession()"));
}

// ── View state ───────────────────────────────────────────────────────────────

void ChatJSBridge::showWelcome(const QString &welcomeState)
{
    callJS(QStringLiteral("ChatApp.showWelcome(%1)").arg(jsonEscape(welcomeState)));
}

void ChatJSBridge::showTranscript()
{
    callJS(QStringLiteral("ChatApp.showTranscript()"));
}

void ChatJSBridge::setStreamingActive(bool active)
{
    callJS(QStringLiteral("ChatApp.setStreamingActive(%1)")
               .arg(active ? QStringLiteral("true") : QStringLiteral("false")));
}

void ChatJSBridge::scrollToBottom()
{
    callJS(QStringLiteral("ChatApp.scrollToBottom()"));
}

// ── Theme ────────────────────────────────────────────────────────────────────

void ChatJSBridge::setTheme(const QJsonObject &themeTokens)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(themeTokens).toJson(QJsonDocument::Compact));
    callJS(QStringLiteral("ChatApp.setTheme(%1)").arg(json));
}

void ChatJSBridge::setWelcomeSuggestions(const QJsonArray &suggestions)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(suggestions).toJson(QJsonDocument::Compact));
    callJS(QStringLiteral("ChatApp.setWelcomeSuggestions(%1)").arg(json));
}

void ChatJSBridge::setToolCount(int count)
{
    callJS(QStringLiteral("ChatApp.setToolCount(%1)").arg(count));
}

// ── Input Area Control ───────────────────────────────────────────────────────

void ChatJSBridge::setInputText(const QString &text)
{
    callJS(QStringLiteral("ChatApp.setInputText(%1)").arg(jsonEscape(text)));
}

void ChatJSBridge::focusInput()
{
    callJS(QStringLiteral("ChatApp.focusInput()"));
}

void ChatJSBridge::clearInput()
{
    callJS(QStringLiteral("ChatApp.clearInput()"));
}

void ChatJSBridge::setStreamingState(bool streaming)
{
    callJS(QStringLiteral("ChatApp.setStreamingState(%1)")
               .arg(streaming ? QStringLiteral("true") : QStringLiteral("false")));
}

void ChatJSBridge::setInputEnabled(bool enabled)
{
    callJS(QStringLiteral("ChatApp.setInputEnabled(%1)")
               .arg(enabled ? QStringLiteral("true") : QStringLiteral("false")));
}

void ChatJSBridge::setSlashCommands(const QJsonArray &commands)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(commands).toJson(QJsonDocument::Compact));
    callJS(QStringLiteral("ChatApp.setSlashCommands(%1)").arg(json));
}

void ChatJSBridge::setMentionItems(const QString &trigger, const QJsonArray &items)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(items).toJson(QJsonDocument::Compact));
    callJS(QStringLiteral("ChatApp.setMentionItems(%1, %2)")
               .arg(jsonEscape(trigger))
               .arg(json));
}

void ChatJSBridge::setMode(int mode)
{
    callJS(QStringLiteral("ChatApp.setMode(%1)").arg(mode));
}

void ChatJSBridge::setModels(const QJsonArray &modelList)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(modelList).toJson(QJsonDocument::Compact));
    callJS(QStringLiteral("ChatApp.setModels(%1)").arg(json));
}

void ChatJSBridge::setCurrentModel(const QString &id)
{
    callJS(QStringLiteral("ChatApp.setCurrentModel(%1)").arg(jsonEscape(id)));
}

void ChatJSBridge::clearModels()
{
    callJS(QStringLiteral("ChatApp.clearModels()"));
}

void ChatJSBridge::addModel(const QJsonObject &modelInfo)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(modelInfo).toJson(QJsonDocument::Compact));
    callJS(QStringLiteral("ChatApp.addModel(%1)").arg(json));
}

void ChatJSBridge::setThinkingVisible(bool visible)
{
    callJS(QStringLiteral("ChatApp.setThinkingVisible(%1)")
               .arg(visible ? QStringLiteral("true") : QStringLiteral("false")));
}

// ── Header / Changes Bar ─────────────────────────────────────────────────────

void ChatJSBridge::setSessionTitle(const QString &title)
{
    callJS(QStringLiteral("ChatApp.setSessionTitle(%1)").arg(jsonEscape(title)));
}

void ChatJSBridge::showChangesBar(int editCount)
{
    callJS(QStringLiteral("ChatApp.showChangesBar(%1)").arg(editCount));
}

void ChatJSBridge::hideChangesBar()
{
    callJS(QStringLiteral("ChatApp.hideChangesBar()"));
}

// ── Attachment Chips ─────────────────────────────────────────────────────────

void ChatJSBridge::addAttachmentChip(const QString &name, int index)
{
    callJS(QStringLiteral("ChatApp.addAttachmentChip(%1, %2)")
               .arg(jsonEscape(name)).arg(index));
}

void ChatJSBridge::removeAttachmentChip(int index)
{
    callJS(QStringLiteral("ChatApp.removeAttachmentChip(%1)").arg(index));
}

void ChatJSBridge::clearAttachmentChips()
{
    callJS(QStringLiteral("ChatApp.clearAttachmentChips()"));
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void ChatJSBridge::callJS(const QString &functionCall)
{
    m_view->evaluateScript(functionCall);
}

QString ChatJSBridge::jsonEscape(const QString &s)
{
    // Use QJsonDocument to properly escape the string as a JSON value
    QJsonArray arr;
    arr.append(s);
    const QString json = QString::fromUtf8(
        QJsonDocument(arr).toJson(QJsonDocument::Compact));
    // json is like: ["escaped string"]  — extract just the string part
    return json.mid(1, json.size() - 2); // gives "escaped string" with quotes
}

} // namespace exorcist

#endif // EXORCIST_HAS_ULTRALIGHT

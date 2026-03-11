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

    m_view->registerJSCallback(QStringLiteral("applyCode"),
        [this](const QJsonValue &v) {
            const auto obj = v.toObject();
            emit applyCodeRequested(obj.value(QStringLiteral("code")).toString(),
                                    obj.value(QStringLiteral("language")).toString());
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

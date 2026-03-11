#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

class EditorView;
class IAgentProvider;

// Bridges between EditorView typing and an AI provider's inline completion.
// Debounces text changes, requests completions, and displays ghost text.

class InlineCompletionEngine : public QObject
{
    Q_OBJECT

public:
    explicit InlineCompletionEngine(QObject *parent = nullptr);

    void attachEditor(EditorView *editor, const QString &filePath,
                      const QString &languageId);
    void detachEditor();

    void setProvider(IAgentProvider *provider);
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // Per-language enable/disable
    void setDisabledLanguages(const QSet<QString> &langs) { m_disabledLangs = langs; }
    QSet<QString> disabledLanguages() const { return m_disabledLangs; }
    bool isLanguageEnabled(const QString &langId) const { return !m_disabledLangs.contains(langId.toLower()); }

    // Manual trigger (Alt+\) — request completion immediately
    void triggerCompletion();

    // Separate model for inline completions (empty = use provider's current model)
    void setCompletionModel(const QString &model) { m_completionModel = model; }
    QString completionModel() const { return m_completionModel; }

private Q_SLOTS:
    void onCompletionReady(const QString &suggestion);

private:
    void onTextChanged();
    void onCursorChanged();
    void requestCompletion();

    EditorView      *m_editor   = nullptr;
    IAgentProvider  *m_provider = nullptr;
    QTimer           m_debounce;

    QString m_filePath;
    QString m_languageId;
    bool    m_enabled       = true;
    bool    m_waitingReply  = false;
    QSet<QString> m_disabledLangs;
    QString       m_completionModel;

    QMetaObject::Connection m_textConn;
    QMetaObject::Connection m_cursorConn;
    QMetaObject::Connection m_acceptConn;
    QMetaObject::Connection m_dismissConn;
    QMetaObject::Connection m_completionConn;
};

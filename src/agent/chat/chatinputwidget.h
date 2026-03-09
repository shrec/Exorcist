#pragma once

#include <QList>
#include <QWidget>

#include <functional>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QToolButton;
class QVBoxLayout;
class AgentOrchestrator;

// ── ChatInputWidget ───────────────────────────────────────────────────────────
//
// Self-contained input area for the chat panel, extracted from AgentChatPanel.
// Contains:
//   - Multiline text editor with auto-resize
//   - Send / Cancel buttons
//   - Attach button
//   - Mode selector (Ask / Edit / Agent)
//   - Model selector combo
//   - Context usage button
//   - New Session / History buttons
//   - Slash-command autocomplete popup
//   - @-mention / #-file autocomplete popup
//   - Attachment chips bar
//
// The widget emits signals; it does NOT directly talk to the orchestrator.

class ChatInputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatInputWidget(QWidget *parent = nullptr);

    // ── Accessors ─────────────────────────────────────────────────────────
    QString text() const;
    void    setText(const QString &text);
    void    setInputText(const QString &text) { setText(text); }
    void    clearText();
    void    clear() { clearText(); clearAttachments(); }
    void    focusInput();

    int  currentMode() const { return m_currentMode; }
    void setCurrentMode(int mode);

    // ── Attachment management ─────────────────────────────────────────────
    struct Attachment {
        QString    path;
        QByteArray data;
        QString    name;
        bool       isImage = false;
    };
    const QList<Attachment> &attachments() const { return m_attachments; }
    void addAttachment(const QString &path);
    void attachSelection(const QString &text, const QString &filePath, int startLine);
    void attachDiagnostics(const QString &summary, int count);
    void clearAttachments();

    // ── Model combo management ────────────────────────────────────────────
    QComboBox *modelCombo() const { return m_modelCombo; }
    void setModels(const QStringList &models, const QString &current);
    void clearModels();
    void addModel(const QString &id, const QString &displayName,
                  bool isPremium = false, double multiplier = 1.0);
    void setCurrentModel(const QString &id);
    QString selectedModel() const;

    // ── Input state ───────────────────────────────────────────────────────
    void setStreamingState(bool streaming);
    void setStreaming(bool streaming) { setStreamingState(streaming); }
    void setInputEnabled(bool enabled);

    // ── Slash commands ────────────────────────────────────────────────────
    void setSlashCommands(const QStringList &commands);
    void addDynamicSlashCommands(const QStringList &commands);

    // ── Workspace file provider ───────────────────────────────────────────
    void setWorkspaceFileProvider(std::function<QStringList()> fn);

    // ── Context info ──────────────────────────────────────────────────────
    void setContextInfo(const QString &info);
    void setContextTokenCount(int tokens);

signals:
    void sendRequested(const QString &text, int mode);
    void cancelRequested();
    void modeChanged(int mode);          // 0=Ask, 1=Edit, 2=Agent
    void modelSelected(const QString &modelId);
    void newSessionRequested();
    void historyRequested();
    void contextPopupRequested();
    void attachFileRequested();
    void followupClicked(const QString &prompt);

private slots:
    void onSendClicked();
    void onTextChanged();

private:
    void setupUi();
    void showSlashMenu();
    void hideSlashMenu();
    void showMentionMenu(const QString &trigger, const QString &filter);
    void hideMentionMenu();
    void rebuildAttachChips();
    void setModeButtonActive(int mode);
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void dragEnterEvent(QDragEnterEvent *ev) override;
    void dropEvent(QDropEvent *ev) override;

    // ── Widgets ───────────────────────────────────────────────────────────
    QPlainTextEdit *m_input      = nullptr;
    QToolButton    *m_sendBtn    = nullptr;
    QToolButton    *m_cancelBtn  = nullptr;
    QToolButton    *m_attachBtn  = nullptr;

    // Mode
    QComboBox      *m_modeCombo  = nullptr;
    QToolButton    *m_askBtn     = nullptr;
    QToolButton    *m_editBtn    = nullptr;
    QToolButton    *m_agentBtn   = nullptr;
    int             m_currentMode = 0;

    // Bottom bar
    QToolButton    *m_newSessionBtn = nullptr;
    QToolButton    *m_historyBtn    = nullptr;
    QComboBox      *m_modelCombo    = nullptr;
    QToolButton    *m_ctxBtn        = nullptr;

    // Context strip
    QLabel         *m_contextStrip  = nullptr;

    // Autocomplete popups
    QListWidget    *m_slashMenu     = nullptr;
    QListWidget    *m_mentionMenu   = nullptr;
    QString         m_mentionTrigger;

    // Attachment bar
    QWidget        *m_attachBar     = nullptr;
    QHBoxLayout    *m_attachLayout  = nullptr;
    QList<Attachment> m_attachments;

    // Input history
    QStringList     m_inputHistory;
    int             m_historyIndex  = -1;

    // Workspace file provider
    std::function<QStringList()> m_workspaceFileFn;

    // Input block + streaming bar
    QWidget      *m_inputBlock    = nullptr;
    QProgressBar *m_streamingBar  = nullptr;

    int m_staticSlashCount = 0;
};

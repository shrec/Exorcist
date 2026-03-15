#pragma once

#include <QEvent>
#include <QWidget>

#include <functional>

#include "chatthemetokens.h"

class QVBoxLayout;

// Helper: makes a QWidget clickable via mouse press event filter
class PillClickFilter : public QObject
{
    Q_OBJECT
public:
    PillClickFilter(QObject *parent, std::function<void()> callback)
        : QObject(parent), m_callback(std::move(callback)) {}

    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    std::function<void()> m_callback;
};

class ChatWelcomeWidget : public QWidget
{
    Q_OBJECT

public:
    enum class State { Default, AuthRequired, RateLimited, Offline, NoProvider };

    explicit ChatWelcomeWidget(QWidget *parent = nullptr);

    void showState(State state);

signals:
    void suggestionClicked(const QString &message);
    void signInRequested();
    void retryRequested();

private:
    void clearLayout();
    void buildDefaultWelcome(bool disabled = false);
    void buildBannerState(const QString &icon, const QString &title,
                          const QString &message, const char *accentColor,
                          State state = State::Default);

    QVBoxLayout *m_layout = nullptr;
};

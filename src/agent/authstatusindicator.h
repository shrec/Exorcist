#pragma once

#include <QObject>
#include <QString>

class QLabel;
class QStatusBar;

// ─────────────────────────────────────────────────────────────────────────────
// AuthStatusIndicator — shows auth state in the statusbar.
//
// States: SignedOut, SigningIn, SignedIn, Expired, RateLimited, Offline.
// Updates the label icon + tooltip to reflect current state.
// ─────────────────────────────────────────────────────────────────────────────

class AuthStatusIndicator : public QObject
{
    Q_OBJECT

public:
    enum State {
        SignedOut,
        SigningIn,
        SignedIn,
        Expired,
        RateLimited,
        Offline,
    };

    explicit AuthStatusIndicator(QStatusBar *statusBar, QObject *parent = nullptr);

    State state() const { return m_state; }
    void setState(State s);
    void setRateLimitRetry(int seconds);
    QLabel *label() const { return m_label; }

signals:
    void stateChanged(int state);
    void clicked();

private:
    QStatusBar *m_statusBar;
    QLabel *m_label;
    State m_state = SignedOut;
};

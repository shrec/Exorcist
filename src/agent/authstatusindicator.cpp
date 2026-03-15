#include "authstatusindicator.h"

#include <QLabel>
#include <QStatusBar>
#include <QTimer>

AuthStatusIndicator::AuthStatusIndicator(QStatusBar *statusBar, QObject *parent)
    : QObject(parent), m_statusBar(statusBar)
{
    m_label = new QLabel(statusBar);
    m_label->setTextFormat(Qt::RichText);
    m_label->setCursor(Qt::PointingHandCursor);
    statusBar->addPermanentWidget(m_label);
    setState(SignedOut);
}

void AuthStatusIndicator::setState(State s)
{
    m_state = s;
    switch (s) {
    case SignedOut:
        m_label->setText(QStringLiteral("\u26AA Copilot"));
        m_label->setToolTip(tr("Not signed in — click to sign in"));
        break;
    case SigningIn:
        m_label->setText(QStringLiteral("\u23F3 Copilot"));
        m_label->setToolTip(tr("Signing in..."));
        break;
    case SignedIn:
        m_label->setText(QStringLiteral("\U0001F7E2 Copilot"));
        m_label->setToolTip(tr("Signed in"));
        break;
    case Expired:
        m_label->setText(QStringLiteral("\U0001F7E1 Copilot"));
        m_label->setToolTip(tr("Token expired — click to sign in again"));
        break;
    case RateLimited:
        m_label->setText(QStringLiteral("\U0001F534 Copilot"));
        m_label->setToolTip(tr("Rate limited — retry later"));
        break;
    case Offline:
        m_label->setText(QStringLiteral("\u26AB Copilot"));
        m_label->setToolTip(tr("Offline — no network connection"));
        break;
    }
    emit stateChanged(s);
}

void AuthStatusIndicator::setRateLimitRetry(int seconds)
{
    setState(RateLimited);
    m_label->setToolTip(tr("Rate limited — retry in %1s").arg(seconds));
    QTimer::singleShot(seconds * 1000, this, [this]() {
        if (m_state == RateLimited)
            setState(SignedIn);
    });
}

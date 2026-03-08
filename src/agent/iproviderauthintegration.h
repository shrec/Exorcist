#pragma once

#include <QObject>
#include <QString>

// ── IProviderAuthIntegration ─────────────────────────────────────────────────
//
// Optional interface that a provider plugin can implement to expose its
// authentication lifecycle to the core platform.  This lets the IDE show
// unified sign-in/sign-out UI, status indicators, and token-refresh hooks
// regardless of which vendor backend is active.
//
// Usage:
//   auto *auth = qobject_cast<IProviderAuthIntegration *>(plugin);
//   if (auth) {
//       connect(auth->authStatusObject(), SIGNAL(authStatusChanged(int)),
//               statusBar, SLOT(onAuthChanged(int)));
//       if (auth->authStatus() == IProviderAuthIntegration::SignedOut)
//           auth->signIn();
//   }

class IProviderAuthIntegration
{
public:
    virtual ~IProviderAuthIntegration() = default;

    enum AuthStatus {
        Unknown,
        SignedOut,
        SigningIn,
        SignedIn,
        Expired,
        RateLimited,
    };

    /// Current authentication status.
    virtual AuthStatus authStatus() const = 0;

    /// Human-readable status string (e.g. "Signed in as @user").
    virtual QString authStatusText() const = 0;

    /// Initiate sign-in flow (e.g. OAuth device flow, API key dialog).
    virtual void signIn() = 0;

    /// Sign out and clear stored credentials.
    virtual void signOut() = 0;

    /// Attempt to refresh token/credential without user interaction.
    virtual void refreshAuth() = 0;

    /// Return the QObject that emits auth signals. This is typically the
    /// plugin or provider itself.  The platform connects to:
    ///   - authStatusChanged(int status)
    virtual QObject *authStatusObject() = 0;
};

#define EXORCIST_PROVIDER_AUTH_IID "org.exorcist.IProviderAuthIntegration/1.0"
Q_DECLARE_INTERFACE(IProviderAuthIntegration, EXORCIST_PROVIDER_AUTH_IID)

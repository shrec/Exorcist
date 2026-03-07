#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

// ─────────────────────────────────────────────────────────────────────────────
// DomainFilter — allow/deny list for web access from AI tools.
//
// Controls which domains the agent tools can access when fetching URLs.
// Default: allow all. Can be configured to block specific domains or
// allow only specific domains.
// ─────────────────────────────────────────────────────────────────────────────

class DomainFilter : public QObject
{
    Q_OBJECT

public:
    enum class Mode {
        AllowAll,     // allow everything unless explicitly denied
        DenyAll,      // deny everything unless explicitly allowed
    };

    explicit DomainFilter(QObject *parent = nullptr)
        : QObject(parent) {}

    /// Set filter mode
    void setMode(Mode mode) { m_mode = mode; }
    Mode mode() const { return m_mode; }

    /// Add domain to allow list
    void allowDomain(const QString &domain)
    {
        m_allowList.insert(domain.toLower());
    }

    /// Add domain to deny list
    void denyDomain(const QString &domain)
    {
        m_denyList.insert(domain.toLower());
    }

    /// Remove domain from allow list
    void removeAllow(const QString &domain)
    {
        m_allowList.remove(domain.toLower());
    }

    /// Remove domain from deny list
    void removeDeny(const QString &domain)
    {
        m_denyList.remove(domain.toLower());
    }

    /// Check if a URL is allowed
    bool isAllowed(const QUrl &url) const
    {
        const QString host = url.host().toLower();
        if (host.isEmpty()) return false;

        // Check deny list first (always takes precedence)
        if (isDomainInSet(host, m_denyList))
            return false;

        if (m_mode == Mode::AllowAll)
            return true;

        // DenyAll mode: must be in allow list
        return isDomainInSet(host, m_allowList);
    }

    /// Check if a URL string is allowed
    bool isAllowed(const QString &urlStr) const
    {
        return isAllowed(QUrl(urlStr));
    }

    /// Get allow list
    QStringList allowList() const
    {
        return QStringList(m_allowList.begin(), m_allowList.end());
    }

    /// Get deny list
    QStringList denyList() const
    {
        return QStringList(m_denyList.begin(), m_denyList.end());
    }

    /// Clear all lists
    void clear()
    {
        m_allowList.clear();
        m_denyList.clear();
    }

private:
    static bool isDomainInSet(const QString &host, const QSet<QString> &set)
    {
        // Exact match
        if (set.contains(host)) return true;

        // Check parent domains (e.g. "api.github.com" matches "github.com")
        int dot = host.indexOf('.');
        while (dot >= 0) {
            const QString parent = host.mid(dot + 1);
            if (set.contains(parent))
                return true;
            dot = host.indexOf('.', dot + 1);
        }
        return false;
    }

    Mode m_mode = Mode::AllowAll;
    QSet<QString> m_allowList;
    QSet<QString> m_denyList;
};

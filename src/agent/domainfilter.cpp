#include "domainfilter.h"

DomainFilter::DomainFilter(QObject *parent)
    : QObject(parent)
{
}

void DomainFilter::allowDomain(const QString &domain)
{
    m_allowList.insert(domain.toLower());
}

void DomainFilter::denyDomain(const QString &domain)
{
    m_denyList.insert(domain.toLower());
}

void DomainFilter::removeAllow(const QString &domain)
{
    m_allowList.remove(domain.toLower());
}

void DomainFilter::removeDeny(const QString &domain)
{
    m_denyList.remove(domain.toLower());
}

bool DomainFilter::isAllowed(const QUrl &url) const
{
    const QString host = url.host().toLower();
    if (host.isEmpty()) return false;

    if (isDomainInSet(host, m_denyList))
        return false;

    if (m_mode == Mode::AllowAll)
        return true;

    return isDomainInSet(host, m_allowList);
}

bool DomainFilter::isAllowed(const QString &urlStr) const
{
    return isAllowed(QUrl(urlStr));
}

QStringList DomainFilter::allowList() const
{
    return QStringList(m_allowList.begin(), m_allowList.end());
}

QStringList DomainFilter::denyList() const
{
    return QStringList(m_denyList.begin(), m_denyList.end());
}

void DomainFilter::clear()
{
    m_allowList.clear();
    m_denyList.clear();
}

bool DomainFilter::isDomainInSet(const QString &host, const QSet<QString> &set)
{
    if (set.contains(host)) return true;

    int dot = host.indexOf('.');
    while (dot >= 0) {
        const QString parent = host.mid(dot + 1);
        if (set.contains(parent))
            return true;
        dot = host.indexOf('.', dot + 1);
    }
    return false;
}

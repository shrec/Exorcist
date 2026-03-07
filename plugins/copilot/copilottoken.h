#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

// Parses the Copilot API token format: key1=val1;key2=val2;...:mac
// Mirrors the CopilotToken class from the VS Code Copilot Chat extension.

class CopilotToken
{
public:
    CopilotToken() = default;

    explicit CopilotToken(const QString &rawToken, qint64 expiresAt)
        : m_raw(rawToken), m_expiresAt(expiresAt)
    {
        parseTokenFields();
    }

    bool isValid() const { return !m_raw.isEmpty(); }
    bool isExpired(qint64 nowSecs, int marginSecs = 60) const
    {
        return nowSecs >= m_expiresAt - marginSecs;
    }

    QString raw()       const { return m_raw; }
    qint64  expiresAt() const { return m_expiresAt; }

    // Token field accessors (from semicolon-separated key=value pairs)
    QString field(const QString &key) const { return m_fields.value(key); }

    // Convenience accessors matching the TS CopilotToken class
    bool isTelemetryEnabled()      const { return field(QStringLiteral("telemetry")) != QLatin1String("0"); }
    bool isMcpEnabled()            const { return field(QStringLiteral("mcp")) != QLatin1String("0"); }
    bool isCodeReviewEnabled()     const { return field(QStringLiteral("ccr")) == QLatin1String("1"); }
    bool isEditorPreviewEnabled()  const { return field(QStringLiteral("editor_preview_features")) != QLatin1String("0"); }

    // SKU / plan info (set from the full JSON envelope, not the token string)
    QString sku() const { return m_sku; }
    void    setSku(const QString &s) { m_sku = s; }

    QStringList organizationList() const { return m_orgList; }
    void setOrganizationList(const QStringList &list) { m_orgList = list; }

    // Endpoint overrides (SKU-isolated endpoints from token envelope)
    QString apiEndpoint()       const { return m_apiEndpoint; }
    QString telemetryEndpoint() const { return m_telemetryEndpoint; }
    void setApiEndpoint(const QString &url)       { m_apiEndpoint = url; }
    void setTelemetryEndpoint(const QString &url) { m_telemetryEndpoint = url; }

private:
    void parseTokenFields()
    {
        // Format: "key1=val1;key2=val2;...keyN=valN:hmac_signature"
        const int colonIdx = m_raw.indexOf(QLatin1Char(':'));
        const QString fieldsPart = (colonIdx >= 0) ? m_raw.left(colonIdx) : m_raw;
        const QStringList pairs = fieldsPart.split(QLatin1Char(';'));
        for (const QString &pair : pairs) {
            const int eq = pair.indexOf(QLatin1Char('='));
            if (eq > 0)
                m_fields.insert(pair.left(eq), pair.mid(eq + 1));
        }
    }

    QString m_raw;
    qint64  m_expiresAt = 0;
    QMap<QString, QString> m_fields;

    QString     m_sku;
    QStringList m_orgList;
    QString     m_apiEndpoint;
    QString     m_telemetryEndpoint;
};

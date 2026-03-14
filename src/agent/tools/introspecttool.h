#pragma once

#include "../itool.h"

#include <functional>

// ── IntrospectTool ───────────────────────────────────────────────────────────
// Queries the running application state: services, plugins, memory, etc.

class IntrospectTool : public ITool
{
public:
    // Each query handler returns a formatted text result.
    using QueryHandler = std::function<QString(const QString &query)>;

    explicit IntrospectTool(QueryHandler handler);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QueryHandler m_handler;
};

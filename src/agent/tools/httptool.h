#pragma once

#include "../itool.h"

// ── HttpRequestTool ──────────────────────────────────────────────────────────
// Makes HTTP requests (GET/POST/PUT/DELETE). Like curl for the AI agent.
// Returns raw response body, status code, headers, and content type.

class HttpRequestTool : public ITool
{
public:
    HttpRequestTool() = default;

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};

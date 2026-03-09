-- file-stats/main.lua
-- Comprehensive file statistics plugin.
--
-- Demonstrates:
--   • Reading file content and editor state
--   • Event-driven status updates (editor.open / editor.save)
--   • Practical text analysis (blank lines, comment detection, longest line)
--   • Status bar integration

-- ── Helpers ──────────────────────────────────────────────────────────────────

local comment_prefixes = {
    lua     = "%-%-",
    py      = "#",
    python  = "#",
    cpp     = "//",
    c       = "//",
    h       = "//",
    hpp     = "//",
    java    = "//",
    js      = "//",
    ts      = "//",
    rs      = "//",
    go      = "//",
    cs      = "//",
    rb      = "#",
    sh      = "#",
    bash    = "#",
    cmake   = "#",
    yaml    = "#",
    yml     = "#",
    toml    = "#",
    json    = nil,  -- no line comments in JSON
    xml     = nil,
    html    = nil,
    md      = nil,
}

local function extension_of(path)
    return path:match("%.([^./\\]+)$") or ""
end

local function short_name(path)
    return path:match("[/\\]([^/\\]+)$") or path
end

local function analyze(content, path)
    local ext = extension_of(path):lower()
    local comment_pat = comment_prefixes[ext]

    local total_lines  = 0
    local blank_lines  = 0
    local comment_lines = 0
    local longest_line = 0
    local total_chars  = #content

    for line in (content .. "\n"):gmatch("(.-)\n") do
        total_lines = total_lines + 1
        local trimmed = line:match("^%s*(.-)%s*$")

        if #trimmed == 0 then
            blank_lines = blank_lines + 1
        elseif comment_pat and trimmed:match("^" .. comment_pat) then
            comment_lines = comment_lines + 1
        end

        if #line > longest_line then
            longest_line = #line
        end
    end

    local code_lines = total_lines - blank_lines - comment_lines

    return {
        total_lines   = total_lines,
        blank_lines   = blank_lines,
        comment_lines = comment_lines,
        code_lines    = code_lines,
        longest_line  = longest_line,
        total_chars   = total_chars,
        extension     = ext,
    }
end

local function format_size(bytes)
    if bytes < 1024 then
        return string.format("%d B", bytes)
    elseif bytes < 1024 * 1024 then
        return string.format("%.1f KB", bytes / 1024)
    else
        return string.format("%.1f MB", bytes / (1024 * 1024))
    end
end

-- ── Lifecycle ────────────────────────────────────────────────────────────────

function initialize()
    ex.log.info("File Stats plugin initializing...")

    -- ── Event: show quick stats bar on file open ─────────────────────────

    ex.events.on("editor.open", function(path)
        if not path then return end
        local content = ex.workspace.readFile(path)
        if not content then return end

        local stats = analyze(content, path)
        ex.notify.status(string.format(
            "%s — %d lines, %s",
            short_name(path), stats.total_lines, format_size(stats.total_chars)
        ), 4000)
    end)

    -- ── Event: update stats on save ──────────────────────────────────────

    ex.events.on("editor.save", function(path)
        if not path then return end
        local content = ex.workspace.readFile(path)
        if not content then return end

        local stats = analyze(content, path)
        ex.notify.status(string.format(
            "%d lines (%d code, %d comment, %d blank)",
            stats.total_lines, stats.code_lines,
            stats.comment_lines, stats.blank_lines
        ), 4000)
    end)

    -- ── Command: full file analysis ──────────────────────────────────────

    ex.commands.register("fileStats.analyze", "File Stats: Analyze", function()
        local path = ex.editor.activeFile()
        if not path then
            ex.notify.warning("No file is open.")
            return
        end

        local content = ex.workspace.readFile(path)
        if not content then
            ex.notify.error("Cannot read file: " .. short_name(path))
            return
        end

        local s = analyze(content, path)
        local lang = ex.editor.language() or s.extension

        ex.notify.info(string.format(
            "%s [%s] — %d lines (%d code, %d comment, %d blank), %s, longest: %d cols",
            short_name(path), lang,
            s.total_lines, s.code_lines, s.comment_lines, s.blank_lines,
            format_size(s.total_chars), s.longest_line
        ))
    end)

    -- ── Command: compare current vs saved stats ──────────────────────────

    ex.commands.register("fileStats.cursorInfo", "File Stats: Cursor Info", function()
        local path = ex.editor.activeFile()
        if not path then
            ex.notify.warning("No file is open.")
            return
        end

        local line = ex.editor.cursorLine()
        local col  = ex.editor.cursorColumn()
        local lang = ex.editor.language() or extension_of(path)

        ex.notify.info(string.format(
            "%s [%s] — Ln %d, Col %d",
            short_name(path), lang, line, col
        ))
    end)

    ex.log.info("File Stats plugin ready.")
end

function shutdown()
    ex.log.info("File Stats plugin shutting down.")
end

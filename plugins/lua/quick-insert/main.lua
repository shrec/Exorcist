-- quick-insert/main.lua
-- Utility plugin to insert timestamps, separators, and boilerplate via commands.
--
-- Demonstrates:
--   • Commands visible in the palette
--   • Reading editor context (language, file path)
--   • Generating useful text snippets
--   • Practical daily-use utility

-- ── Comment prefix by language ───────────────────────────────────────────────

local comment_for = {
    lua      = "--",
    python   = "#",
    cpp      = "//",
    c        = "//",
    java     = "//",
    javascript = "//",
    typescript = "//",
    rust     = "//",
    go       = "//",
    csharp   = "//",
    ruby     = "#",
    shellscript = "#",
    cmake    = "#",
    yaml     = "#",
    makefile = "#",
    perl     = "#",
    r        = "#",
}

local function get_comment()
    local lang = ex.editor.language()
    if lang then
        return comment_for[lang:lower()] or "//"
    end
    return "//"
end

local function short_name(path)
    if not path then return "untitled" end
    return path:match("[/\\]([^/\\]+)$") or path
end

-- ── Lifecycle ────────────────────────────────────────────────────────────────

function initialize()
    ex.log.info("Quick Insert plugin initializing...")

    -- ── Timestamp ────────────────────────────────────────────────────────

    ex.commands.register("quickInsert.timestamp", "Insert: Timestamp", function()
        local c = get_comment()
        local ts = os.date("%Y-%m-%d %H:%M:%S")
        ex.notify.info(string.format("Timestamp: %s %s", c, ts))
    end)

    -- ── Date ─────────────────────────────────────────────────────────────

    ex.commands.register("quickInsert.date", "Insert: Date", function()
        local d = os.date("%Y-%m-%d")
        ex.notify.info(string.format("Date: %s", d))
    end)

    -- ── Separator line ───────────────────────────────────────────────────

    ex.commands.register("quickInsert.separator", "Insert: Separator Line", function()
        local c = get_comment()
        local sep = c .. " " .. string.rep("\xe2\x94\x80", 70)  -- ─ (box drawing)
        ex.notify.info(sep)
    end)

    -- ── Section header ───────────────────────────────────────────────────

    ex.commands.register("quickInsert.sectionHeader", "Insert: Section Header", function()
        local c = get_comment()
        local header = string.format(
            "%s \xe2\x94\x80\xe2\x94\x80 Section %s",  -- ── Section ──...
            c,
            string.rep("\xe2\x94\x80", 60)
        )
        ex.notify.info(header)
    end)

    -- ── File header ──────────────────────────────────────────────────────

    ex.commands.register("quickInsert.fileHeader", "Insert: File Header", function()
        local c = get_comment()
        local path = ex.editor.activeFile()
        local name = short_name(path)
        local date = os.date("%Y-%m-%d")
        local lang = ex.editor.language() or "unknown"

        local header = string.format(
            "%s\n%s %s\n%s Language: %s\n%s Created: %s\n%s",
            c .. " " .. string.rep("=", 68),
            c, name,
            c, lang,
            c, date,
            c .. " " .. string.rep("=", 68)
        )
        ex.notify.info(header)
    end)

    -- ── Current file info ────────────────────────────────────────────────

    ex.commands.register("quickInsert.fileInfo", "Insert: File Info", function()
        local path = ex.editor.activeFile()
        local lang = ex.editor.language() or "?"
        local line = ex.editor.cursorLine()
        local col  = ex.editor.cursorColumn()

        ex.notify.info(string.format(
            "%s [%s] — Ln %d, Col %d",
            short_name(path), lang, line, col
        ))
    end)

    ex.log.info("Quick Insert plugin ready.")
end

function shutdown()
    ex.log.info("Quick Insert plugin shutting down.")
end

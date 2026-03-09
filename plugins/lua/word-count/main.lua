-- word-count/main.lua
-- Sample Lua plugin for Exorcist IDE.
--
-- Demonstrates:
--   • Reading selected text and active file (ex.editor)
--   • Reading file contents from workspace (ex.workspace)
--   • Practical text analysis utility
--   • Multiple command registrations

-- ── Helpers ──────────────────────────────────────────────────────────────────

local function count_words(text)
    if not text or #text == 0 then return 0 end
    local n = 0
    for _ in text:gmatch("%S+") do
        n = n + 1
    end
    return n
end

local function count_lines(text)
    if not text or #text == 0 then return 0 end
    local n = 1
    for _ in text:gmatch("\n") do
        n = n + 1
    end
    return n
end

local function format_count(text, label)
    local chars = #text
    local words = count_words(text)
    local lines = count_lines(text)
    return string.format("%s: %d lines, %d words, %d characters",
                         label, lines, words, chars)
end

-- ── Lifecycle ────────────────────────────────────────────────────────────────

function initialize()
    ex.log.info("Word Count plugin initializing...")

    -- Count selection (or entire file if nothing selected).
    ex.commands.register("wordCount.count", "Word Count: Count", function()
        local sel = ex.editor.selectedText()
        if sel and #sel > 0 then
            ex.notify.info(format_count(sel, "Selection"))
            return
        end

        -- No selection — count the whole file.
        local path = ex.editor.activeFile()
        if not path then
            ex.notify.warning("No file is open.")
            return
        end

        local content = ex.workspace.readFile(path)
        if not content then
            ex.notify.error("Cannot read file: " .. path)
            return
        end

        -- Extract just the filename for display.
        local name = path:match("[/\\]([^/\\]+)$") or path
        ex.notify.info(format_count(content, name))
    end)

    -- Quick status bar count (shorter output).
    ex.commands.register("wordCount.status", "Word Count: Status Bar", function()
        local sel = ex.editor.selectedText()
        local text = sel and #sel > 0 and sel
        if not text then
            local path = ex.editor.activeFile()
            if path then text = ex.workspace.readFile(path) end
        end

        if text then
            local w = count_words(text)
            local l = count_lines(text)
            ex.notify.status(string.format("%d words, %d lines", w, l), 3000)
        else
            ex.notify.status("No text to count", 2000)
        end
    end)

    ex.log.info("Word Count plugin ready.")
end

function shutdown()
    ex.log.info("Word Count plugin shutting down.")
end

-- save-guard/main.lua
-- Tracks save activity and highlights TODO/FIXME markers after each save.
--
-- Demonstrates:
--   • Event-driven plugin (editor.save)
--   • Reading file content (ex.workspace.readFile)
--   • Status bar updates (ex.notify.status)
--   • Session state tracking in Lua tables
--   • Practical value: keeps you aware of unfinished work

-- ── Session state ────────────────────────────────────────────────────────────

local session = {
    save_count  = 0,
    files_saved = {},   -- path → count
    markers     = {},   -- path → { line = ..., text = ... }
}

-- ── Helpers ──────────────────────────────────────────────────────────────────

local marker_patterns = { "TODO", "FIXME", "HACK", "XXX", "BUG" }

local function scan_markers(content, path)
    local results = {}
    local line_num = 0
    for line in (content .. "\n"):gmatch("(.-)\n") do
        line_num = line_num + 1
        for _, pat in ipairs(marker_patterns) do
            if line:find(pat, 1, true) then
                results[#results + 1] = {
                    line    = line_num,
                    marker  = pat,
                    text    = line:match("^%s*(.-)%s*$"),  -- trimmed
                }
            end
        end
    end
    return results
end

local function short_name(path)
    return path:match("[/\\]([^/\\]+)$") or path
end

-- ── Lifecycle ────────────────────────────────────────────────────────────────

function initialize()
    ex.log.info("Save Guard plugin initializing...")

    -- ── Event: editor.save ───────────────────────────────────────────────

    ex.events.on("editor.save", function(path)
        if not path then return end

        -- Track saves
        session.save_count = session.save_count + 1
        session.files_saved[path] = (session.files_saved[path] or 0) + 1

        -- Scan for markers
        local content = ex.workspace.readFile(path)
        if not content then return end

        local markers = scan_markers(content, path)
        session.markers[path] = markers

        local name = short_name(path)

        if #markers > 0 then
            -- Group markers by type
            local counts = {}
            for _, m in ipairs(markers) do
                counts[m.marker] = (counts[m.marker] or 0) + 1
            end

            local parts = {}
            for marker, count in pairs(counts) do
                parts[#parts + 1] = string.format("%s:%d", marker, count)
            end
            table.sort(parts)

            ex.notify.warning(string.format(
                "%s — %d marker(s) found: %s",
                name, #markers, table.concat(parts, ", ")
            ))

            -- Log details for the first few
            local show = math.min(#markers, 5)
            for i = 1, show do
                local m = markers[i]
                ex.log.info(string.format(
                    "  [%s] L%d: %s", m.marker, m.line, m.text
                ))
            end
            if #markers > show then
                ex.log.info(string.format("  ... and %d more", #markers - show))
            end
        else
            ex.notify.status(string.format(
                "Save #%d — %s clean, no markers", session.save_count, name
            ), 3000)
        end
    end)

    -- ── Command: show session save statistics ────────────────────────────

    ex.commands.register("saveGuard.stats", "Save Guard: Session Stats", function()
        if session.save_count == 0 then
            ex.notify.info("No saves yet this session.")
            return
        end

        local file_count = 0
        for _ in pairs(session.files_saved) do
            file_count = file_count + 1
        end

        local total_markers = 0
        for _, markers in pairs(session.markers) do
            total_markers = total_markers + #markers
        end

        ex.notify.info(string.format(
            "Session: %d save(s) across %d file(s), %d active marker(s)",
            session.save_count, file_count, total_markers
        ))
    end)

    -- ── Command: list markers in current file ────────────────────────────

    ex.commands.register("saveGuard.markers", "Save Guard: List Markers", function()
        local path = ex.editor.activeFile()
        if not path then
            ex.notify.warning("No file is open.")
            return
        end

        local content = ex.workspace.readFile(path)
        if not content then
            ex.notify.error("Cannot read file.")
            return
        end

        local markers = scan_markers(content, path)
        if #markers == 0 then
            ex.notify.info(short_name(path) .. " — no TODO/FIXME markers found.")
            return
        end

        local name = short_name(path)
        ex.notify.info(string.format("%s — %d marker(s):", name, #markers))
        for i, m in ipairs(markers) do
            ex.log.info(string.format("  L%d [%s]: %s", m.line, m.marker, m.text))
            if i >= 10 then
                ex.log.info(string.format("  ... and %d more", #markers - 10))
                break
            end
        end
    end)

    ex.log.info("Save Guard plugin ready.")
end

function shutdown()
    ex.log.info("Save Guard plugin shutting down.")
end

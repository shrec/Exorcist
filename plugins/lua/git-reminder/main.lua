-- git-reminder/main.lua
-- Reminds you to commit when you've been saving without committing.
--
-- Demonstrates:
--   • Git API (ex.git.isRepo, ex.git.branch, ex.git.diff)
--   • Event-driven reminders (editor.save)
--   • Session state tracking with thresholds
--   • Practical workflow utility

-- ── Config ───────────────────────────────────────────────────────────────────

local SAVE_THRESHOLD = 5   -- remind after this many saves without commit check

-- ── Session state ────────────────────────────────────────────────────────────

local saves_since_remind = 0

-- ── Lifecycle ────────────────────────────────────────────────────────────────

function initialize()
    ex.log.info("Git Reminder plugin initializing...")

    -- ── Event: check on save ─────────────────────────────────────────────

    ex.events.on("editor.save", function(path)
        saves_since_remind = saves_since_remind + 1

        if saves_since_remind < SAVE_THRESHOLD then
            return
        end

        -- Time to check git status
        if not ex.git.isRepo() then
            return
        end

        local branch = ex.git.branch() or "unknown"
        local diff = ex.git.diff(false)  -- unstaged diff

        if diff and #diff > 0 then
            -- Count approximate changed lines
            local additions = 0
            local deletions = 0
            for line in diff:gmatch("[^\n]+") do
                if line:sub(1, 1) == "+" and line:sub(1, 3) ~= "+++" then
                    additions = additions + 1
                elseif line:sub(1, 1) == "-" and line:sub(1, 3) ~= "---" then
                    deletions = deletions + 1
                end
            end

            local total = additions + deletions
            if total > 0 then
                ex.notify.info(string.format(
                    "Git [%s]: %d uncommitted changes (+%d/-%d) — consider committing!",
                    branch, total, additions, deletions
                ))
                ex.log.info(string.format(
                    "Git reminder: %d saves since last check, %d line changes on branch '%s'",
                    saves_since_remind, total, branch
                ))
            end
        end

        saves_since_remind = 0
    end)

    -- ── Command: manual git status check ─────────────────────────────────

    ex.commands.register("gitReminder.status", "Git: Quick Status", function()
        if not ex.git.isRepo() then
            ex.notify.warning("Not a Git repository.")
            return
        end

        local branch = ex.git.branch() or "unknown"
        local diff = ex.git.diff(false)

        if not diff or #diff == 0 then
            ex.notify.info(string.format("Git [%s]: Working tree clean.", branch))
            return
        end

        local additions = 0
        local deletions = 0
        local files_changed = {}
        for line in diff:gmatch("[^\n]+") do
            if line:sub(1, 6) == "diff -" then
                local f = line:match("b/(.+)$")
                if f then files_changed[#files_changed + 1] = f end
            elseif line:sub(1, 1) == "+" and line:sub(1, 3) ~= "+++" then
                additions = additions + 1
            elseif line:sub(1, 1) == "-" and line:sub(1, 3) ~= "---" then
                deletions = deletions + 1
            end
        end

        ex.notify.info(string.format(
            "Git [%s]: %d file(s) changed, +%d/-%d lines",
            branch, #files_changed, additions, deletions
        ))

        -- Log individual files
        for _, f in ipairs(files_changed) do
            ex.log.info("  modified: " .. f)
        end
    end)

    -- ── Command: show branch info ────────────────────────────────────────

    ex.commands.register("gitReminder.branch", "Git: Show Branch", function()
        if not ex.git.isRepo() then
            ex.notify.warning("Not a Git repository.")
            return
        end

        local branch = ex.git.branch() or "unknown"
        ex.notify.info(string.format("Current branch: %s", branch))
    end)

    ex.log.info("Git Reminder plugin ready.")
end

function shutdown()
    ex.log.info("Git Reminder plugin shutting down.")
end

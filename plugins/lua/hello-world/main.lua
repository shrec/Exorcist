-- hello-world/main.lua
-- Sample Lua plugin for Exorcist IDE.
--
-- Demonstrates:
--   • Registering a command (ex.commands.register)
--   • Showing notifications (ex.notify)
--   • Subscribing to IDE events (ex.events.on)
--   • Querying runtime memory (ex.runtime)
--   • Reading editor state (ex.editor)

function initialize()
    ex.log.info("Hello World plugin initializing...")

    -- Register a greeting command accessible from the Command Palette.
    ex.commands.register("helloWorld.greet", "Hello: Greet", function()
        local file = ex.editor.activeFile() or "(no file)"
        local line = ex.editor.cursorLine()
        local msg = string.format("Hello from Lua! You're on line %d of %s", line, file)
        ex.notify.info(msg)
    end)

    -- Register a memory info command.
    ex.commands.register("helloWorld.memoryInfo", "Hello: Memory Info", function()
        local used = ex.runtime.memoryUsed()
        local limit = ex.runtime.memoryLimit()
        local pct = math.floor((used / limit) * 100)
        local kb = math.floor(used / 1024)
        ex.notify.info(string.format("Lua memory: %d KB used (%d%% of limit)", kb, pct))
    end)

    -- Subscribe to editor save events.
    ex.events.on("editor.save", function(path)
        ex.log.info("File saved: " .. (path or "unknown"))
    end)

    ex.log.info("Hello World plugin ready.")
end

function shutdown()
    ex.log.info("Hello World plugin shutting down.")
end

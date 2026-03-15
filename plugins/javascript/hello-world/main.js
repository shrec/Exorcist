// hello-world/main.js
// Sample JavaScript plugin for Exorcist IDE.
//
// Demonstrates:
//   - Registering a command (ex.commands.register)
//   - Showing notifications (ex.notify)
//   - Subscribing to IDE events (ex.events.on)
//   - Querying runtime memory (ex.runtime)
//   - Reading editor state (ex.editor)

function initialize() {
    ex.log.info("Hello World JS plugin initializing...");

    // Register a greeting command accessible from the Command Palette.
    ex.commands.register("jsHelloWorld.greet", "JS Hello: Greet", function() {
        const file = ex.editor.activeFile() || "(no file)";
        const line = ex.editor.cursorLine();
        ex.notify.info("Hello from JavaScript! You're on line " + line + " of " + file);
    });

    // Register a memory info command.
    ex.commands.register("jsHelloWorld.memoryInfo", "JS Hello: Memory Info", function() {
        const used = ex.runtime.memoryUsed();
        const limit = ex.runtime.memoryLimit();
        const pct = Math.floor((used / limit) * 100);
        const kb = Math.floor(used / 1024);
        ex.notify.info("JS memory: " + kb + " KB used (" + pct + "% of limit)");
    });

    // Subscribe to editor save events.
    ex.events.on("editor.save", function(path) {
        ex.log.info("JS: File saved: " + path);
    });

    ex.log.info("Hello World JS plugin ready!");
}

function shutdown() {
    ex.log.info("Hello World JS plugin shutting down.");
}

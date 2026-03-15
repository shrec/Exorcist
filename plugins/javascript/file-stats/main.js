// file-stats/main.js
// Shows workspace file stats via a command.

function initialize() {
    ex.commands.register("jsFileStats.show", "JS: File Stats", function() {
        const root = ex.workspace.root();
        if (!root) {
            ex.notify.warning("No workspace open.");
            return;
        }

        const entries = ex.workspace.listDir();
        const openFiles = ex.workspace.openFiles();

        ex.notify.info(
            "Workspace: " + root +
            " | Top-level entries: " + entries.length +
            " | Open files: " + openFiles.length
        );
    });
}

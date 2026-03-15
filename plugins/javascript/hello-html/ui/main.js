// hello-html/ui/main.js
// Example HTML plugin using the ex.* SDK with full DOM access.

function showWorkspaceInfo() {
    var output = document.getElementById('output');
    var root = ex.workspace.root();
    if (!root) {
        output.innerHTML = '<span class="info-label">Workspace:</span> No workspace open';
        return;
    }

    var entries = ex.workspace.listDir();
    var openFiles = ex.workspace.openFiles();

    output.innerHTML =
        '<span class="info-label">Root:</span> ' + root + '\n' +
        '<span class="info-label">Top-level entries:</span> ' + entries.length + '\n' +
        '<span class="info-label">Open files:</span> ' + openFiles.length;
}

function showEditorInfo() {
    var output = document.getElementById('output');
    var file = ex.editor.activeFile();
    if (!file) {
        output.innerHTML = '<span class="info-label">Editor:</span> No file open';
        return;
    }

    var lang = ex.editor.language();
    var line = ex.editor.cursorLine();
    var col = ex.editor.cursorColumn();
    var sel = ex.editor.selectedText();

    output.innerHTML =
        '<span class="info-label">File:</span> ' + file + '\n' +
        '<span class="info-label">Language:</span> ' + (lang || 'unknown') + '\n' +
        '<span class="info-label">Cursor:</span> line ' + line + ', col ' + col + '\n' +
        '<span class="info-label">Selection:</span> ' + (sel ? '"' + sel.substring(0, 100) + '"' : 'none');
}

function sendNotification() {
    ex.notify.info('Hello from the HTML plugin!');
    var output = document.getElementById('output');
    output.innerHTML = '<span class="info-label">Sent:</span> info notification';
}

function initialize() {
    ex.commands.register('helloHtml.showWorkspace', 'Hello HTML: Show Workspace', function() {
        showWorkspaceInfo();
    });

    ex.commands.register('helloHtml.showEditor', 'Hello HTML: Show Editor Info', function() {
        showEditorInfo();
    });

    ex.log.info('Hello HTML plugin initialized');
}

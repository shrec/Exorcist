// word-count/main.js
// Shows word, line, and character count for the current selection or file.

function initialize() {
    ex.commands.register("jsWordCount.count", "JS: Word Count", function() {
        const text = ex.editor.selectedText() || "";

        if (text.length === 0) {
            ex.notify.info("No text selected.");
            return;
        }

        const words = text.trim().split(/\s+/).filter(function(w) { return w.length > 0; }).length;
        const lines = text.split("\n").length;
        const chars = text.length;

        ex.notify.info("Words: " + words + "  Lines: " + lines + "  Chars: " + chars);
    });
}

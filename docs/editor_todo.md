# Editor TODO (Architecture-First)

This list prioritizes a correct editor core over quick UI hacks.

## Core Data Model
- TextBuffer interface (immutable snapshots, edits).
- Piece table implementation (append-only add buffer).
- Line index structure (fast offset -> line, line -> offset).
- Undo/redo command log with batch edits.

## IO and Large Files
- Lazy load + chunked reading.
- Memory-mapped backend per platform.
- Encoding detection + line ending normalization.

## View Layer
- EditorView renders visible lines only.
- Line numbering (gutter) with sync to scroll.
- Selection and caret model separate from buffer.
- Glyph layout cache per line.

## Parsing and Language
- tree-sitter integration per file type.
- Incremental parse on edits.
- Token stream for highlighting.

## LSP
- Document sync (incremental).
- Diagnostics + go-to-definition + completion.
- Lifecycle management per language server.

## Performance Targets
- Open 100MB file < 2s.
- Scroll large file without stutter.
- Memory stable under heavy edits.

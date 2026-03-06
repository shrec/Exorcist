# Research Notes: Editor Pain Points (Draft)

These are common complaints from developer workflows. Treat as hypotheses
until validated with real feedback.

## Performance
- Large files freeze UI or become sluggish.
- Search across projects is slow or unreliable.
- High memory usage for simple tasks.

## Navigation
- Inconsistent symbol search or go-to-definition.
- Project tree slow with large repos.
- Tabs and buffers become unwieldy.

## Search
- Weak regex support or poor result ranking.
- No incremental results while typing.
- Filters and exclude rules are confusing.

## LSP/Intellisense
- Stale diagnostics or slow completions.
- Language servers crash without recovery.
- Too much CPU usage in background.

## UX Friction
- Unpredictable keybindings across features.
- Settings are scattered or undocumented.
- Too many modal dialogs for common tasks.

## Design Implications
- Keep core editor fast and incremental.
- Make search index-backed with streaming fallback.
- Strict lifecycle management for LSP processes.

# Exorcist UX Principles — Developer-Friendly UI/UX (mandatory)

> Project-wide, non-negotiable UX rules for every plugin, panel, dialog, and feature in Exorcist. Companion document to [manifesto.md](manifesto.md), [plugin-ui-standards.md](plugin-ui-standards.md), and [reference/ui-ux-reference.md](reference/ui-ux-reference.md). This document is normative — when in doubt, this document wins.

---

## Philosophy

Qt Creator's UX is dated and modal-heavy. It buries common operations behind multi-step dialogs, hides settings under deep category trees, and forces mouse-only flows for routine work. That is the past. **Exorcist is not Qt Creator.** Even though we ship on Qt, we explicitly reject the Qt Creator UX style.

VS Code raised the bar. It made the Command Palette a household concept, made search-everywhere expected, made inline editing the default, and made keyboard-driven workflows unsurprising. We adopt these wins by default. Where VS Code has dated patterns of its own (e.g., its settings tree), we improve, we do not regress.

**Exorcist's UX charter is simple: keyboard-first, search-everywhere, inline editing, no surprises.** A developer should be able to discover any feature without reading a manual: a command palette away, a search box away, a tooltip away. The UI must respect their flow — never block on modal dialogs for trivial actions, never force category-tree navigation when a flat searchable list works, never demand a mouse for a routine operation.

These principles are **mandatory across every plugin, every panel, every dialog**. They apply equally to the IDE shell, shared components, and third-party plugins. If a plugin author cannot meet a principle, they must justify it in the plugin's design notes — not silently ignore it.

---

## The 10 Principles

### 1. Keyboard-first navigation
Every common action has a shortcut. Mouse never required for any common workflow. **Ctrl+Shift+P** opens a Command Palette listing every action — that's the universal escape hatch when a user can't remember a shortcut. **Tab** cycles through editable fields in any property panel. **Esc** dismisses any popup or restores selection.

### 2. Search-everywhere
Every list/tree with >10 items has a search filter at the top with placeholder `Search...`. Property panels include a search box. Settings panels search by key. Long combo boxes are filterable.

### 3. Inline > Modal
Modal dialogs only for: destructive ops (delete project), multi-step wizards (new project), authentication. Everything else uses in-place editing: double-click to edit text, click on color cell to open small popover, drag-resize handles on canvas. **No "Edit Properties" dialog for trivial widget property changes.**

### 4. Smart guides and snap
On any drag operation in canvas-style editors, show alignment lines (Figma-style) when widgets align with siblings or layout containers. Snap to grid (8px), sibling edges, parent margins. Show distance hints between selected and other elements.

### 5. Live preview
**Spacebar** (or **F5** in form editors) toggles Edit ↔ Preview. Forms render real Qt widgets, code editors highlight syntax, markdown shows side preview. No "View → Preview" menu detour.

### 6. Flat property inspector
Replace nested category trees with a flat sortable list. Top: search filter. **"Modified ⬆ / All ⬇"** toggle. Type-aware inline editors (string→QLineEdit, color→swatch+popover, font→inline picker, geometry→inline ints). No two-level expansion.

### 7. Multi-select operations
**Ctrl+click** adds, **Shift+click** range-extends, marquee drag selects, **Ctrl+A** select-all. Common operations (align, distribute, set property, delete) work on multi-selection.

### 8. Always undoable
Every state-mutating action goes through `QUndoStack` with a meaningful description (`"Resize button to 120×30"`). Undo stack capacity 1000 per editor. **Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z** all work.

### 9. Visual feedback
Selected = accent border (`#007acc`) + 8 grip handles for resizable. Hover = subtle bg highlight (`#3e3e42`). Drop target = pulsing accent fill. Drag preview = ghost copy at 60% opacity. Loading = inline spinner, not modal "Please wait".

### 10. Smart defaults; explicit configuration
Plugins must provide working defaults: language detection, sensible color theme, minimal config required. Configuration is searchable. No config files to edit by hand for normal use. Power users can still drop into raw settings via **Ctrl+Shift+P → "Open Settings JSON"**.

---

## Concrete patterns (must follow)

### Panel header
Top row: title (left) + search (center) + 1–3 action buttons (right). All ≤ 28px tall.

### Lists/Trees
Always: hover highlight, selected accent, 2px border-radius. **NO native QListWidget styling** — use QSS to match VS dark.

### Property cells
Type-specific inline editor.
- Color = swatch + popover.
- Font = `"Segoe UI 9"` inline picker.
- Geometry = `"x:10 y:20 w:100 h:30"` 4 inline ints.
- List = comma-separated string editor.

### Status bar messages
3-second auto-dismiss. Color-coded (gray=info, yellow=warning, red=error). Inline retry button on errors.

### Tooltips
On every clickable widget. Format: `"Action description (Shortcut)"`. Example: `"Run Build (F7)"`.

### Drag visual
Drag preview = ghost (50% opacity) of dragged widget. Drop target zones light up with `#094771` background.

### Search box
Auto-focus on panel open. Placeholder `"Search items..."`. Clear button (×) when text present. Highlight matched substring in results.

---

## Anti-patterns (forbidden)

- ❌ Modal dialogs for trivial property changes
- ❌ Two-level property categories (use flat + filter)
- ❌ Mouse-only operations (must have keyboard equivalent)
- ❌ Native QFileDialog for color picking (use small popover)
- ❌ Tooltip without action description
- ❌ Lists without search above ~10 items
- ❌ Buttons without tooltips and shortcuts (when applicable)
- ❌ Action with no undo support
- ❌ Drop without visual feedback
- ❌ "Click here to apply" buttons (apply on change unless destructive)

---

## Plugin author checklist

When you write a new plugin, before merging:

- [ ] All operations accessible from keyboard
- [ ] All clickable elements have tooltips with shortcuts
- [ ] Lists/trees with >10 items have search
- [ ] Property panels are flat with search
- [ ] All state changes undoable
- [ ] Drag operations show visual feedback
- [ ] No modal dialogs except for destructive/multi-step ops
- [ ] Smart defaults work without configuration
- [ ] Theme matches VS dark-modern (`#1e1e1e` bg, `#d4d4d4` text, `#007acc` accent)
- [ ] Status bar updates on async ops

---

## Reference UI examples in our codebase

- ✅ **Good:** `OutputPanel` (search filter, category toggles), `OutlinePanel` (search + sort), `WorkspaceSymbolsDialog` (Ctrl+T fuzzy)
- ✅ **Good:** `QuickOpenDialog` (Ctrl+P fuzzy file open)
- ✅ **Good:** `GotoLineDialog` (Ctrl+G simple line jump)
- ⚠️ **Needs polish:** `ProblemsPanel` (no flat property)
- ❌ **Bad reference:** Qt Creator (don't follow its UI patterns)

---

## See also

- [manifesto.md](manifesto.md) — project spine (principle 4: Keyboard-Centric Workflow)
- [plugin-ui-standards.md](plugin-ui-standards.md) — shell vs plugin UI ownership
- [reference/ui-ux-reference.md](reference/ui-ux-reference.md) — VS 2022 dark theme visual reference
- [core-philosophy.md](core-philosophy.md) — broader "developer pain first" rationale

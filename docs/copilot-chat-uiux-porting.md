# VS Code Copilot Chat — UI/UX Porting Reference Document

> **Purpose:** Deep analysis of the VS Code Copilot Chat extension UI/UX, mapped against the current Exorcist `AgentChatPanel` implementation. Every visual component, color, spacing value, behavior, and interaction pattern is documented with exact CSS values from the VS Code source. Gaps are identified with specific remediation steps.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Color System — Dark Modern Theme](#2-color-system--dark-modern-theme)
3. [Typography & Font Stack](#3-typography--font-stack)
4. [Panel Layout Structure](#4-panel-layout-structure)
5. [Chat Participants & Modes](#5-chat-participants--modes)
6. [Input Area](#6-input-area)
7. [Message Bubbles & Layout](#7-message-bubbles--layout)
8. [Avatars & Identity](#8-avatars--identity)
9. [Code Blocks](#9-code-blocks)
10. [Tool Call Rendering](#10-tool-call-rendering)
11. [Thinking/Reasoning Display](#11-thinkingreasoning-display)
12. [Streaming & Progress](#12-streaming--progress)
13. [Slash Commands](#13-slash-commands)
14. [Follow-ups](#14-follow-ups)
15. [Context Attachments](#15-context-attachments)
16. [Feedback Actions](#16-feedback-actions)
17. [Welcome/Empty State](#17-welcomeempty-state)
18. [Error States](#18-error-states)
19. [Diff Preview & Code Changes](#19-diff-preview--code-changes)
20. [Session Management](#20-session-management)
21. [Scrollbar & Overflow](#21-scrollbar--overflow)
22. [Icons & Iconography](#22-icons--iconography)
23. [Model Picker](#23-model-picker)
24. [Menu & Dropdown Patterns](#24-menu--dropdown-patterns)
25. [Keyboard Shortcuts](#25-keyboard-shortcuts)
26. [Review System](#26-review-system)
27. [Accessibility](#27-accessibility)
28. [Gap Analysis — Exorcist vs VS Code](#28-gap-analysis--exorcist-vs-vs-code)

---

## 1. Architecture Overview

### VS Code Chat Panel Architecture

The VS Code Chat panel is **not part of the Copilot extension**. The chat panel UI lives in the VS Code workbench itself (`src/vs/workbench/contrib/chat/`). The Copilot extension provides:

- **Chat Participants** (via `contributes.chatParticipants`) — request handlers that process messages
- **Slash Commands** — registered per participant
- **Tools** (via `contributes.languageModelTools`) — 30+ tools for agent mode
- **Tool Sets** (via `contributes.languageModelToolSets`) — grouped tool collections
- **Menus** — context menus, title bar actions, input toolbar
- **Icons** — custom font (`copilot.woff`) with 3 glyphs
- **Welcome Messages** — 9 variants for different error states

The rendering of messages, avatars, thinking blocks, code blocks, tool call cards — all happen in **VS Code workbench code**, which the Copilot extension does not control. The CSS files in the Copilot repo are test simulation fixtures that replicate the workbench CSS for integration testing.

### Exorcist Architecture

`AgentChatPanel` is a monolithic `QWidget` (~2800 lines) that owns everything: layout, styling, message rendering via `QTextBrowser` with HTML/CSS, input handling, session management. This is fine for a standalone IDE — the workbench/extension split is a VS Code architectural pattern, not a design requirement.

---

## 2. Color System — Dark Modern Theme

### VS Code Dark Modern (from `dark-modern.ts` theme definition)

| Token | VS Code Value | Exorcist Value | Status |
|-------|--------------|----------------|--------|
| `editor.background` | `#1f1f1f` | `#1e1e1e` | ≈ Close (1 shade) |
| `editor.foreground` | `#cccccc` | `#cccccc` | ✅ Match |
| `sideBar.background` | `#181818` | `#1e1e1e` | ❌ Wrong — sidebar/panel bg should be darker |
| `panel.background` | `#181818` | `#1e1e1e` | ❌ Wrong |
| `panel.border` | `#2b2b2b` | `#2d2d2d` | ≈ Close |
| `input.background` | `#313131` | `#252526` | ❌ Too dark — VS Code input is lighter |
| `input.border` | `#3c3c3c` | `#3e3e42` | ≈ Close |
| `focusBorder` | `#0078d4` | `#007acc` | ≈ Close (slightly different blue) |
| `button.background` | `#0078d4` | `#0e639c` | ❌ Wrong — VS Code uses brighter blue |
| `button.foreground` | `#ffffff` | `white` | ✅ Match |
| `button.hoverBackground` | `#026ec1` | `#1177bb` | ❌ Different hover shade |
| `descriptionForeground` | `#9d9d9d` | `#858585` | ❌ Too dark |
| `errorForeground` | `#f85149` | `#f14c4c` | ≈ Close |
| `textLink.foreground` | `#4daafc` | `#569cd6` | ❌ Wrong blue — VS Code uses brighter link blue |
| `chat.slashCommandBackground` | `#34414b` | N/A (not implemented) | ❌ Missing |
| `chat.slashCommandForeground` | `#40a6ff` | N/A | ❌ Missing |
| `widget.border` | `#313131` | `#3e3e42` | ❌ Different |
| `widget.shadow` | (OS-dependent) | N/A | ❌ No box-shadow |
| `foreground` | `#cccccc` | `#cccccc` | ✅ Match |
| `list.hoverBackground` | (theme var) | `#094771` | → Check theme |

### VS Code Dark Default (Classic) — for reference

Many Exorcist colors match the **Dark Default** theme better, which may have been the original reference:

| Token | Dark Default | Dark Modern |
|-------|-------------|-------------|
| `editor.background` | `#1e1e1e` | `#1f1f1f` |
| `sideBar.background` | `#252526` | `#181818` |
| `input.background` | `#3c3c3c` | `#313131` |
| `button.background` | `#0e639c` | `#0078d4` |
| `focusBorder` | `#007acc` | `#0078d4` |
| `textLink.foreground` | `#3794ff` | `#4daafc` |

**Recommendation:** Decide on one theme as the reference and be consistent. VS Code Dark Modern is the current default since VS Code 1.85+. If staying with Dark Default, document it explicitly.

---

## 3. Typography & Font Stack

### VS Code Values

| Context | Font | Size | Weight |
|---------|------|------|--------|
| UI text | `"Segoe WPC", "Segoe UI", sans-serif` (Windows) | `13px` | `400` |
| Chat messages | Same as UI | `13px` | `400` |
| Chat line-height | — | `1.4em` (~18.2px) | — |
| Code blocks | Editor font (`Consolas`, `"Courier New"`, monospace) | `editor.fontSize` (default `14px`) | `400` |
| Inline code | `"SF Mono", Monaco, Menlo, Consolas, "Ubuntu Mono", monospace` | Inherits | — |
| Status/description | UI font | `11px` | `400` |
| Slash command pills | UI font | `12px` | — |
| Follow-up links | UI font | `12px` | — |
| Timestamps | UI font | `10px` or `11px` | — |

### Exorcist Values

| Context | Font | Size | Gap |
|---------|------|------|----|
| Body text | `'Segoe UI',Arial,sans-serif` | `13px` | ✅ |
| Line-height | — | `1.5` | ❌ Should be `1.4em` |
| Code blocks | `'Cascadia Code','Fira Code',Consolas,monospace` | `12px` | ❌ Should be `14px` |
| Inline code | Same as code | — | — |
| Status text | — | `11px` | ✅ |
| Timestamps | — | `10px` | ✅ |

**Gaps:**
- Line-height should be `1.4em`, not `1.5`
- Code block font size should match user's editor font size (default `14px`)
- Font stack missing `"Segoe WPC"` first

---

## 4. Panel Layout Structure

### VS Code Chat Panel (from workbench)

```
┌─────────────────────────────────────────────┐
│ [Chat Title] [Model Picker ▼] [⊕ New Chat]  │ ← Title bar (part of VS Code panel chrome)
├─────────────────────────────────────────────┤
│                                             │
│  Interactive List (messages area)            │ ← .interactive-list
│  padding: 4px 0 0 0                         │
│                                             │
│  ┌─ message ──────────────────────────────┐ │
│  │ [Avatar] [Name] [Timestamp]            │ │ ← .interactive-item-compact
│  │          [Message Content]             │ │   gap: 6px
│  │          [Actions]                     │ │   padding: 2px 20px 0 6px
│  └────────────────────────────────────────┘ │
│                                             │
├─────────────────────────────────────────────┤
│  [Context chips] [Attachments]              │ ← Context strip above input
├─────────────────────────────────────────────┤
│  ┌─ Input ──────────────────────────────┐   │ ← .interactive-input-part
│  │ [Text input area          ] [▶Send]  │   │   padding: 4px 6px 0 6px
│  │ [Model][Mode pills][+ Attach]        │   │   border-radius: 2px
│  └──────────────────────────────────────┘   │
└─────────────────────────────────────────────┘
```

### VS Code Panel Layout Details

| Component | CSS Property | Value |
|-----------|-------------|-------|
| Interactive list | padding | `4px 0 0 0` |
| Message item | gap | `6px` |
| Message item | padding-top | `2px` |
| Message item | padding-right | `20px` |
| Message item | padding-left | `6px` |
| Input part | padding | `4px 6px 0 6px` |
| Input + toolbar | border-radius | `2px` |
| Input + toolbar | width | `100%` |
| Execute toolbar | margin-bottom | `1px` |

### Exorcist Panel Layout

```
┌──────────────────────────────────────────────┐
│ [Provider Tab 1] [Provider Tab 2] ...        │ ← m_providerTabBar (35px)
├──────────────────────────────────────────────┤
│ separator (1px #2d2d2d)                      │
├──────────────────────────────────────────────┤
│                                              │
│  Chat History (QTextBrowser)                 │ ← m_history
│                                              │
├──────────────────────────────────────────────┤
│ [Working indicator: Thinking... shimmer bar] │ ← m_workingBar
├──────────────────────────────────────────────┤
│ [Changes: N files changed | Keep | Undo]     │ ← m_changesBar
├──────────────────────────────────────────────┤
│ separator                                    │
├──────────────────────────────────────────────┤
│ [Context strip: 1234 / 8192 tokens]          │ ← m_contextStrip
├──────────────────────────────────────────────┤
│ [📎 file.cpp] [📎 file.h] ...               │ ← m_attachBar
├──────────────────────────────────────────────┤
│ [Input area        ] [↑ Send] [✕ Cancel]     │ ← Input row (margins: 6,4,6,4)
├──────────────────────────────────────────────┤
│ [+ New] [☰ History] [Ask|Edit|Agent] [model] │ ← Bottom bar
│ [{ } Context]                                │
└──────────────────────────────────────────────┘
```

### Layout Gaps

| Component | VS Code | Exorcist | Issue |
|-----------|---------|----------|-------|
| Provider tabs | N/A (no provider tabs) | 35px tab bar | Extra element — acceptable for multi-provider |
| Title chrome | VS Code panel title bar | Not present | N/A — VS Code handles this in panel chrome |
| Input position | Input at BOTTOM inside panel | Input at bottom ✅ | Match |
| Mode selector | Quick pick / inline in input | Bottom bar pills | ❌ Mode pills should be INSIDE/NEAR the input area |
| Model picker | Title bar dropdown | Bottom bar combo | ❌ Should be inside the input bar area |
| New/History | Title bar icons | Bottom bar buttons | ❌ Should be in title area |
| Context chips | Just above or inside input | Separate strip | Acceptable |

---

## 5. Chat Participants & Modes

### VS Code Participants

| Participant | ID | Location | Mode | Icon |
|-------------|-----|----------|------|------|
| Default (Ask) | `github.copilot.default` | `panel` | `ask` | `$(copilot)` |
| Edit Session | `github.copilot.editingSession` | `panel` | `edit` | `$(copilot)` |
| Agent | `github.copilot.editsAgent` | `panel` | `agent` | `$(tools)` |
| Inline Editor | `github.copilot.editingSessionEditor` | `editor` | — | `$(copilot)` |
| VS Code Helper | `github.copilot.vscode` | `panel` | — | `$(vscode)` |
| Terminal | `github.copilot.terminal` | `terminal` | — | `$(terminal)` |
| Terminal Panel | `github.copilot.terminalPanel` | `panel` | — | `$(terminal)` |
| Notebook | `github.copilot.notebook` | `notebook` | — | `$(copilot)` |

### Mode Selector in VS Code

The mode selector appears as a **dropdown inside the input area** (not a separate pill bar):
- Current mode shown as text: "Ask" / "Edit" / "Agent"
- Clicking opens a quick-pick with mode descriptions
- Mode persistence: per-conversation

### Exorcist Implementation

- Three `QToolButton` pills: Ask, Edit, Agent
- Container: `background:#252526; border:1px solid #3e3e42; border-radius:4px`
- Active: `background:#007acc; color:#ffffff; border-radius:3px`
- Inactive: `background:transparent; color:#858585`
- Located in the bottom bar

### Gaps

1. **Mode selector position** — Should be part of the input bar, not a separate bottom bar element
2. **Mode descriptions** — VS Code shows agent descriptions (e.g., "Answers questions without making changes")
3. **Tool availability indicator** — Agent mode shows `$(tools)` icon
4. **Mode-specific behaviors:**
   - Ask: read-only tools only
   - Edit: edit tools for active file + attached files only
   - Agent: full tool-calling with all tools
5. **Handoff mechanism** — Edit mode can suggest switching to Agent mode

---

## 6. Input Area

### VS Code Input (from CSS analysis)

| Property | Value | Source |
|----------|-------|--------|
| Background | `var(--vscode-inlineChatInput-background)` ≈ `#313131` (dark modern) | CSS variable |
| Border | `outline: 1px solid var(--vscode-inlineChatInput-border)` | **NOT border** — uses `outline` |
| Focus | `outline: 1px solid var(--vscode-inlineChatInput-focusBorder)` → `#0078d4` | CSS variable |
| Border-radius | `2px` (input+toolbar container) | `.interactive-input-and-execute-toolbar` |
| Padding | `2px 2px 2px 6px` | `.input` |
| Placeholder color | `var(--vscode-inlineChatInput-placeholderForeground)` | CSS variable |
| Container padding | `4px 6px 0 6px` | `.interactive-input-part` |
| Width | `100%` | `.interactive-input-and-execute-toolbar` |

### VS Code Input Features

- **Monaco editor** as the input widget (not a plain text input)
- **Syntax highlighting** in the input
- **Slash command completion** inline
- **@mention completion** inline
- **#file reference** completion inline
- **Multi-line support** with auto-resize
- **Send button** inside the input bar on the right
- **Attach button** inside the input bar
- **Mode selector** inside the input bar
- **Model indicator** inside the input bar

### Exorcist Input

| Property | Value |
|----------|-------|
| Widget | `QPlainTextEdit` |
| Background | `#252526` |
| Border | `1px solid #3e3e42` |
| Focus | `border-color: #007acc` |
| Border-radius | `2px` |
| Padding | `2px 2px 2px 6px` |
| Font-size | `13px` |
| Min/Max height | `60–180px` |

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| Input background | `#313131` | `#252526` | Medium — color mismatch |
| Border method | `outline` (not `border`) | `border` | Low — visual difference is minimal |
| Editor-as-input | Monaco editor with syntax highlight | QPlainTextEdit | High — significant UX gap |
| Send button position | Inside input bar, right side | Separate button in input row | Medium |
| Attach button | Inside input bar | Separate button | Medium |
| Mode/Model | Inside input bar | Bottom bar | Medium |
| Auto-resize | Grows with input | 60–180px clamp | ✅ Similar |
| Placeholder | Themed via CSS variable | (check implementation) | Low |

---

## 7. Message Bubbles & Layout

### VS Code Message Layout

VS Code uses a **flat layout** — NO table, NO bubbles. Messages are stacked vertically in a list widget:

```
┌────────────────────────────────────────────┐
│ [24px Avatar] User Name          [time]    │ ← header row
│              Message text...               │ ← content
│              More text...                  │
│              [👍] [👎] [📋 Copy] [⋮ More]  │ ← actions (hover-reveal)
├────────────────────────────────────────────┤
│ [24px Avatar] Copilot            [time]    │
│              Response text...              │
│              ```code block```              │
│              [👍] [👎] [📋 Copy] [⋮ More]  │
└────────────────────────────────────────────┘
```

### VS Code Message CSS

| Selector | Property | Value |
|----------|----------|-------|
| `.interactive-item-compact` | gap | `6px` |
| `.interactive-item-compact` | padding-top | `2px` |
| `.interactive-item-compact` | padding-right | `20px` |
| `.interactive-item-compact` | padding-left | `6px` |
| `.header .avatar` | outline-offset | `-1px` |
| `.chatMessage` | padding | `0 3px` (workbench) / `8px 3px` (editor) |
| `.chatMessageContent` | padding | `2px 2px` |
| `.chatMessageContent` | user-select | `text` |

### Exorcist Message Layout

Uses HTML `<table>` with two columns: avatar cell + content cell:

```html
<table class='msg-table'>
  <tr>
    <td class='avatar-cell'>[Avatar]</td>
    <td class='content-cell'>
      <div class='who'>Name <span class='timestamp'>HH:MM</span></div>
      <div class='body'>Message content</div>
      <div class='msg-actions'>Actions</div>
    </td>
  </tr>
</table>
```

| CSS Property | Value |
|-------------|-------|
| `.msg-table` | `width:100%; border-spacing:0` |
| `.avatar-cell` | `width:28px; vertical-align:top; padding:8px 0 4px 10px` |
| `.content-cell` | `vertical-align:top; padding:6px 16px 4px 8px` |

### Gaps

| Feature | VS Code | Exorcist | Impact |
|---------|---------|----------|--------|
| Layout method | Flex-based list items | HTML `<table>` | Medium — table works visually |
| Message padding | `0 3px` content, `6px` left container | `8px 0 4px 10px` avatar, `6px 16px 4px 8px` content | ❌ Over-padded |
| Separator | No separator between messages | No separator | ✅ Match |
| User-select | `text` (selectable) | QTextBrowser default | ✅ Match |
| Actions visibility | Hidden until hover | Always visible | ❌ Actions should auto-hide |
| Content max-width | Grows with panel | `width:100%` table | ✅ Match |

---

## 8. Avatars & Identity

### VS Code Avatars

- **User**: GitHub profile picture (fetched from GitHub API), circular
- **Copilot**: `$(copilot)` theme icon, circular
- **Size**: ~24px
- **Border**: `outline-offset: -1px` (thin outline on the avatar itself)
- **Shape**: Circular (CSS `border-radius: 50%`)

### Exorcist Avatars

| Property | User | AI |
|----------|------|-----|
| Size | `24px × 24px` | `24px × 24px` |
| Shape | `border-radius: 12px` (circle) | `border-radius: 12px` (circle) |
| Background | `#0e639c` (blue) | `#5a4fcf` (purple) |
| Content | Letter `U` | `✦` (sparkle &#10022;) |
| Text color | `#ffffff` | `#ffffff` |
| Font-size | `13px` | `13px` |
| Font-weight | `600` | `600` |

### Gaps

1. **User avatar** — VS Code shows the user's GitHub profile picture. Exorcist shows a static "U" letter. Consider:
   - Showing user's initials from OS username
   - Loading avatar from GitHub profile if authenticated
2. **Copilot avatar** — VS Code uses the Copilot icon font glyph. Exorcist uses ✦ (unicode sparkle). The Copilot icon is a distinct visual identity. Consider:
   - Using the actual Copilot icon (from `copilot.woff` — character `\0041`)
   - Or a custom SVG equivalent
3. **Avatar outline** — VS Code has `outline-offset: -1px`, Exorcist has none

---

## 9. Code Blocks

### VS Code Code Block Rendering

VS Code uses Monaco Editor instances for code blocks in chat:

- Each code block is rendered as a **mini Monaco editor** (read-only)
- **Syntax highlighting** via TextMate/TreeSitter grammars
- **Language detection** from the fence marker (```python, ```typescript, etc.)
- **File path resolution** from code block headers
- **IntelliSense** (hover, go-to-definition) via `vscode-chat-code-block` URI scheme
- **Code block actions**: Copy, Insert at Cursor, Apply to File, Run in Terminal

### Code Block Processing (from `codeBlockProcessor.ts`)

State machine for streaming code blocks:
1. `OutsideCodeBlock` — Accumulating markdown
2. `LineAfterFence` — Just saw ` ``` ` — checking for language/filepath
3. `LineAfterFilePath` — Collecting file path annotation
4. `InCodeBlock` — Accumulating code until closing ` ``` `

Supports nested code blocks via `_nestingLevel` counter.

### VS Code Code Block Actions

| Action | Icon | Description |
|--------|------|-------------|
| Copy | `$(copy)` | Copy code to clipboard |
| Insert at Cursor | `$(insert)` | Insert at editor cursor |
| Apply to File | `$(sparkle-filled)` | Apply as edit to resolved file |
| Run in Terminal | `$(terminal)` | Run in integrated terminal |

### Exorcist Code Block CSS

```css
pre {
    background: #1e1e1e;
    padding: 10px 12px;
    border-radius: 2px;
    font-family: 'Cascadia Code','Fira Code',Consolas,monospace;
    font-size: 12px;
    border-left: 3px solid #0e639c;
}
code { color: #ce9178; }
.code-header {
    background: #2d2d30;
    border-radius: 2px 2px 0 0;
    padding: 4px 10px;
    font-size: 11px;
    color: #858585;
}
.code-lang { color: #569cd6; font-weight: 600; }
.code-header-actions a { color: #569cd6; font-size: 11px; margin-left: 8px; }
```

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| Renderer | Monaco editor (syntax highlighting) | `<pre>` + monochrome text | **Critical** |
| Syntax highlighting | Full TextMate/TreeSitter | None (single color `#ce9178`) | **Critical** |
| Language detection | From fence marker | Parsed but display only | Medium |
| Code block border | None (seamless in editor) | `border-left: 3px solid #0e639c` | Low — stylistic choice |
| Font size | Editor font size (`14px` default) | `12px` | Medium |
| IntelliSense | Hover, go-to-definition | None | Low (future) |
| Code actions | Copy, Insert, Apply, Run | Copy, Insert (via links) | Medium |
| Action position | Header bar, below code | In code-header-actions | ✅ Similar |

**Remediation:** Syntax highlighting is the #1 visual gap. Options:
1. Use QSyntaxHighlighter with language-specific highlighter classes
2. Server-side highlight into HTML spans with colors before inserting into QTextBrowser
3. Use a code editor widget (QScintilla) for code blocks

---

## 10. Tool Call Rendering

### VS Code Tool Call Display

VS Code renders tool calls as expandable cards:

```
┌────────────────────────────────────┐
│ ▶ readFile src/main.cpp            │ ← Collapsed tool call
└────────────────────────────────────┘

┌────────────────────────────────────┐
│ ▼ readFile src/main.cpp            │ ← Expanded tool call
│   ┌──────────────────────────────┐ │
│   │ File contents...              │ │ ← Tool result (collapsible)
│   └──────────────────────────────┘ │
└────────────────────────────────────┘
```

Response stream types for tools:
- `beginToolInvocation` — Shows tool name + spinner
- `updateToolInvocation` — Updates with result preview
- `clearToPreviousToolInvocation` — Clears to reuse tool card

### Exorcist Tool Call CSS

```css
.tool-call {
    background: #1e1e1e;
    border-radius: 2px;
    padding: 5px 10px;
    border-left: 3px solid #3e3e42;
}
.tool-run  { border-left-color: #007acc; color: #9d9d9d; }
.tool-ok   { border-left-color: #4ec9b0; }
.tool-fail { border-left-color: #f14c4c; color: #f14c4c; }
.tool-name { color: #cccccc; font-weight: 600; }
```

### Exorcist Tool Call HTML

```html
Running:  <div class='tool-call tool-run'>▶ <span class='tool-name'>NAME</span>…</div>
Success:  <div class='tool-call tool-ok'>▶ <span class='tool-name'>NAME</span> ✓</div>
Failure:  <div class='tool-call tool-fail'>✗ <span class='tool-name'>NAME</span>...</div>
```

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| Expand/collapse | Collapsible card with results | Static display | Medium |
| Spinner | Animated spinner during execution | Text "…" | Low |
| Result preview | Shows abbreviated result | Not shown | Medium |
| Tool arguments | Shown in expandable detail | Not shown | Low |
| Confirmation UI | Dialog before dangerous tools | QMessageBox dialog | ✅ Implemented |
| Tool progress | Streaming updates | Border color change | ✅ Adequate |

---

## 11. Thinking/Reasoning Display

### VS Code Thinking

VS Code supports `thinkingProgress` response parts (from `languageModelThinkingPart` proposed API):
- Thinking content shown in a collapsible section
- Labeled "Thinking…" with animation during streaming
- Collapses when response generation begins
- Configurable thinking budget: `0–32000` tokens (default: `16000`)
- Effort levels: `low`, `medium`, `high`

### Exorcist Thinking CSS

```css
.thinking-block {
    background: #1a1a2e;
    border-left: 3px solid #5a4fcf;
    padding: 6px 10px;
    border-radius: 2px;
    color: #9d9dbd;
}
.thinking-summary { color: #7a7a9e; font-size: 11px; }
```

### Exorcist Thinking HTML

```html
<div class='thinking-block'>
  <div class='thinking-summary'>💡 Thinking</div>
  <div style='margin-top:4px;white-space:pre-wrap;'>...reasoning text...</div>
</div>
```

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| Collapsible | Yes — expand/collapse | Static | Medium |
| Animation | Animated label during streaming | Static text | Low |
| Effort control | Setting with 3 levels + token budget | Not exposed in UI | Medium |
| Auto-collapse | Collapses when response starts | Persists | Medium |

---

## 12. Streaming & Progress

### VS Code Streaming

- Text streams token-by-token into the message area
- Cursor: blinking bar at end of text (editor cursor)
- Progress: `monaco-progress-container` with animated bar
- Progress bar: `relative; width:calc(100%-18px); left:19px`
- No explicit "typing" indicator — content just appears

### Exorcist Streaming

- Text accumulated in `m_pendingAccum`, rendered via `updateStreamingContent()`
- Cursor: `<span style='color:#007acc;'>█</span>` (block cursor)
- Working indicator: shimmer bar animation (30ms interval, 3px/tick)
- Working label: `color:#858585; font-size:11px; font-style:italic`
- Progress track: `background:#2d2d2d; height:2px`
- Progress thumb: `background:#007acc; border-radius:1px; width:80px`

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| Cursor style | Blinking editor cursor | Block character `█` | Low |
| Progress bar | Native VS Code progress | Custom shimmer bar | ✅ Adequate |
| Smooth scroll | Auto-scroll during streaming | Scroll via QTextBrowser | Check behavior |
| Token counter | Not shown during streaming | Not shown | ✅ Match |

---

## 13. Slash Commands

### VS Code Slash Commands

Slash commands are registered per participant and rendered as **pills** in the input:

| Command | Participant | Description |
|---------|-------------|-------------|
| `/explain` | default, agent | Explain code |
| `/review` | default, agent | Review and comment |
| `/tests` | default, agent | Generate tests |
| `/fix` | default, agent | Fix code |
| `/new` | default, agent | New workspace (sticky) |
| `/newNotebook` | default, agent | New Jupyter notebook |
| `/search` | vscode | Search settings |
| `/compact` | agent | Compact conversation |
| `/generate` | editor | Generate code (inline) |
| `/edit` | editor | Edit code (inline) |
| `/doc` | editor | Add documentation (inline) |

### Slash Command Pill CSS (VS Code)

```css
.inline-chat-slash-command {
    background-color: var(--vscode-chat-slashCommandBackground);  /* #34414b */
    color: var(--vscode-chat-slashCommandForeground);              /* #40a6ff */
    border-radius: 2px;
    padding: 1px;
}
```

### Slash Command Pill CSS (older variant)

```css
.slash-command-pill CODE {
    border-radius: 3px;
    padding: 0 1px;
    background-color: var(--vscode-chat-slashCommandBackground);
    color: var(--vscode-chat-slashCommandForeground);
}
```

### Exorcist Slash Menu

- `QListWidget` popup: `width:300px`
- Menu styling: `background:#252526; border:1px solid #3e3e42`
- Item: `padding:5px 10px; font-size:12px`
- Hover/select: `background:#094771`
- Resolves: `/explain`, `/fix`, `/tests`, `/review`, `/doc`, `/commit`, `/terminal`

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| Pill rendering in input | Inline colored pill | Plain text replacement | Medium |
| Pill colors | `#34414b` bg, `#40a6ff` fg | Not implemented | Medium |
| Sticky commands | `/new` stays active across turns | Not implemented | Low |
| Sample requests | `/fix` has "Fix the code in..." hint | Not implemented | Low |
| Command completion | Inline as-you-type | Popup menu | ✅ Acceptable |

---

## 14. Follow-ups

### VS Code Follow-ups

Follow-up suggestions appear after a response as clickable links:

```css
.followUps {
    padding: 5px 5px;
}
.followUps .monaco-button {
    display: block;
    color: var(--vscode-textLink-foreground);  /* #4daafc */
    font-size: 12px;
}
```

### Exorcist Follow-ups

Follow-ups are rendered as part of the suggestion pills in the empty state. No follow-up suggestions after AI responses.

### Gap

- **Critical**: VS Code shows contextual follow-up suggestions after each AI response. Exorcist does not implement this. This is a significant UX gap for discoverability.

---

## 15. Context Attachments

### VS Code Context Types

| Type | Trigger | Description |
|------|---------|-------------|
| File | `#file:path` | Attach file content |
| Selection | `#selection` | Attach editor selection |
| Terminal | `#terminalSelection` | Terminal output |
| Editor | `#editor` | Current editor content |
| Problems | `#problems` | Diagnostics list |
| Changed Files | `#changes` | Git modifications |
| Codebase | `#codebase` | Semantic search results |
| Web | `@fetch url` | Web page content |

### Exorcist Context Attachments

Implemented:
- File attachment via drag-drop or attach button → chip with `📎 FILENAME`
- Selection attachment via `attachSelection()` → chip with `✂ Selected lines`
- Diagnostics attachment via `attachDiagnostics()` → chip with `⚠ N diagnostics`
- Image attachment (base64 inline) → chip with `🖼 FILENAME`

Chip CSS:
```css
/* Attachment chips */
background: #252526;
border: 1px solid #3e3e42;
border-radius: 3px;
padding: 2px 7px;
font-size: 11px;
```

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| `#file` reference | Inline in input with completion | Attachment chip separate | Medium |
| `#selection` | Inline reference | Separate chip | ✅ Adequate |
| `#codebase` | Semantic search context | Not implemented as UI | Low |
| `#problems` | Diagnostics context | Via `attachDiagnostics` | ✅ |
| `#changes` | Git diff context | Not implemented | Medium |
| `#editor` | Full editor content | Not implemented as UI | Low |
| Inline completion | Type `#` for context picker | Not implemented | Medium |

---

## 16. Feedback Actions

### VS Code Feedback

Actions appear on hover over a message:

| Action | Icon | Behavior |
|--------|------|----------|
| Thumbs Up | `$(thumbsup)` | Positive feedback + telemetry |
| Thumbs Down | `$(thumbsdown)` | Negative feedback + telemetry |
| Copy | `$(copy)` | Copy to clipboard |
| Insert at Cursor | `$(insert)` | Insert at editor cursor |
| More... | `$(more)` | Context menu: Report issue, etc. |

Hover behavior: Actions container is hidden by default, shown on mouse hover over the message.

### Exorcist Feedback

```html
<div class='feedback'>
  <a href='feedback://N/up'>👍</a>
  <a href='feedback://N/down'>👎</a>
  <a href='copyresp://N'>📋 Copy</a>
</div>
```

User messages:
```html
<div class='msg-actions'>
  <a href='retry://N'>↻ Retry</a>
  <a href='edit://N'>✎ Edit</a>
</div>
```

CSS:
```css
.msg-actions a { color:#6a6a6a; padding:1px 4px; border-radius:2px; }
.feedback a    { color:#6a6a6a; padding:1px 5px; border-radius:2px; }
```

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| Hover-reveal | Hidden until hover | Always visible | Medium — reduces visual noise |
| Insert at Cursor | Dedicated button | Not in feedback area | Low |
| More menu | Dropdown with extra actions | Not implemented | Low |
| Issue reporting | "Report Issue" opens GitHub | Not implemented | Low |
| Emoji vs icons | Codicons (SVG icons) | Unicode emoji (👍👎📋) | Low — visual difference |
| Action color | Theme-based | `#6a6a6a` hard-coded | Low |

---

## 17. Welcome/Empty State

### VS Code Welcome State

VS Code's chat welcome shows:
- Copilot icon (from icon font)
- "Ask Copilot" or similar heading
- Mode-dependent prompt suggestions
- `$(chat-sparkle)` icon in welcome messages
- 9 error-state variants (expired, offline, rate limited, etc.)

### Exorcist Welcome State

```html
<div style='text-align:center; padding-top:48px;'>
  <div style='font-size:36px; color:#5a4fcf;'>✦</div>
  <div style='font-size:15px; font-weight:600; color:#cccccc;'>Copilot</div>
  <div style='font-size:12px; color:#858585;'>Ask anything or pick a suggestion</div>
  <!-- Suggestion pills -->
  <a href='suggest://explain'>/explain — Explain selected code</a>
  <a href='suggest://fix'>/fix — Find and fix bugs</a>
  <a href='suggest://test'>/test — Generate unit tests</a>
  <a href='suggest://review'>/review — Code review</a>
  <a href='suggest://doc'>/doc — Add documentation</a>
</div>
```

Pill CSS: `background:#252526; border:1px solid #3e3e42; border-radius:14px; padding:6px 14px; margin:4px; font-size:12px; color:#cccccc`

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| Icon | Copilot icon (custom font) | ✦ (unicode sparkle) | Low |
| Suggestions | Context-aware | Static 5 commands | Medium |
| Error variants | 9 specialized messages | Single welcome state | Medium |
| Mode-dependent | Different for Ask/Edit/Agent | Same for all modes | Medium |

---

## 18. Error States

### VS Code Error Variants (from `chatViewsWelcome`)

1. **Individual Expired** — Subscription expired message
2. **Enterprise Disabled** — Organization disabled chat
3. **Offline** — No internet connection
4. **Invalid Token** — Authentication failed
5. **Rate Limited** — Too many requests
6. **GitHub Login Failed** — Login error
7. **Contact Support** — Generic support redirect
8. **Chat Disabled** — Feature flag off
9. **Switch to Release** — Insiders-only feature

### Exorcist Error Handling

| Error Type | Icon | CSS | Behavior |
|------------|------|-----|----------|
| Auth error | 🔒 | `.err` class + recovery steps | Shows provider-specific message |
| Rate limited | ⏲ | `.err` class | Auto-retry info |
| Network error | 🌐 | `.err` class | Connectivity tips |
| Cancelled | — | italic text | "Request cancelled." |
| Generic | ⚠ | `.err` class | Error message |

Error CSS:
```css
.err {
    color: #f14c4c;
    border-left: 2px solid #f14c4c;
    background: rgba(241,76,76,0.06);
    padding: 6px 10px 6px 42px;
    margin: 4px 10px 4px 38px;
}
```

### Assessment

Exorcist has good error differentiation. The main gap is the lack of error-specific welcome states (showing a specialized empty state when the provider is unavailable vs. showing an error after a failed request).

---

## 19. Diff Preview & Code Changes

### VS Code Diff/Edit Preview

The VS Code chat panel supports inline diff previewing:

```css
.previewDiff {
    display: inherit;
    padding: 6px;
    border: 1px solid var(--vscode-inlineChat-border);
    border-radius: 2px;
    margin: 6px 0;
}
.previewCreate {
    display: inherit;
    padding: 6px;
    border: 1px solid var(--vscode-inlineChat-border);
    border-radius: 2px;
    margin: 6px 0;
}
```

Diff colors:
```css
.inline-chat-inserted-range {
    background-color: var(--vscode-inlineChatDiff-inserted);
}
.inline-chat-inserted-range-linehighlight {
    background-color: var(--vscode-diffEditor-insertedLineBackground);
}
.inline-chat-original-zone2 {
    background-color: var(--vscode-diffEditor-removedLineBackground);
    opacity: 0.8;
}
```

### Exorcist Changes Bar

```
m_changesBar: background:#1a2433; border-top:1px solid #2d2d2d
m_changesLabel: "N files changed" → color:#cccccc; font-size:12px
m_keepBtn: background:#0e639c → "Keep"
m_undoBtn: background:#3a3d41 → "Undo"
```

Pending patches stored as `QList<PatchProposal>` with filePath, startLine, endLine, replacement.

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| Inline diff preview | Monaco diff editor embedded | Not shown | **High** |
| Side-by-side diff | Available | Not available | Medium |
| Per-file accept/reject | Granular control | All-or-nothing Keep/Undo | **High** |
| Diff colors | Themed via CSS variables | No diff display | **High** |
| Created file preview | Shown in chat | Not shown | Medium |
| Apply actions | Accept, Discard per change | Keep All / Undo All | **High** |

---

## 20. Session Management

### VS Code Sessions

- **New Chat**: Title bar icon, creates new conversation
- **Chat History**: Title bar icon, opens session list
- **Session titles**: Auto-generated from first message via `ChatTitleProvider`
- **Session persistence**: Via `conversationStore` (LRU cache, 1000 entries)
- **CLI Sessions**: Separate session type for Copilot CLI integration
- **Cloud Sessions**: Remote agent sessions (background/cloud agents)
- **Session actions**: Rename, Delete, Resume in Terminal, Open Repository

### Exorcist Sessions

- **New Session**: `m_newSessionBtn` (`+` icon) in bottom bar
- **History**: `m_historyBtn` (`☰` icon) opens styled menu
- **Session list**: Custom popup with search field
- **Session actions**: Delete (🗑), Rename (✏)
- **Session store**: via `SessionStore*` (injected)

Session menu CSS:
```css
background: #252526;
color: #cccccc;
border: 1px solid #3e3e42;
```

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| Auto-title | LLM-generated title | Not implemented | Medium |
| Session count | 1000 LRU | Implementation-dependent | ✅ |
| Position | Title bar | Bottom bar | Low — acceptable |
| Search | Not prominent | Has search field | ✅ Better |
| CLI sessions | Separate session type | Not applicable | N/A |

---

## 21. Scrollbar & Overflow

### VS Code Scrollbar

VS Code uses custom styled scrollbars across all panels:
- Thin (≈8-10px)
- Semi-transparent handle
- No track background
- Fade in/out on hover

### Exorcist Scrollbar

```css
QScrollBar:vertical {
    background: transparent;
    width: 8px;
}
QScrollBar::handle:vertical {
    background: #424242;
    border-radius: 4px;
    min-height: 20px;
}
```

### Assessment

✅ Close match. The scrollbar styling is appropriate. Could add fade behavior but this is a Qt limitation.

---

## 22. Icons & Iconography

### VS Code Icons (Codicon System)

VS Code uses the **Codicon** icon font for all UI icons. The Copilot extension adds:

| Icon ID | Character | Font |
|---------|-----------|------|
| `copilot-logo` | `\0041` (A) | `copilot.woff` |
| `copilot-warning` | `\0042` (B) | `copilot.woff` |
| `copilot-notconnected` | `\0043` (C) | `copilot.woff` |

Common Codicons used in chat:
- `$(copilot)` — Copilot logo
- `$(tools)` — Agent mode
- `$(sparkle)` / `$(sparkle-filled)` — AI action indicators
- `$(chat-sparkle)` — Chat-specific sparkle
- `$(copy)` — Copy
- `$(insert)` — Insert at cursor
- `$(terminal)` — Terminal
- `$(thumbsup)` / `$(thumbsdown)` — Feedback
- `$(more)` — More actions menu
- `$(close)` — Close/discard
- `$(pencil)` — Edit
- `$(eye)` — View/read
- `$(search)` — Search
- `$(globe)` — Web
- `$(arrow-up)` / `$(arrow-down)` — Navigate

### Exorcist Icons

All icons are Unicode characters/emoji:

| Symbol | Usage |
|--------|-------|
| `↑` | Send |
| `✕` | Cancel |
| `⊕` | Attach |
| `+` | New session |
| `☰` | History menu |
| `{ }` | Context window |
| `✦` | AI avatar/sparkle |
| `U` | User avatar |
| `👍👎` | Feedback |
| `📋` | Copy |
| `📎` | File attachment |
| `💡` | Thinking header |
| `▶✓✗` | Tool call states |

### Gap Assessment

Using Unicode characters is a pragmatic choice for a Qt application. However:
1. **Inconsistent rendering** across platforms — emoji may look different on Windows/macOS/Linux
2. **No icon font** — Consider Codicon-compatible SVG icons or a custom icon font
3. **Professional appearance** — SVG icons look more polished than emoji

**Recommendation:** Create a small Codicon-subset icon font or use SVG resources via Qt Resource System (`.qrc`). Focus on the most visible icons first: Send, Copilot avatar, feedback buttons.

---

## 23. Model Picker

### VS Code Model Picker

- **Location**: Title bar of chat panel, or via command palette
- **Format**: Dropdown showing model name + vendor
- **Models available**: 9 built-in vendor categories:
  - Copilot (built-in)
  - Anthropic (API key)
  - xAI (API key)
  - Google Gemini (API key)
  - OpenRouter (API key)
  - OpenAI (API key)
  - Ollama (localhost)
  - OpenAI Compatible (custom)
  - Azure (API key / Entra ID)
- **Model info**: name, vendor, version, capabilities, billing tier
- **Per-model capabilities**: streaming, toolCalls, vision, thinking, maxTokens

### Exorcist Model Picker

- `QComboBox` in the bottom bar
- Per-mode model selection (`m_perModeModel[3]`)
- Styling: `background:transparent; border:none; color:#858585; font-size:12px`
- Dropdown: `background:#252526; color:#cccccc; border:1px solid #3e3e42; selection-background-color:#094771`

### Gaps

| Feature | VS Code | Exorcist | Priority |
|---------|---------|----------|----------|
| Position | Title bar | Bottom bar | Medium |
| Model details | Shows vendor + capabilities | Just model name | Low |
| Per-mode models | Yes | Yes (`m_perModeModel`) | ✅ Match |
| Capability display | Shows premium/preview badges | Not shown | Low |
| Custom model config | Full UI for custom endpoints | Via provider plugins | ✅ Adequate |

---

## 24. Menu & Dropdown Patterns

### VS Code Menu CSS

```css
.monaco-menu-option {
    font-size: 13px;
    padding: 0 10px;
    line-height: 26px;
    border-radius: 4px;
    gap: 8px;
}
/* Active item */
background: var(--vscode-editorActionList-focusBackground);
color: var(--vscode-editorActionList-focusForeground);
outline: 1px solid var(--vscode-menu-selectionBorder, transparent);
```

### Exorcist Menu/List CSS

```css
QListWidget {
    background: #252526;
    border: 1px solid #3e3e42;
    color: #cccccc;
}
QListWidget::item {
    padding: 5px 10px;
    font-size: 12px;
}
QListWidget::item:hover,
QListWidget::item:selected {
    background: #094771;
}
```

### Gaps

| Property | VS Code | Exorcist | Delta |
|----------|---------|----------|-------|
| Item height | `26px` (line-height) | ~22px (5+12+5 padding) | Close |
| Border-radius | `4px` per item | `0` | ❌ Should be `4px` |
| Font-size | `13px` | `12px` | ❌ Should be `13px` |
| Gap | `8px` | N/A | N/A |
| Selection border | `1px outline` | None | Low |

---

## 25. Keyboard Shortcuts

### VS Code Chat Shortcuts

| Key | Action | Context |
|-----|--------|---------|
| `Enter` | Send message | Chat input focused |
| `Shift+Enter` | New line | Chat input focused |
| `Ctrl+Shift+I` | Toggle chat panel | Global |
| `Ctrl+I` | Inline chat (editor) | Editor focused |
| `Up/Down` | Navigate history | Chat input focused |
| `Ctrl+Shift+.` | Add file reference (CLI) | Editor focus + CLI session |
| `Ctrl+Enter` | NES capture confirm | NES mode |
| `Escape` | Close inline chat / NES abort | Various |

### Exorcist Keyboard Shortcuts

| Key | Action | Implementation |
|-----|--------|---------------|
| `Enter` | Send (if no modifier) | `eventFilter` on m_input |
| `Shift+Enter` | New line | Default QPlainTextEdit |
| `Up/Down` | Navigate input history | `eventFilter` |
| `/` | Show slash menu | `eventFilter` |
| `@` | Show mention menu | `eventFilter` |
| `Escape` | Hide menus | `eventFilter` |

### Assessment

Core keyboard shortcuts are well-implemented. Global shortcuts (like `Ctrl+Shift+I` to toggle panel) would need to be registered at the `MainWindow` level.

---

## 26. Review System

### VS Code Code Review

The Copilot extension integrates a full code review system:

- **Trigger**: `github.copilot.chat.review` command or `/review` slash command
- **UI**: VS Code **Comments API** (`commentController: github-copilot-review`)
- **Comment thread actions**: Apply, Discard, Navigate (previous/next), Continue in Chat, Mark helpful/unhelpful
- **Menu structure**:
  - `comments/comment/title`: thumbsup/thumbsdown
  - `comments/commentThread/title`: previous/next/continue/discard
  - `comments/commentThread/additionalActions`: apply+next, discard+next submenus
- **File-level review**: Review staged changes, unstaged changes, or file changes
- **SCM integration**: Review button in Source Control title bar

### Exorcist Review

- `AgentIntent::CodeReview` intent
- Signal: `reviewAnnotationsReady(filePath, QList<QPair<int, QString>>)`
- Emits file annotations but UI for showing them in-editor is not fully implemented

### Gap: The full review experience (inline comments in editor, apply/discard per comment, navigate through review comments) is a major feature that requires editor integration beyond the chat panel.

---

## 27. Accessibility

### VS Code Accessibility

- All interactive elements have ARIA labels
- Keyboard navigation throughout (Tab, Shift+Tab, Enter)
- Screen reader support via VS Code's accessibility layer
- High contrast themes (HC Dark, HC Light) with distinct color tokens
- Focus indicators on all interactive elements (`var(--vscode-focusBorder)`)

### Exorcist Accessibility

- `QToolButton` widgets have tooltip texts
- Focus borders on input fields (`:focus { border-color: #007acc }`)
- No explicit ARIA/screen reader support (Qt handles some platform accessibility)
- No high contrast theme variant

### Gap: While Qt provides some platform-native accessibility, explicit accessible names, roles, and high-contrast theme support should be added.

---

## 28. Gap Analysis — Exorcist vs VS Code

### Critical Gaps (Must Fix)

| # | Gap | Description | Effort |
|---|-----|-------------|--------|
| 1 | **Syntax highlighting in code blocks** | Code blocks render as monochrome `<pre>` text. VS Code uses Monaco editor with full grammar-based highlighting. | High |
| 2 | **Inline diff preview** | No visual diff showing for proposed edits. VS Code shows colored diff inline. | High |
| 3 | **Per-file accept/reject** | Changes bar is all-or-nothing (Keep All / Undo All). VS Code allows per-file granular control. | Medium |

### High Priority Gaps

| # | Gap | Description | Effort |
|---|-----|-------------|--------|
| 4 | **Follow-up suggestions** | No follow-up suggestions after AI responses. VS Code shows contextual next-step links. | Medium |
| 5 | **Input area integration** | Mode selector, model picker, and attach button are in a separate bottom bar. VS Code integrates them inside the input container. | Medium |
| 6 | **Color system alignment** | Many colors are from Dark Default theme, not Dark Modern. Key mismatches: input bg, button bg, link color, description foreground. | Low |
| 7 | **Hover-reveal actions** | Feedback and message actions are always visible. VS Code shows them only on hover. | Low |

### Medium Priority Gaps

| # | Gap | Description | Effort |
|---|-----|-------------|--------|
| 8 | **Collapsible thinking blocks** | Thinking blocks are static. Should be collapsible with auto-close when response starts. | Low |
| 9 | **Collapsible tool calls** | Tool call cards don't expand/show results. Should be expandable. | Medium |
| 10 | **Slash command pills** | Slash commands typed as plain text. Should render as colored pills (`#34414b` bg, `#40a6ff` fg). | Low |
| 11 | **Context `#` completion** | No inline `#file`, `#selection` completion in input. | Medium |
| 12 | **Error welcome states** | Only one welcome state. Should show specialized messages for auth failure, offline, rate limit. | Low |
| 13 | **Auto-generated session titles** | Sessions don't get auto-titles. VS Code uses LLM to generate titles. | Low |
| 14 | **User avatar** | Static "U" letter. Should show real initials or profile picture. | Low |

### Low Priority / Future Gaps

| # | Gap | Description |
|---|-----|-------------|
| 15 | **SVG icons** | Unicode emoji/characters vs proper icon font. Consider Codicon subset. |
| 16 | **Code block IntelliSense** | No hover/go-to-definition in chat code blocks. |
| 17 | **Linkification** | No automatic linking of file paths and symbols in responses. |
| 18 | **Review in editor** | No inline review comments with apply/discard. |
| 19 | **MCP server integration** | No MCP server definition provider UI. |
| 20 | **Question carousel** | `questionCarousel` response type not supported. |
| 21 | **Chat hooks** | No chat hooks (PreToolUse, SessionStart, Stop lifecycle events). |
| 22 | **High contrast theme** | No HC Dark/HC Light theme variants. |

### Already Well-Implemented ✅

| Feature | Assessment |
|---------|------------|
| Message layout (table-based) | Functional, visually acceptable |
| Avatar sizing (24px circle) | Matches VS Code |
| Streaming with cursor | Working |
| Working indicator (shimmer) | Good — visually polished |
| Scrollbar styling | Close match |
| Input auto-resize | Working |
| Keyboard shortcuts (Enter/Shift+Enter/Up/Down) | Working |
| Slash command menu | Working |
| Mention menu | Working |
| Attachment chips | Working |
| Error differentiation (auth/rate/network) | Good |
| Session management (new/history/delete/rename) | Working |
| Per-mode model selection | Working |
| Provider tab system | Working (Exorcist-specific) |
| Tool confirmation dialog | Working |
| Three chat modes (Ask/Edit/Agent) | Working |
| Thinking block rendering | Working |
| Tool call status indicators | Working |
| Changes bar (Keep/Undo) | Working |
| Context window info | Working |
| Drag-and-drop file attachment | Working |

---

## Appendix A: VS Code CSS Variable Reference (Dark Modern Values)

For implementing theme-correct colors in Exorcist:

```
--vscode-editor-background:           #1f1f1f
--vscode-editor-foreground:           #cccccc
--vscode-sideBar-background:          #181818
--vscode-panel-background:            #181818
--vscode-panel-border:                #2b2b2b
--vscode-input-background:            #313131
--vscode-input-border:                #3c3c3c
--vscode-focusBorder:                 #0078d4
--vscode-button-background:           #0078d4
--vscode-button-foreground:           #ffffff
--vscode-button-hoverBackground:      #026ec1
--vscode-foreground:                  #cccccc
--vscode-descriptionForeground:       #9d9d9d
--vscode-errorForeground:             #f85149
--vscode-textLink-foreground:         #4daafc
--vscode-widget-border:               #313131
--vscode-chat-slashCommandBackground: #34414b
--vscode-chat-slashCommandForeground: #40a6ff
--vscode-textPreformat-foreground:    (code inline text)
--vscode-textPreformat-background:    (code inline bg)
--vscode-list-hoverBackground:        (list hover)
--vscode-scrollbar-shadow:            (scrollbar shadow)
```

## Appendix B: VS Code CSS Variable Reference (Dark Default Values)

Current Exorcist colors are closer to these:

```
--vscode-editor-background:           #1e1e1e
--vscode-sideBar-background:          #252526
--vscode-input-background:            #3c3c3c
--vscode-focusBorder:                 #007acc
--vscode-button-background:           #0e639c
--vscode-textLink-foreground:         #3794ff
```

## Appendix C: Complete VS Code Chat CSS Class Hierarchy

```
.monaco-workbench
  └── .chat-widget
      └── .interactive-session
          ├── .interactive-list
          │   └── .interactive-item-container.interactive-item-compact
          │       ├── .header
          │       │   ├── .avatar (24px, border-radius: 50%)
          │       │   ├── .username
          │       │   └── .detail (timestamp)
          │       ├── .body
          │       │   └── .value (markdown rendered content)
          │       │       ├── p, ul, ol, h1-h6
          │       │       ├── pre > code (code blocks → Monaco editor)
          │       │       ├── code (inline code)
          │       │       ├── a (links)
          │       │       ├── blockquote
          │       │       ├── table
          │       │       └── .tool-invocation-card
          │       ├── .followUps
          │       │   └── .interactive-session-followups
          │       │       └── .monaco-button (12px, link-colored)
          │       ├── .chat-notification-widget
          │       ├── .status
          │       │   ├── .label (11px)
          │       │   └── .actions
          │       └── .feedback-actions
          │           ├── .codicon-thumbsup
          │           ├── .codicon-thumbsdown
          │           ├── .codicon-copy
          │           └── .codicon-more
          ├── .interactive-input-part
          │   ├── .context-attachments
          │   │   └── .chat-attached-context-attachment
          │   ├── .interactive-input-and-execute-toolbar
          │   │   ├── .monaco-editor (input editor)
          │   │   ├── .chat-input-toolbars
          │   │   │   ├── .attach-context-button
          │   │   │   ├── .mode-selector
          │   │   │   └── .model-picker
          │   │   └── .execute-toolbar
          │   │       └── .submit-button
          │   └── .chat-input-progress
          │       └── .monaco-progress-container
          └── .chat-empty-state (welcome)
              ├── .chat-empty-state-title
              ├── .chat-empty-state-subtitle
              └── .chat-empty-state-suggestions
```

## Appendix D: Full Tool Catalog

### Tool Sets (from `contributes.languageModelToolSets`)

| Set | Icon | Tools |
|-----|------|-------|
| `edit` | `$(pencil)` | createDirectory, createFile, createJupyterNotebook, editFiles, editNotebook, rename |
| `execute` | — | runNotebookCell, testFailure |
| `read` | `$(eye)` | getNotebookSummary, problems, readFile, readNotebookCellOutput |
| `search` | `$(search)` | changes, codebase, fileSearch, listDirectory, searchResults, textSearch, searchSubagent, usages |
| `vscode` | — | getProjectSetupInfo, installExtension, memory, newWorkspace, runCommand, switchAgent, vscodeAPI |
| `web` | `$(globe)` | fetch, githubRepo |

### Individual Tools (30+)

| # | Reference Name | Display Name | Category |
|---|---------------|--------------|----------|
| 1 | `codebase` | Semantic Search | Search |
| 2 | `searchSubagent` | Search Subagent | Search |
| 3 | `symbols` | Workspace Symbols | Search |
| 4 | `fileSearch` | Find Files | Search |
| 5 | `textSearch` | Find Text | Search |
| 6 | `listDirectory` | List Directory | Search |
| 7 | `readFile` | Read File | Read |
| 8 | `problems` | Get Errors | Read |
| 9 | `changes` | Changed Files | Search |
| 10 | `applyPatch` | Apply Patch (V4A) | Edit |
| 11 | `insertEdit` | Insert Edit | Edit |
| 12 | `createFile` | Create File | Edit |
| 13 | `createDirectory` | Create Directory | Edit |
| 14 | `replaceString` | Replace String | Edit |
| 15 | `multiReplaceString` | Multi Replace | Edit |
| 16 | `editFiles` | Edit Files (placeholder) | Edit |
| 17 | `newWorkspace` | Create Workspace | VS Code |
| 18 | `getProjectSetupInfo` | Project Setup Info | VS Code |
| 19 | `installExtension` | Install Extension | VS Code |
| 20 | `runCommand` | Run Command | VS Code |
| 21 | `vscodeAPI` | VS Code API Docs | VS Code |
| 22 | `memory` | Persistent Memory | VS Code |
| 23 | `switchAgent` | Switch Agent | VS Code |
| 24 | `fetch` | Fetch Web Page | Web |
| 25 | `githubRepo` | GitHub Repo Search | Web |
| 26 | `testFailure` | Test Failure Info | Execute |
| 27 | `findTestFiles` | Find Test Files | Search |
| 28 | `getSearchResults` | Search View Results | Search |
| 29 | `createJupyterNotebook` | New Notebook | Edit |
| 30 | `editNotebook` | Edit Notebook | Edit |
| 31 | `runNotebookCell` | Run Cell | Execute |
| 32 | `getNotebookSummary` | Notebook Summary | Read |
| 33 | `readNotebookCellOutput` | Cell Output | Read |

## Appendix E: Agent Mode Definitions

### Ask Mode
- **Tools**: Read-only (search, read, web, vscode/memory, github issues, test failure)
- **Behavior**: Never modifies files. Explains, researches, answers questions.
- **Handoffs**: None

### Edit Mode
- **Tools**: read + edit only
- **Behavior**: Edits only active file + attached files. No terminal.
- **Handoffs**: "Continue with Agent Mode" → agent

### Agent Mode
- **Tools**: All tool sets (edit, execute, read, search, vscode, web)
- **Behavior**: Full autonomous coding agent. Creates files, runs commands, searches codebase.
- **Sub-agents**: Explore (fast search subagent)
- **Handoffs**: None (top-level)

### Plan Mode (via `switchAgent` tool)
- **Tools**: Read-only + Explore subagent + askQuestions
- **Behavior**: Researches and outlines multi-step plans without coding.
- **Phases**: Discovery → Alignment → Design → Refinement
- **Handoffs**: "Start Implementation" → agent, "Open in Editor" → agent

### Explore Sub-agent
- **Tools**: Read-only (DEFAULT_READ_TOOLS)
- **Behavior**: Fast codebase exploration, search-heavy, parallelized.
- **Not user-invocable**: Only callable by Agent/Plan modes.
- **Fallback models**: Claude Haiku 4.5, Gemini 3 Flash, Auto

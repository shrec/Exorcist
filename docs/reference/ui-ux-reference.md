# Exorcist IDE — UI/UX Reference Specification

## Reference Image

**File:** `docs/reference/vs-ui-reference.png`

This reference image shows the Visual Studio 2022 dark theme layout that Exorcist IDE must emulate in visual structure, panel arrangement, and overall aesthetic. The image is NOT committed to git (gitignored) but must exist locally for developers.

---

## Layout Specification (from reference screenshot)

### Overall Theme
- **Dark theme** with deep blue-gray backgrounds (#1e1e1e to #252526 range)
- High-contrast text on dark surfaces
- Subtle borders between panels (1px, slightly lighter than background)
- Professional, dense information layout — no wasted whitespace

### Main Regions (top → bottom, left → right)

#### 1. Title Bar
- Application title with project/file context
- Standard window controls (minimize, maximize, close)

#### 2. Menu Bar
- Horizontal menu bar below title: File, Edit, View, Git, Project, Build, Debug, Test, Analyze, Tools, Extensions, Window, Help
- Clean text-only menu items, no icons in menu bar

#### 3. Main Toolbar
- Below menu bar, single toolbar row
- Contains: navigation arrows (back/forward), build configuration dropdown (Debug/Release), platform dropdown (x64), debugger selector, start/stop debug buttons
- Compact icon + text buttons, grouped by function with subtle separators

#### 4. Left Sidebar — Solution Explorer
- **Docked left**, collapsible tree view
- Header with "Solution Explorer" title + toolbar icons (sync, collapse all, properties, etc.)
- Search/filter input at top of tree
- Hierarchical tree: Solution → Project → folders → files
- File icons differentiated by type (.cpp, .h, etc.)
- Indentation guides for tree depth
- Right-click context menu support

#### 5. Center — Code Editor Area
- **Tabbed document interface** — multiple file tabs at top
- Active tab highlighted, inactive tabs dimmer
- Close button on each tab
- Syntax-highlighted code with:
  - Line numbers in left gutter (gray on dark)
  - Color scheme: keywords (blue), types (teal/green), strings (red/brown), comments (green), preprocessor (gray/purple), constants (light blue)
  - Current line highlight (subtle background change)
  - Code folding controls in gutter
- Find/Replace overlay dialog:
  - Floating panel at top of editor
  - Search input + options (case, whole word, regex)
  - Match count display
  - Scope selector (Current Document / Entire Solution)

#### 6. Right Sidebar — Tool Panels
- **Stacked panels** OR **tabbed panels**
- Panels shown in reference:
  - **Toolbox** — collapsible sections with UI components (General group)
  - **GitHub Copilot Chat** — AI assistant panel with:
    - Chat input at bottom
    - Suggested actions (Summarize, Write unit tests, Fix my code)
    - Reference (+) button for context attachment
    - Model selector dropdown at bottom
    - New Thread button at top
- Panels are resizable with drag handles

#### 7. Bottom Panel Area — Output/Terminal Tabs
- **Tabbed panel** at bottom, spans full width
- Visible tabs in reference:
  - Package Manager Console
  - Developer PowerShell (multiple instances)
  - Output
  - GitHub Copilot Terminal
  - Find Symbol Results
  - Error List
  - Search
  - Properties
  - Toolbox
  - Notifications
- Each tab can host:
  - Terminal emulator (PowerShell, bash)
  - Output log view
  - Search results list
  - Problem/error list table
- Tab overflow handled with scroll or dropdown

#### 8. Status Bar (bottom-most)
- **Full-width status bar** at very bottom
- Left side: ready state, background task indicators
- Right side: line/column display (Ln: X, Ch: Y), encoding (UTF-8), line ending (CRLF), indent style (TABS), language mode, git branch, notification indicators

---

## Key UX Principles to Follow

1. **Density over minimalism** — Show maximum useful information. Professional users want data, not whitespace.
2. **Consistent dark theme** — All panels share the same dark color palette. No bright or jarring panel backgrounds.
3. **Tabs everywhere** — Documents are tabbed. Bottom panels are tabbed. Side tool windows can be tabbed.
4. **Dockable everything** — Panels can be docked left, right, bottom, or float. Users can rearrange freely.
5. **Search is prominent** — Solution Explorer has its own search. Global find/replace overlays the editor. Search results have their own bottom tab.
6. **AI integration is a side panel** — Copilot Chat lives in the right sidebar as a dockable panel, not a modal or overlay.
7. **Multiple terminals** — Users can have many terminal tabs open simultaneously with different shells.
8. **Non-intrusive notifications** — Notification area in status bar, not popup toasts.
9. **Context keeps position** — When switching between files/tabs, scroll position and cursor are preserved.
10. **Compact toolbars** — Toolbar buttons are small, icon-primary, with text only for critical items like build config.

---

## Color Palette Reference (approximate from screenshot)

| Element | Color (approx) |
|---------|----------------|
| Background (editor) | `#1e1e1e` |
| Background (sidebar) | `#252526` |
| Background (title/menu bar) | `#2d2d30` |
| Background (status bar) | `#007acc` (accent blue) |
| Text (primary) | `#d4d4d4` |
| Text (secondary/dimmed) | `#808080` |
| Keyword (syntax) | `#569cd6` (blue) |
| Type (syntax) | `#4ec9b0` (teal) |
| String (syntax) | `#ce9178` (brown/orange) |
| Comment (syntax) | `#6a9955` (green) |
| Number/Constant | `#b5cea8` (light green) |
| Preprocessor | `#9b9b9b` (gray) |
| Line numbers | `#858585` |
| Selection highlight | `#264f78` |
| Current line | `#2a2d2e` |
| Tab active | matches editor background |
| Tab inactive | slightly darker than active |
| Border/separator | `#3c3c3c` |

---

## Agent Instruction

When implementing any UI component, panel, dock widget, toolbar, status bar item, or visual element in Exorcist IDE:

1. **Read this specification first** — ensure your UI matches the reference layout and aesthetic.
2. **Check the reference image** at `docs/reference/vs-ui-reference.png` for visual verification.
3. **Use the dark color palette** defined above. Never use light/white backgrounds.
4. **Follow the panel arrangement** — left sidebar for explorers, right sidebar for tools, bottom for terminals/output.
5. **Maintain information density** — prefer compact, data-rich layouts over spacious minimalist design.

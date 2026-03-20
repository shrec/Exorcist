# Exorcist Manifesto

This document is the project's spine. Every major decision must align with it.

## 1) Native, Not Web

- The UI runs on native Qt widgets, not a browser engine.
- No Electron, no Chromium, no JavaScript runtime.
- Desktop tools deserve desktop-class performance. A code editor is not a web app.

## 2) Predictability Is a Feature

- The IDE must never freeze the UI thread on I/O, indexing, or search.
- All background jobs must be visible in the status bar. No hidden activity.
- There are no "Extension Host Terminated" dialogs. If a plugin crashes, the IDE recovers silently and logs the error.
- Restart is not a valid solution to a problem.

## 3) Minimal Footprint

- Cold start target: under 300 ms on a mid-range machine.
- Idle RAM: under 80 MB for an empty session.
- Large file open (100 MB): under 2 seconds.
- These are hard targets, not aspirations. Measure continuously.

## 4) Keyboard-Centric Workflow

- Every frequent action must be reachable by keyboard.
- Command Palette (Ctrl+Shift+P) is the primary interaction surface.
- File quick-open (Ctrl+P) must be instant and fuzzy.
- Mouse is optional, not required.

## 5) Modularity Over Monolith

- Core interfaces define the system; implementations are replaceable.
- UI, features, and plugins depend only on stable APIs.
- No internal headers in plugins.
- Adding a feature must not require touching unrelated modules.

## 6) Cross-Platform First

- Every feature must be portable by design.
- Platform code stays isolated behind core bindings.
- No hard OS assumptions in UI or feature layers.

## 7) Editor Architecture Done Right

- Separate document model (TextBuffer) from view (EditorView) and UI state.
- The text engine is incremental: edits never copy the whole document.
- Prefer proven parsers (tree-sitter) over ad-hoc regex for language features.

## 8) Language Intelligence Without Chaos

- LSP is the default contract for language features.
- Language servers run out-of-process. A crashing language server does not crash the IDE.
- Clear lifecycle: start, stop, timeout, restart — all observable.

## 9) Transparent Defaults

- No network access without explicit user action.
- No telemetry, no analytics, no phoning home.
- No background indexing that wasn't asked for.
- The user always knows what the IDE is doing.

## 10) Layered Ownership (RAII)

- Ownership is explicit and local.
- Qt parent-child ownership for UI objects.
- `std::unique_ptr` / `std::shared_ptr` for core services.
- No global singletons. ServiceRegistry is the DI root.

## 11) AI Is Optional, Not Foundational

- The IDE must be fully usable without any AI feature.
- AI is a plugin layer, not a core dependency.
- The editor core is not coupled to any AI provider.

## 12) Self-Hosting Early

- Build, run, and edit Exorcist inside Exorcist as early as possible.
- Dogfooding is the most honest performance and usability test.

## 13) Stable Plugin SDK

- Small, versioned interfaces. IPlugin/1.0, IAIProvider/1.0.
- Backward compatibility is a core requirement.
- Plugin crashes are caught and logged; they never bring down the host.

## 14) Quality and Longevity

- Clear code, minimal magic.
- Every non-trivial public API gets a test.
- The codebase must be readable by a new contributor without a tour guide.

## 15) Developer Pain First

- Exorcist exists to reduce developer pain, not to accumulate fashionable features.
- Features must justify their cost by removing friction from real workflows.
- Practicality beats novelty when the two conflict.
- Shared pain points are solved once in shared infrastructure, not re-solved in every plugin.
- Developer-friendly plugin authoring is part of the product, not a side concern.

---

## Performance Budget (hard targets)

| Metric                        | Target        |
|-------------------------------|---------------|
| Cold start (empty session)    | < 300 ms      |
| Idle RAM                      | < 80 MB       |
| Open 100 MB file              | < 2 s         |
| In-file search (1 MB)         | < 50 ms       |
| Project search (10k files)    | < 3 s         |
| UI frame time (scroll/type)   | < 16 ms       |

## Architecture Rules (non-negotiable)

- UI thread never blocks on file I/O, indexing, or network.
- Editor core (`TextBuffer`, `PieceTableBuffer`) has zero Qt Widget dependency.
- Plugin `initialize()` / `shutdown()` are always wrapped in try/catch.
- Every background job updates the status bar while running.
- `QDir::currentPath()` is never used as a project root outside of initialization.

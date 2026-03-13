# Dependency Notes

The core goal is to keep dependencies minimal and permissive. The list below is
for optional acceleration; nothing is bundled yet.

## Bundled (via FetchContent)
- **tree-sitter** v0.25.3 (MIT): incremental parsing engine for syntax highlighting. Core library + 10 grammar packages (C, C++, Python, JavaScript, TypeScript, Rust, JSON, Go, YAML, TOML). Controlled by `EXORCIST_USE_TREESITTER` CMake option (ON by default).
- **LuaJIT** (MIT): scripting runtime for Lua plugins.
- **Ultralight** v1.4.0-beta (free for open source — [license](https://ultralig.ht/pricing/)): lightweight HTML/CSS/JS renderer for the chat panel. WebKit fork with JavaScriptCore, ~10 MB RAM. CPU rendering via BitmapSurface→QImage. Controlled by `EXORCIST_USE_ULTRALIGHT` CMake option (OFF by default). Pre-download SDK to `third_party/ultralight-sdk/` or let FetchContent grab it.

## Candidates (permissive)
- tree-sitter (MIT): ~~fast incremental parsing and syntax highlighting.~~ **Bundled.**
- rapidfuzz (MIT): fuzzy search for quick open and command palette.
- nlohmann/json (MIT): lightweight JSON parsing for settings.
- sqlite (Public Domain): workspace indexing and metadata caching.
- libssh2 (BSD-style): SSH connections for remote builds.

## Candidates (review license before use)
- libgit2 (GPLv2 with linking exception): Git integration.
- ksyntaxhighlighting (LGPL-2.1): syntax highlighting engine.

## LLVM
LLVM is used for future language tooling integration. It is optional for now.
Enable with `-DEXORCIST_USE_LLVM=ON` and set `LLVM_DIR` if needed.

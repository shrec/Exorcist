# Dependency Notes

The core goal is to keep dependencies minimal and permissive. The list below is
for optional acceleration; nothing is bundled yet.

## Candidates (permissive)
- tree-sitter (MIT): fast incremental parsing and syntax highlighting.
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

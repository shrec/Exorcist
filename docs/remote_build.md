# Terminal and Remote Build (Draft)

## Terminal Integration
Goal: a fast terminal panel for local commands with minimal overhead.

MVP approach:
- Embed a terminal panel widget in the UI.
- Start with local shell via `QProcess` (simple, cross-platform).
- Later upgrade to PTY-based backend for full terminal emulation.

## SSH and Remote Build
Goal: build and run tasks on remote machines (including RISC-V).

Options:
- Use system `ssh` client (simpler, fewer dependencies).
- Use `libssh2` for direct control and key management.

MVP workflow:
1) Configure remote host + key path.
2) Send build commands over SSH.
3) Stream logs back into terminal panel.

## Future
- Connection profiles per workspace.
- File sync (rsync or SFTP).
- Remote debugging and task runner presets.

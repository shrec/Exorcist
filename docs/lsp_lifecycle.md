# LSP Lifecycle (clangd)

## Goals
- Stable diagnostics and completions.
- Predictable CPU/RAM usage.
- Safe recovery on crashes.

## Lifecycle States
- Stopped -> Starting -> Ready -> Failed -> Restarting

## Startup
- Start clangd per workspace.
- Initialize with root URI + compile commands path.
- Set debounce for document changes.

## Steady State
- Incremental text sync only.
- Request queue with cancellation.
- Background indexing throttled.

## Failure Handling
- Detect crash or non-responsive server.
- Exponential backoff restart.
- Preserve UI responsiveness.

## Configuration
- Workspace settings for flags and compile_commands.json location.
- Opt-in background index for huge repos.

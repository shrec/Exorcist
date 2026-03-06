# Search Framework (VS-Level Plan)

Goal: fast, reliable project search that scales to millions of lines.

## Architecture
- Query parser: literal/regex, case, whole-word.
- Execution pipeline:
  1) Trigram prefilter (fast candidate files).
  2) Regex or literal verification.
  3) Streaming results (cancelable).
- Indexer:
  - Incremental updates via file watcher.
  - Persistence (SQLite or custom binary).

## MVP Steps
1) SearchService API and UI wiring.
2) Background scan worker (multi-threaded).
3) Trigram index (in-memory).
4) Optional on-disk index.

## Performance Targets
- 100k files: first results < 300ms.
- Cancel responsive < 50ms.

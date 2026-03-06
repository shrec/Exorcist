# Recommendations (Future-Proofing)

Track these in Phase 1-2 so we don't bake in avoidable debt.

## Performance Budget
- Define max binary size and startup time budgets.
- Add build artifacts reporting (size, cold start, warm start).

## Service Registry Versioning
- Add registry versioning with contract metadata.
- Enforce dependency compatibility at plugin load.

## Plugin Safety
- Wrap plugin initialization with exception handling.
- Fail fast with clear error logging.

## Core Interfaces Expansion
- Add file watching, environment variables, and path utilities.
- Define boundaries so platform code stays isolated.

## CMake Integration Notes
- Keep CMake integration simple in Phase 1.
- Document required presets and toolchain paths in README.

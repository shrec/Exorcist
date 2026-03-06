---
description: "Add a new dependency to the Exorcist project after license and necessity evaluation"
agent: "developer"
argument-hint: "Library name and purpose, e.g. 'tree-sitter for incremental syntax parsing'"
---
Evaluate and integrate a new dependency:

1. **License check**: Verify the library is MIT, BSD, or public-domain
2. **Necessity**: Confirm no existing Qt 6 API covers the use case
3. **Integration**:
   - Add to root `CMakeLists.txt` with `find_package` or `FetchContent`
   - Use a CMake option for optional dependencies (e.g., `EXORCIST_USE_<NAME>`)
   - Update `src/CMakeLists.txt` with conditional linking
4. **Documentation**: Update [docs/dependencies.md](docs/dependencies.md) with:
   - Library name, version, license
   - Purpose and which component uses it
   - Whether it's required or optional
5. **Abstraction**: If wrapping an external library, create an interface in `src/core/`

Reference: [Dependencies policy](docs/dependencies.md)

# Recommendations (Future-Proofing)

Track these in Phase 1-2 so we don't bake in avoidable debt.

## Performance Budget  ✅
- ✅ `StartupProfiler` refactored: header/source split, budget enforcement in `finish()`.
- ✅ Budget constants from manifesto: startup < 300 ms, idle RSS < 80 MB.
- ✅ CMake post-build binary size reporting via `cmake/report_binary_size.cmake`.
- ✅ Config-aware budgets: 50 MB Release, 200 MB Debug (includes debug symbols).
- ✅ Warnings emitted at build time if binary exceeds budget.
- ✅ Startup profiler warns at runtime if startup time or RSS exceed budgets.

## Service Registry Versioning  ✅
- ✅ `ServiceContract` struct with major/minor version + description.
- ✅ `registerService(name, service, contract)` overload.
- ✅ `isCompatible(name, requiredMajor, minMinor)` compatibility check.
- ✅ `hasService(name)` query.
- ✅ Backward compatible — legacy `registerService(name, service)` defaults to v1.0.
- ✅ 15 tests in `test_serviceregistry`.

## Plugin Safety  ✅
- ✅ Plugin initialization wrapped with `try/catch(std::exception)` + `catch(...)`.
- ✅ Errors collected in `PluginManager::errors()`.
- ✅ Shutdown also wrapped.

## Core Interfaces Expansion  ✅
- ✅ `IFileWatcher` + `QtFileWatcher` — file/directory change monitoring.
- ✅ `IEnvironment` + `QtEnvironment` — environment variables, platform detection.
- ✅ 13 tests in `test_coreinterfaces`.
- Path utilities not added — Qt's `QDir`/`QFileInfo` sufficient.

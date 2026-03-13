# Exorcist IDE — Core Philosophy Alignment Audit

> თარიღი: 2026-03-13
> წყარო: კოდის პირდაპირი ანალიზი — mainwindow.h/cpp, bootstraps, plugins, CMakeLists.txt
> მიზანი: Core IDE vs Core Plugins ფილოსოფიასთან შესაბამისობის შემოწმება

---

## Executive Summary

| მეტრიკა | მიმდინარე | სამიზნე | სტატუსი |
|----------|-----------|---------|---------|
| MainWindow members | **117** | <40 | ❌ |
| Core-ში ჩახურული plugin-კანდიდატი | **6 subsystem** | 0 | ❌ |
| Plugin activation model | Eager-load all | Lazy/Project/Contextual | ❌ |
| Zero-cost guarantee | არ არსებობს | 0 CPU/RAM/threads | ❌ |
| Agent callbacks coupling | 45+ hardcoded | Dynamic from services | ❌ |
| Contribution interfaces wired | 2/10 | 10/10 | ❌ |
| Abstract interfaces for extractable subsystems | 1/6 | 6/6 | ❌ |

---

## 1. რა არის Core-ში, რაც Plugin უნდა იყოს?

### 1.1 Build System (`src/build/`) — **MUST EXTRACT**

| ფაილი | ტიპი | ხაზები |
|-------|------|--------|
| `cmakeintegration.h/.cpp` | CMake lifecycle | ~400 |
| `toolchainmanager.h/.cpp` | Toolchain detection | ~350 |
| `buildtoolbar.h/.cpp` | Build UI toolbar | ~200 |
| `debuglaunchcontroller.h/.cpp` | Build-before-debug | ~250 |
| `outputpanel.h/.cpp` | Build output | ~500 |
| `runlaunchpanel.h/.cpp` | Run/launch config | ~300 |

**პრობლემა:** BuildDebugBootstrap აერთიანებს Build + Debug-ს ერთ bootstrap-ში.
`restartClangd()` სიგნალი Build→LSP cross-subsystem coupling-ს ქმნის.

**Activation model:** Project detection — `CMakeLists.txt`, `Cargo.toml`, `Makefile`, `build.gradle`
**Abstract interface:** ❌ `IBuildSystem` არ არსებობს

---

### 1.2 Debug System (`src/debug/`) — **MUST EXTRACT**

| ფაილი | ტიპი | ხაზები |
|-------|------|--------|
| `idebugadapter.h` | Abstract interface | ~100 |
| `gdbmiadapter.h/.cpp` | GDB/MI implementation | ~800 |
| `debugpanel.h/.cpp` | Debug UI (5 tabs) | ~600 |
| `quickwatchdialog.h/.cpp` | Watch expression dialog | ~150 |
| `watchtreemodel.h/.cpp` | Watch tree data model | ~200 |

**Activation model:** Contextual — F5 / debug start
**Abstract interface:** ✅ `IDebugAdapter` არსებობს
**Coupling:** 6 agent callback + BuildDebugBootstrap-ში Debug objects

---

### 1.3 Testing System (`src/testing/`) — **MUST EXTRACT**

| ფაილი | ტიპი | ხაზები |
|-------|------|--------|
| `testdiscoveryservice.h/.cpp` | CTest JSON discovery | ~200 |
| `testexplorerpanel.h/.cpp` | Test explorer UI | ~300 |

**Activation model:** Contextual — test explorer panel გახსნაზე
**Abstract interface:** ❌ `ITestRunner` არ არსებობს
**Coupling:** CMakeIntegration::buildDirectory() — Build-ზეა დამოკიდებული

---

### 1.4 Remote/SSH System (`src/remote/`) — **MUST EXTRACT**

| ფაილი | ტიპი | ხაზები |
|-------|------|--------|
| `sshsession.h/.cpp` | SSH command execution | ~400 |
| `sshconnectionmanager.h/.cpp` | Profile CRUD, lifecycle | ~300 |
| `remotefilepanel.h/.cpp` | Remote file browser UI | ~400 |
| `remotefilesystemmodel.h/.cpp` | Remote file tree model | ~200 |
| `remotehostinfo.h/.cpp` | Host detection (arch/OS) | ~250 |
| `remotelspmanager.h/.cpp` | Remote LSP bridging | ~300 |
| `remotesyncservice.h/.cpp` | rsync/scp sync | ~200 |
| `sshprofile.h` | Profile struct | ~50 |

**Activation model:** Manual — SSH profile სეთაპზე
**Abstract interface:** ❌ `IRemoteConnection` არ არსებობს
**Coupling:** ✅ **ZERO** — არცერთ სხვა subsystem-ზე არ არის დამოკიდებული

---

### 1.5 GitHub Integration (`src/github/`) — **MUST EXTRACT**

| ფაილი | ტიპი | ხაზები |
|-------|------|--------|
| `ghservice.h/.cpp` | `gh` CLI wrapper | ~200 |
| `ghpanel.h/.cpp` | GitHub issues/PRs panel | ~250 |
| `ghbootstrap.h/.cpp` | Bootstrap (unused!) | ~50 |

**Activation model:** Project detection — `.git/` + `gh` CLI available
**Abstract interface:** ❌ არ არსებობს
**Coupling:** ✅ **ZERO** — header ჩართულია MainWindow-ში, მაგრამ არასდროს instantiate ხდება

---

### 1.6 Language-Specific LSP (Clangd) — **SHOULD EXTRACT**

| ფაილი | ტიპი | ხაზები |
|-------|------|--------|
| `clangdmanager.h/.cpp` | Clangd process lifecycle | ~300 |

**პრობლემა:** `LspClient` არის generic LSP protocol implementation — Core-ში რჩება.
`ClangdManager` არის C++-specific — Plugin უნდა იყოს.
**Activation model:** Project detection — `.cpp`, `.h`, `compile_commands.json`
**Abstract interface:** ✅ `LspClient` generic-ია, Clangd-ს interface არ სჭირდება — plugin-ი LspClient API-ს იყენებს

---

### 1.7 Language Highlighting Data (`src/editor/languages/`) — **SHOULD EXTRACT**

| ფაილი | ენა |
|-------|-----|
| `lang_cpp.cpp` | C/C++ |
| `lang_python.cpp` | Python |
| `lang_javascript.cpp` | JavaScript |
| `lang_web.cpp` | HTML/CSS |
| `lang_jvm.cpp` | Java/Kotlin |
| `lang_systems.cpp` | Go/Rust |
| `lang_scripting.cpp` | Perl/Ruby |
| `lang_shell.cpp` | Bash/Zsh |
| `lang_data.cpp` | YAML/JSON/TOML |
| `lang_config.cpp` | Config formats |

**~800 ხაზი** ენის მონაცემები hardcoded Core-ში.
**Activation model:** Project detection — ფაილის გაფართოების მიხედვით
**რეკომენდაცია:** `ILanguageContributor` interface-ით plugin-ად გამოტანა

---

## 2. MainWindow God Object — 117 Member Variables

### კატეგორიების მიხედვით

| კატეგორია | members | Core-ში რჩება? |
|-----------|---------|----------------|
| Core Editor (tabs, tree, search) | 5 | ✅ |
| Build/CMake | 6 | ❌ → Plugin |
| Debug | 2 | ❌ → Plugin |
| Remote/SSH | 3 | ❌ → Plugin |
| LSP/Clangd | 3 | ⚠️ LspClient Core, Clangd Plugin |
| Git | 2 | ✅ Core (universally useful) |
| Agent/AI | 7 | ✅ Core runtime |
| Search/Index | 3 | ✅ Core |
| Terminal | 1 | ✅ Core |
| Inline Completion | 3 | ✅ Core |
| MCP | 2 | ✅ Core |
| Panels/Dialogs | 14 | ⚠️ lazy-load |
| Theme | 1 | ✅ Core |
| UI Infrastructure | 4 | ✅ Core |
| Services/Util | 14 | ✅ Core |
| **Plugin-extractable** | **~14** | ❌ |

**Plugin extraction-ით MainWindow: 117 → ~103**
**Lazy-load panels-ით: 103 → ~89**
**EditorManager/DockBootstrap extraction-ით: 89 → ~50**

---

## 3. Bootstrap Coupling Analysis

### BuildDebugBootstrap — **უნდა გაიყოს**

```
BuildDebugBootstrap
  ├── Build objects (4): ToolchainMgr, CMakeIntegration, BuildToolbar, DebugLauncher
  ├── Debug objects (2): GdbMiAdapter, DebugPanel
  ├── Output (1): OutputPanel
  └── CROSS-COUPLING:
      ├── CMakeIntegration::configureFinished → restartClangd() ← LSP boundary violation
      └── DebugLaunchController::setToolchainManager() ← Build→Debug coupling
```

**რეკომენდაცია:**
1. გაყოფა `BuildBootstrap` + `DebugBootstrap`
2. `restartClangd()` → ServiceRegistry event მექანიზმით
3. DebugLauncher → IBuildSystem service lookup (optional)

### AIServicesBootstrap — **30+ შიდა სერვისი**

AIServicesBootstrap 30+ სერვისს ქმნის. ეს არ არის plugin-კანდიდატი — ეს არის agent platform-ის შიდა infrastructure. მაგრამ ბევრი მათგანი lazy-init შეიძლება იყოს:

| Lazy-init კანდიდატი | რატომ |
|---------------------|-------|
| TestScaffold | არ სჭირდება test UI-ის გარეშე |
| MergeConflictResolver | არ სჭირდება merge conflict-ის გარეშე |
| TrajectoryRecorder | debug feature — default off |
| InlineReviewWidget | არ სჭირდება review-ის გარეშე |
| NotebookManager | stubs only — can skip |

---

## 4. Agent Callback System — 45+ Coupling Points

Agent platform-ის callback struct არის **ყველაზე დიდი coupling vector**.
MainWindow ავსებს 45+ lambda-ს, რომლებიც პირდაპირ მიმართავს subsystem objects-ს.

### Plugin-extractable callbacks

| Callback Group | Count | Target Plugin |
|----------------|-------|---------------|
| Build/Test | 4 | Build Plugin |
| Debug | 6 | Debug Plugin |
| LSP-specific (Clangd) | 7 | Language Plugin |
| Tree-sitter | 5 | Already Core (grammars bundled) |

### რეკომენდაცია: Dynamic Callback Resolution

```
ახლანდელი მოდელი:
  MainWindow → ავსებს callbacks struct → AgentPlatformBootstrap::initialize(callbacks)
  
სამიზნე მოდელი:
  AgentPlatformBootstrap → resolves services from ServiceRegistry
  Build Plugin → registers IBuildService → agent tools auto-discover
  Debug Plugin → registers IDebugService → agent tools auto-discover
  LSP Plugin → registers ILspService → agent tools auto-discover
```

ეს ნიშნავს რომ agent tools-ის callback model-ი უნდა შეიცვალოს:
- **ახლა:** static Callbacks struct → 45+ function pointers
- **სამიზნე:** ServiceRegistry dynamic lookup → tools check service existence at runtime

---

## 5. Plugin System Infrastructure Gaps

### 5.1 Activation Model — **არ არის იმპლემენტირებული**

| Feature | სტატუსი |
|---------|---------|
| Manual enable/disable | ❌ Gallery UI-ში ღილაკი აქვს, runtime-ში არ მუშაობს |
| Project detection activation | ❌ Manifest-ში `workspaceContains` პარსავს, არ ასრულებს |
| Contextual (lazy) activation | ❌ `activationEvents` პარსავს, არ ეხება |
| Zero-cost guarantee | ❌ ყველა plugin ყოველთვის იტვირთება |

### 5.2 Contribution Interfaces — 2/10 Wired

| Interface | Wired? | საჭირო? |
|-----------|--------|---------|
| ICommandHandler | ✅ | ✅ |
| IViewContributor | ✅ | ✅ |
| ICompletionContributor | ❌ | ✅ Language packs-თვის |
| IEditorContributor | ❌ | ✅ Editor extensions-თვის |
| ILanguageContributor | ❌ | ✅ **CRITICAL** — language pack extraction-თვის |
| IFormatterContributor | ❌ | ✅ Formatter plugins-თვის |
| ILinterContributor | ❌ | ⚠️ medium priority |
| ISettingsContributor | ❌ | ✅ Plugin settings UI-თვის |
| ITaskContributor | ❌ | ✅ Build task plugins-თვის |
| IThemeContributor | ❌ | ⚠️ medium priority |

### 5.3 C++ Plugin Manifests — **არ არსებობს**

AI provider plugins-ს (`copilot`, `claude`, `codex`, `ollama`, `byok`) არ აქვთ `plugin.json`.
ისინი declarative contributions ვერ აკეთებენ — მხოლოდ C++ API-თი ერთვებიან.

---

## 6. Cross-Subsystem Dependency Matrix

```
              BUILD   DEBUG   TEST   REMOTE  GITHUB  LSP    AGENT
BUILD          —      ✅       —       —       —     ✅(!)    ✅
DEBUG         ✅       —       —       —       —      —      ✅
TEST          ✅       —       —       —       —      —      ✅
REMOTE         —       —       —       —       —      —       —
GITHUB         —       —       —       —       —      —       —
LSP            —       —       —       —       —      —      ✅
AGENT         ✅      ✅      ✅       —       —     ✅       —
MAINWINDOW    ✅      ✅      ✅      ✅      ✅     ✅      ✅

✅ = hardcoded dependency
(!) = restartClangd signal violation
```

---

## 7. რეკომენდირებული Extraction Order

### Phase A: Zero-Dependency Extractions ✅ DONE

| # | Subsystem | Effort | Status |
|---|-----------|--------|--------|
| A1 | **GitHub** → `plugins/github/` | Minimal | ✅ Extracted |
| A2 | **Remote/SSH** → `plugins/remote/` | Low | ✅ Extracted |

### Phase B: Infrastructure Prerequisites ✅ (commit `5caca2a`)

| # | სამუშაო | Effort | Status |
|---|---------|--------|--------|
| B1 | `IBuildSystem` SDK interface + BuildSystemService adapter | Medium | ✅ Done |
| B2 | `ITestRunner` SDK interface + TestRunnerService adapter | Low | ✅ Done |
| B3 | `BuildDebugBootstrap` → `BuildBootstrap` + `DebugBootstrap` | Medium | ✅ Done |
| B4 | Agent callbacks → IBuildSystem ServiceRegistry lookup | High | ✅ Done |
| B5 | Plugin activation model (lazy, workspace, event-based) | High | ✅ Done |
| B6 | ContributionRegistry wires languages/tasks/settings/themes | Medium | ✅ Done |

### Phase C: Subsystem Extractions (AFTER B)

| # | Subsystem | Depends On |
|---|-----------|------------|
| C1 | **Build System** | B1, B3, B4 |
| C2 | **Testing System** | B2, C1 |
| C3 | **Debug System** | B3, B4, C1 |
| C4 | **Clangd Manager** | B4 |
| C5 | **Language Highlighting Data** | B6 (ILanguageContributor wiring) |

### Phase D: God Object Decomposition

| # | სამუშაო | Impact |
|---|---------|--------|
| D1 | `EditorManager` extraction (tab/doc lifecycle) | -10 members |
| D2 | `DockBootstrap` extraction (dock creation/layout) | -14 members |
| D3 | Lazy panel initialization | startup speedup |

---

## 8. Critical Path

```
A1 + A2 (GitHub + Remote) ─────────── შეიძლება PARALLEL
    │
    ▼
B1-B6 (Infrastructure) ────────────── SEQUENTIAL
    │
    ▼
C1 (Build) → C2 (Testing) ─────────── SEQUENTIAL
C3 (Debug)  → PARALLEL with C1 after B3
C4 (Clangd) → PARALLEL after B4
C5 (Languages) → PARALLEL after B6
    │
    ▼
D1-D3 (God Object Decomp) ─────────── LAST
```

---

## 9. Risk Assessment

| რისკი | სიმძიმე | mitigation |
|-------|---------|------------|
| Agent callback refactor ტეხავს 79 tools-ის wiring-ს | **HIGH** | incremental migration — ჯერ optional check, შემდეგ cleanup |
| Build↔Debug coupling ძნელი გასაყოფი | **MEDIUM** | IBuildSystem service lookup, არა direct reference |
| Plugin lazy-load ტეხავს startup sequence-ს | **MEDIUM** | deferredInit() უკვე async-ია, გამოიყენება |
| Language data extraction-ის შემდეგ highlighting არ მუშაობს | **LOW** | ILanguageContributor registered before first file open |
| Test coverage gaps | **MEDIUM** | ყოველი extraction step-ისთვის regression tests |

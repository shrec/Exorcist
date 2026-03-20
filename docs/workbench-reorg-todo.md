# Workbench Reorganization Todo

This is the execution todo for the canonical model defined in:

- [system-model.md](system-model.md)
- [plugin-model.md](plugin-model.md)
- [template-roadmap.md](template-roadmap.md)

## Phase A — Shell and Shared Layers

- [x] Remove shell-owned default toolbar.
- [x] Move build/debug/language menu ownership into plugins.
- [x] Introduce shared workbench base plugin layer.
- [x] Establish canonical docs and rules for shell/container architecture.
- [x] Extract shared service bootstrap for `WorkspaceSettings`, `ProfileManager`, and `ProjectTemplateRegistry`.
- [x] Collapse shell dock creation onto a single registration helper instead of repeated concrete `ExDockWidget` wiring.
- [x] Move core workbench command registration into a dedicated bootstrap.
- [x] Move remaining reusable non-visual workbench services out of `MainWindow`.
  - `ThemeManager`, `KeymapManager`, `FileWatchService` → `WorkbenchServicesBootstrap`;
    registered as `"themeManager"`, `"keymapManager"`, `"fileWatcher"` in ServiceRegistry.
- [x] Move the first duplicated visual panels (`DebugPanel`, `TestExplorerPanel`) onto shared component sources.

## Phase B — Plugin Standardization

- [x] Normalize `build` plugin onto `WorkbenchPluginBase`.
- [x] Normalize `cpp-language` plugin onto `WorkbenchPluginBase`.
- [x] Normalize `debug` plugin onto the same workbench base layer.
- [x] Normalize `testing` plugin onto the same workbench base layer.
- [x] Normalize `git` and other remaining bundled workbench plugins — `GitPanel`, `DiffExplorerPanel`, `MergeEditor` extracted to `plugins/git/` using `WorkbenchPluginBase` + `IViewContributor`; AI commit-message and conflict-resolution wiring moved into the plugin.
- [x] Normalize `remote` and `github` onto the shared workbench base layer.
- [x] Define one manifest/activation standard for bundled and third-party plugins.

## Phase C — Shared Components and Services

- [x] Define the first shared-components cut (terminal + search extracted):
  - **Terminal** → `plugins/terminal/` using `WorkbenchPluginBase` + `IViewContributor`;
    `ITerminalService` extended with `selectedText()` + `setWorkingDirectory()`;
    resolved lazily via `HostServices::terminal()` (dynamic_cast from ServiceRegistry).
  - **Search** → `plugins/search/` using `WorkbenchPluginBase` + `IViewContributor`;
    `ISearchService` (new SDK) with `setRootPath`, `activateSearch`, `indexWorkspace`,
    `resultActivated` signal; `SymbolIndex` + `WorkspaceIndexer` registered as companion
    services; post-plugin wiring in `loadPlugins()` connects `resultActivated` → navigation.
  - Remaining: project tree, output/log, serial monitor, generic explorers.
- [x] Define the first shared-services cut:
  workspace/profile/settings, diagnostics aggregation, task/build session helpers.
      <!-- SharedServicesBootstrap now owns workspace/profile/settings; TemplateServicesBootstrap split out;
           DiagnosticsServiceImpl no longer stores MainWindow; TaskServiceImpl delegates build.* tasks through IBuildSystem;
           task routing covered by test_taskservice -->
- [x] Move any duplicated panel chrome/tool wiring into shared helpers instead of plugins.
  <!-- WorkbenchPluginBase now provides addToolBarCommands/addMenuCommands;
       debug and testing plugins use the shared command-spec helper instead of repeating toolbar/menu wiring -->

## Phase D — Profile and Template Assembly

- [x] Keep current bundled profiles working.
- [x] Align profile manifests with the new plugin/component ownership model.
      <!-- terminal→org.exorcist.terminal, search→org.exorcist.search IDs normalized;
           terminal+search+git added to requiredPlugins of all 5 dev profiles;
           lightweight gains optional terminal+search; mainwindow.cpp keymapmanager include restored -->
- [x] Separate profile logic from template logic everywhere in docs and code.
  <!-- SharedServicesBootstrap now owns workspace/profile services only;
       TemplateServicesBootstrap owns ProjectTemplateRegistry + BuiltinTemplateProvider;
       MainWindow initializes them independently; bootstrap split covered by tests -->
- [x] Add missing profile metadata for deferred plugin activation and dock defaults.
  <!-- ProfileManifest gains deferredPlugins; ProfileManager force-activates matching deferred plugins;
       Wave 1 and active bundled profiles now carry explicit deferredPlugins + fuller dockDefaults -->

## Phase E — Template Waves

- [x] Wave 1: C++ Native, Qt, Python, Rust, Lightweight
  <!-- Added bundled org.exorcist.python-language and org.exorcist.rust-language plugins
       backed by shared ProcessLanguageServer, plus org.exorcist.qt-tools for Qt workspaces;
       profiles now resolve to real bundled Wave 1 plugin IDs and coverage is verified by tests -->
- [ ] Wave 2: Embedded MCU, Embedded Linux, DevOps, Automation
    <!-- Added scaffold bundled profiles for embedded-linux, devops, and automation.
      Embedded MCU now has real org.exorcist.embedded-tools and org.exorcist.serial-monitor
      plugins, optional live serial support via Qt SerialPort, persisted serial monitor UX,
      workspace-aware command inference for PlatformIO, ESP-IDF, Zephyr, pyOCD,
      OpenOCD, STM32CubeProgrammer script flows, and Makefile flows,
      recursive profile auto-detection for nested embedded markers,
      plus persisted per-workspace flash/monitor/debug command overrides for manual toolchain control.
      Embedded Linux now has concrete recursive Buildroot/Yocto detection coverage and
      remote plugin activation from both the profile and embedded Linux workspace markers.
      Remaining Wave 2 work is broader device/debug ownership plus devops/automation plugin depth. -->
- [ ] Wave 3: Web Backend, Web Frontend, Full Stack, Data Engineering
- [ ] Wave 4: Systems, Game Dev, AI/ML, Blockchain
- [ ] Wave 5: Security Research, Reverse Engineering, external plugin validation

## Immediate Next Steps

1. Extract the next shared service cluster from `MainWindow`.
2. Standardize `debug` and `testing` plugins on the same base ownership model.
3. Audit reusable panels for shared-component promotion.

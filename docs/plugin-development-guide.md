# Plugin Development Guide

> **Write your first Exorcist plugin in 15 minutes.**
>
> A tutorial-style end-to-end guide for third-party plugin authors. For the
> formal contract, see [plugin-model.md](plugin-model.md). For UI rules, see
> [plugin-ui-standards.md](plugin-ui-standards.md). For the system layering,
> see [system-model.md](system-model.md).

---

## Table of contents

1. [Introduction](#1-introduction)
2. [Quickstart (5 min)](#2-quickstart-5-min)
3. [Plugin lifecycle](#3-plugin-lifecycle)
4. [Manifest (`plugin.json`)](#4-manifest-pluginjson)
5. [Common patterns](#5-common-patterns)
   - [Adding a dock panel](#5a-adding-a-dock-panel)
   - [Registering commands](#5b-registering-commands)
   - [Adding an AI tool](#5c-adding-an-ai-tool)
   - [Adding an LSP language plugin](#5d-adding-an-lsp-language-plugin)
6. [Cross-DLL signal/slot — IMPORTANT](#6-cross-dll-signalslot--important)
7. [UI standards](#7-ui-standards)
8. [Testing](#8-testing)
9. [Packaging & distribution](#9-packaging--distribution)
10. [Reference plugins (pointers)](#10-reference-plugins-pointers)
11. [Common pitfalls](#11-common-pitfalls)
12. [Where to ask](#12-where-to-ask)

---

## 1. Introduction

### What plugins can do

Exorcist is a lightweight container — almost every visible feature in the IDE is
implemented by a plugin. As a plugin author you can:

- contribute dock panels (sidebar, bottom, right)
- register commands, menus, toolbars, status-bar items
- declare keybindings and settings pages
- add language support (LSP, syntax, snippets)
- add themes (color schemes)
- expose AI tools to the agent
- expose AI providers (chat completion backends)
- discover and run tasks (build, test, lint)
- contribute completions, formatters, linters

### Plugin philosophy

Three rules drive every design decision in the SDK:

1. **Thin glue layer.** A plugin imports shared components and parameterises
   them — it does not re-implement editors, terminals, output panels, or
   project trees. Most plugins are <500 lines of glue.
2. **`IHostServices` direct access.** After `PluginManager` loads the DLL it
   hands you an `IHostServices*` and steps aside. The plugin talks to the host
   through this single root interface only — never to `MainWindow`.
3. **Write once, reuse everywhere.** If you build a new component (e.g. a
   serial monitor, a profiler view), it goes into a shared component library
   so the next 100 plugins can compose it instead of forking.

If your plugin starts duplicating code that already exists in `src/sdk/`,
`src/plugin/`, or another plugin — stop and lift it into a shared base.

---

## 2. Quickstart (5 min)

### Use the New Plugin wizard

Inside Exorcist:

1. **File → New Plugin…**
2. Fill in the fields:

   | Field | Example |
   |---|---|
   | `id` | `com.acme.hello-world` (reverse-DNS) |
   | `name` | `Hello World` |
   | `description` | `Says hello from a panel and a command.` |
   | `author` | `Acme Inc.` |
   | `type` | *View Contributor* |
   | `folder` | `…/MyPlugins/hello-world` |

3. Press **Create**. The wizard scaffolds a complete, compilable plugin.

### Resulting folder structure

```
hello-world/
├── CMakeLists.txt          # builds the SHARED target → hello-world.dll
├── plugin.json             # manifest (id, version, contributions)
├── README.md
├── helloworldplugin.h      # IPlugin + IViewContributor declaration
└── helloworldplugin.cpp    # initializePlugin, createView, commands
```

The wizard supports five templates:

| Template | What it scaffolds |
|---|---|
| **View Contributor** | `IViewContributor` panel with placeholder widget |
| **Command Plugin** | menu item + toolbar action + status-bar entry |
| **Language Plugin** | `LanguageContribution` + LSP launch wiring |
| **Agent Tool Plugin** | `IAgentToolPlugin` with one example tool |
| **AI Provider Plugin** | `IAgentPlugin` with a stub `IAgentProvider` |

After scaffolding, build the plugin (see [§9 Packaging & distribution](#9-packaging--distribution))
and reload Exorcist — your plugin appears under **Extensions**.

---

## 3. Plugin lifecycle

### `IPlugin` — the base contract

Defined in `src/plugininterface.h`:

```cpp
class IPlugin
{
public:
    virtual ~IPlugin() = default;
    virtual PluginInfo info() const = 0;

    // SDK v1: typed services.
    virtual bool initialize(IHostServices *host) { return true; }

    virtual void suspend() {}                              // optional
    virtual void resume() {}                               // optional
    virtual void onWorkspaceChanged(const QString &root) {} // optional

    virtual void shutdown() = 0;
};

#define EXORCIST_PLUGIN_IID "org.exorcist.IPlugin/1.0"
Q_DECLARE_INTERFACE(IPlugin, EXORCIST_PLUGIN_IID)
```

Every plugin returns a `PluginInfo` from `info()` and is `Q_DECLARE_INTERFACE`'d
with the IID above. Qt's `QPluginLoader` discovers it via `Q_PLUGIN_METADATA`.

### Use the workbench base

You almost never inherit `IPlugin` directly. Use the shared base layer:

- **`WorkbenchPluginBase`** (`src/plugin/workbenchpluginbase.h`) — for general
  plugins that contribute commands, menus, toolbars, or docks.
- **`LanguageWorkbenchPluginBase`** (`src/plugin/languageworkbenchpluginbase.h`)
  — adds LSP lifecycle helpers, standard language menus/toolbars,
  status-bar items, and crash-restart handling.

The base wires `initialize(IHostServices*)` and `shutdown()` for you and gives
you typed accessors: `commands()`, `views()`, `notifications()`, `git()`,
`docks()`, `menus()`, `toolbars()`, `statusBar()`, etc.

### Minimal plugin skeleton

```cpp
// helloworldplugin.h
#pragma once
#include <QObject>
#include "plugin/workbenchpluginbase.h"

class HelloWorldPlugin : public QObject, public WorkbenchPluginBase
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin)

public:
    PluginInfo info() const override;

private:
    bool initializePlugin() override;   // called by base from initialize()
    void shutdownPlugin() override;     // called by base from shutdown()
};
```

```cpp
// helloworldplugin.cpp
#include "helloworldplugin.h"
#include "sdk/icommandservice.h"
#include "sdk/inotificationservice.h"

PluginInfo HelloWorldPlugin::info() const
{
    return {
        QStringLiteral("com.acme.hello-world"),
        QStringLiteral("Hello World"),
        QStringLiteral("1.0.0"),
        QStringLiteral("Says hello"),
        QStringLiteral("Acme"),
        QStringLiteral("1.0"),                     // SDK API version
        { PluginPermission::WorkspaceRead }
    };
}

bool HelloWorldPlugin::initializePlugin()
{
    commands()->registerCommand(
        QStringLiteral("hello.greet"),
        tr("Say Hello"),
        [this]() { showInfo(tr("Hello from the plugin!")); });
    return true;
}

void HelloWorldPlugin::shutdownPlugin()
{
    // Commands, menus, toolbars registered through the base are cleaned up
    // automatically. Only release resources you allocated yourself.
}
```

### Lifecycle phases

| Phase | What happens |
|---|---|
| **load** | `QPluginLoader` loads the DLL and validates SDK version. |
| **initialize** | `initialize(IHostServices*)` → your `initializePlugin()`. Register services, commands, contributions. |
| **activate** | UI materialised (only for views in your manifest, only when first shown). |
| **deactivate** | Plugin asked to release transient state (panels hidden, processes paused). |
| **shutdown** | `shutdownPlugin()` then `shutdown()`. Free everything you own. |

### Registering a service

Plugins expose stable services to other plugins via the host registry:

```cpp
auto *svc = new MyCustomService(this);
registerService(QStringLiteral("myCustomService"), svc);
```

Other plugins resolve it by name:

```cpp
auto *svc = host->service<MyCustomService>(QStringLiteral("myCustomService"));
```

Always use a stable string key. Never share concrete C++ types across DLL
boundaries unless they live in the SDK headers.

---

## 4. Manifest (`plugin.json`)

Every plugin ships a `plugin.json` next to its DLL. The IDE parses it via
`PluginManifest::fromJson`. Validate it locally with
`python tools/validate_plugin_manifests.py`.

### Required fields

```json
{
    "id":          "com.acme.hello-world",
    "name":        "Hello World",
    "version":     "1.0.0",
    "description": "Says hello from a panel and a command.",
    "author":      "Acme",
    "license":     "MIT",
    "layer":       "cpp-sdk",
    "category":    "general"
}
```

### Activation events

Lazy activation keeps startup fast. Choose the narrowest event that still loads
your plugin when needed:

```json
"activationEvents": [
    "onStartupFinished",
    "onCommand:hello.greet",
    "onLanguage:python",
    "onView:hello.panel",
    "workspaceContains:**/Cargo.toml",
    "*"
]
```

| Event | Fires when |
|---|---|
| `*` | Always at startup (use sparingly). |
| `onStartupFinished` | After the IDE finishes booting. |
| `onCommand:<id>` | A command is invoked. |
| `onLanguage:<id>` | A file of that language is opened. |
| `onView:<id>` | One of your views is shown. |
| `workspaceContains:<glob>` | The workspace contains a matching file. |

### Permissions

Every capability your plugin needs must be declared up-front:

```json
"requestedPermissions": [
    "WorkspaceRead",
    "FilesystemRead",
    "TerminalExecute",
    "GitRead"
]
```

Available values are listed in `src/sdk/pluginpermission.h`:
`FilesystemRead/Write`, `TerminalExecute`, `GitRead/Write`, `NetworkAccess`,
`DiagnosticsRead`, `WorkspaceRead/Write`, `AgentToolInvoke`.

### Contributions

A fully-annotated example:

```json
"contributions": {
    "views": [
        {
            "id":             "hello.panel",
            "title":          "Hello Panel",
            "location":       "sidebar-left",   // sidebar-left | sidebar-right | bottom | top
            "defaultVisible": false,
            "priority":       50
        }
    ],
    "commands": [
        {
            "id":         "hello.greet",
            "title":      "Say Hello",
            "category":   "Hello",              // shown as "Hello: Say Hello"
            "icon":       "play",               // ThemeIcons name (optional)
            "keybinding": "Ctrl+Alt+H",
            "when":       "editorFocus"
        }
    ],
    "menus": [
        {
            "commandId": "hello.greet",
            "location":  "MainMenuTools",
            "group":     "1_modification",
            "order":     10
        }
    ],
    "keybindings": [
        { "commandId": "hello.greet", "key": "Ctrl+Alt+H" }
    ],
    "settings": [
        {
            "key":          "hello.greeting",
            "title":        "Greeting text",
            "description":  "Message shown when the command runs.",
            "type":         "String",
            "defaultValue": "Hello from the plugin!"
        }
    ],
    "languages": [],
    "themes":    [],
    "tasks":     [],
    "snippets":  []
}
```

The full schema is `PluginManifest` in `src/plugin/pluginmanifest.h`. Bundled
plugins (e.g. `plugins/cpp-language/plugin.json`, `plugins/build/plugin.json`)
are good real-world references.

---

## 5. Common patterns

### 5a. Adding a dock panel

A view contributor returns a `QWidget` for each `id` declared in the manifest's
`views[]`.

**Step 1 — declare the view in `plugin.json`:**

```json
"views": [
    {
        "id":             "hello.panel",
        "title":          "Hello",
        "location":       "sidebar-left",
        "defaultVisible": false,
        "priority":       50
    }
]
```

**Step 2 — implement `IViewContributor`:**

```cpp
// helloworldplugin.h
#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"

class HelloWorldPlugin
    : public QObject
    , public WorkbenchPluginBase
    , public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin IViewContributor)

public:
    PluginInfo info() const override;
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;

    QWidget *m_panel = nullptr;     // owned by parent set in createView
};
```

```cpp
// helloworldplugin.cpp
QWidget *HelloWorldPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QLatin1String("hello.panel")) {
        if (!m_panel) {
            m_panel = new QLabel(tr("Hello!"), parent);
        } else {
            m_panel->setParent(parent);
        }
        return m_panel;
    }
    return nullptr;
}
```

**Step 3 — show / hide programmatically:**

```cpp
showPanel(QStringLiteral("hello.panel"));    // make visible
hidePanel(QStringLiteral("hello.panel"));
togglePanel(QStringLiteral("hello.panel"));
```

The dock manager parents the widget for you. Never create a top-level
`QWidget` (no parent) — it will appear as a stray window. See
[§11 Common pitfalls](#11-common-pitfalls).

### 5b. Registering commands

```cpp
bool HelloWorldPlugin::initializePlugin()
{
    auto *cmds = commands();   // ICommandService*

    cmds->registerCommand(
        QStringLiteral("hello.greet"),
        tr("Say Hello"),
        [this]() { showInfo(tr("Hello!")); });

    cmds->registerCommand(
        QStringLiteral("hello.shout"),
        tr("Shout Hello"),
        [this]() { showInfo(tr("HELLO!!!")); });

    // Add to the Tools menu
    addMenuCommand(
        IMenuManager::MainMenuTools,
        tr("Say Hello"),
        QStringLiteral("hello.greet"),
        this,
        QKeySequence(QStringLiteral("Ctrl+Alt+H")));

    // Add to a plugin-owned toolbar
    createToolBar(QStringLiteral("hello"), tr("Hello"));
    addToolBarCommand(
        QStringLiteral("hello"),
        tr("Greet"),
        QStringLiteral("hello.greet"),
        this);

    return true;
}
```

The same command id (`hello.greet`) automatically appears in the
**Command Palette**, picks up the manifest keybinding, and fires from any
menu/toolbar item that references it. **Never duplicate handlers.**

For status-bar items use `IStatusBarManager` via `statusBar()` or the language
base helper `installLanguageStatusItem(...)`.

### 5c. Adding an AI tool

Tools are how plugins extend the IDE's chat agent. The agent platform discovers
them automatically from any plugin that implements `IAgentToolPlugin`.

```cpp
// echotool.h
#include "agent/itool.h"

class EchoTool : public ITool
{
public:
    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("echo");
        s.description = QStringLiteral("Echoes its input back as text.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.parallelSafe = true;       // safe to run concurrently with other tools
        s.timeoutMs   = 5000;
        s.inputSchema = QJsonObject{
            { "type", "object" },
            { "properties", QJsonObject{
                { "text", QJsonObject{{ "type", "string" }} }
            }},
            { "required", QJsonArray{ "text" } }
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        ToolExecResult r;
        r.ok          = true;
        r.textContent = args.value("text").toString();
        return r;
    }
};
```

```cpp
// echotoolplugin.h
#include "agent/iagentplugin.h"
#include "plugin/workbenchpluginbase.h"

class EchoToolPlugin
    : public QObject
    , public WorkbenchPluginBase
    , public IAgentToolPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin IAgentToolPlugin)

public:
    PluginInfo info() const override;

    QString toolPluginName() const override { return QStringLiteral("Echo"); }

    std::vector<std::unique_ptr<ITool>> createTools() override
    {
        std::vector<std::unique_ptr<ITool>> v;
        v.emplace_back(std::make_unique<EchoTool>());
        return v;
    }

    QStringList contexts() const override { return {}; }   // always available

private:
    bool initializePlugin() override { return true; }
    void shutdownPlugin() override {}
};
```

Notes:

- Set `parallelSafe = true` only if your `invoke()` does NOT touch
  main-thread `QObject`s. The agent will run such tools concurrently.
- Use `contexts()` (e.g. `{"python"}`, `{"web"}`) to scope a tool to specific
  workspaces — the agent only exposes it when the context is active.
- Tools are owned by the `ToolRegistry`, not by your plugin. They survive
  plugin unload only if they are self-contained (no back-references to plugin
  state).

### 5d. Adding an LSP language plugin

Inherit `LanguageWorkbenchPluginBase` and use `createProcessLanguageServer` +
`registerStandardLspCommands` to get a fully wired language plugin in <100
lines.

**Step 1 — manifest:**

```json
{
    "id":   "com.acme.zig-language",
    "name": "Zig Language",
    "layer":    "cpp-sdk",
    "category": "language",
    "activationEvents": [
        "onLanguage:zig",
        "workspaceContains:build.zig"
    ],
    "requestedPermissions": [
        "WorkspaceRead", "TerminalExecute", "FilesystemRead", "DiagnosticsRead"
    ],
    "contributions": {
        "languages": [
            {
                "id":         "zig",
                "name":       "Zig",
                "extensions": [".zig"],
                "lspCommand": "zls"
            }
        ]
    }
}
```

**Step 2 — plugin code:**

```cpp
// zigplugin.h
#include "plugin/languageworkbenchpluginbase.h"

class LspClient;
class ProcessLanguageServer;

class ZigPlugin : public QObject, public LanguageWorkbenchPluginBase
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin)

public:
    PluginInfo info() const override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;

    std::unique_ptr<ProcessLanguageServer> m_server;
    LspClient *m_client = nullptr;
};
```

```cpp
// zigplugin.cpp
bool ZigPlugin::initializePlugin()
{
    m_server = createProcessLanguageServer(
        QStringLiteral("zls"),
        QStringLiteral("Zig Language Server"));
    startProcessLanguageServer(m_server.get());

    m_client = /* obtain LspClient bound to m_server */;

    registerStandardLspCommands(
        QStringLiteral("zig"),     // command prefix → zig.goToDefinition, etc.
        m_client,
        m_server.get(),
        this);

    installStandardLspMenu(
        QStringLiteral("zig"), tr("Zig"),
        QStringLiteral("zig"), this);

    installStandardLspToolBar(
        QStringLiteral("zig"), tr("Zig"),
        QStringLiteral("zig"), this);

    installLanguageStatusItem(
        QStringLiteral("zig.status"),
        tr("Zig: starting…"));

    setupServerAutoRestart(
        m_server.get(),
        QStringLiteral("zig.status"),
        QStringLiteral("zig"),
        /*maxRestarts*/ 3);

    return true;
}

void ZigPlugin::shutdownPlugin()
{
    if (m_server) stopProcessLanguageServer(m_server.get());
}
```

That's the entire plugin. The base layer wires goto-definition, find-refs,
rename, format, restart, status, crash-restart, and standard menu/toolbar.

For deeper customisation see `plugins/cpp-language/cpplanguageplugin.cpp`,
`plugins/python-language/`, and `plugins/rust-language/`.

---

## 6. Cross-DLL signal/slot — IMPORTANT

> **Read this section. It will save you hours of debugging.**

### Why pointer-to-member-function (PMF) `connect()` silently fails

Qt's modern `connect(sender, &Sender::sig, receiver, &Receiver::slot)` syntax
relies on the receiver's MOC metaobject living in the same translation unit as
the connect call. When SDK headers like `IBuildSystem` are MOC-compiled into
**both** the build plugin DLL and your plugin DLL, the two metaobjects are
distinct — Qt cannot match the slot at runtime, **and the connect fails
without warning**. You see no compile error, no log line, just nothing
happening when the signal fires.

### Rule

For signals declared on **SDK interface classes** that live in `src/sdk/i*.h`
and are exported by another plugin, **always use the string-based connect**:

```cpp
connect(sender, SIGNAL(buildOutput(QString,bool)),
        this,   SLOT(onCfgBuildOutput(QString,bool)));
```

The string form looks up signals/slots through `QMetaObject::indexOfSignal`,
which works across DLL boundaries even when SDK MOC is duplicated.

### Pattern in the C++ plugin

Real-world example from `plugins/cpp-language/cpplanguageplugin.cpp` (~L955):

```cpp
// IBuildSystem comes from the build plugin's DLL (org.exorcist.build).
// PMF connect would silently no-op here. Use SIGNAL/SLOT strings.
connect(m_buildSystem, SIGNAL(configureFinished(bool,QString)),
        this,          SLOT(onCfgConfigureFinished(bool,QString)));
connect(m_buildSystem, SIGNAL(buildOutput(QString,bool)),
        this,          SLOT(onCfgBuildOutput(QString,bool)));
connect(m_buildSystem, SIGNAL(buildFinished(bool,int)),
        this,          SLOT(onCfgBuildFinished(bool,int)));
```

Matching slot declarations in the receiver:

```cpp
private slots:
    void onCfgConfigureFinished(bool success, const QString &message);
    void onCfgBuildOutput(const QString &text, bool isError);
    void onCfgBuildFinished(bool success, int exitCode);
```

### When PMF is fine

- Signals / slots **inside your own DLL** (one MOC database).
- Connections between **two `QObject` instances both owned by the IDE shell**
  (e.g. a `QPushButton` to a slot in the same plugin).

### When string-based is mandatory

- Any signal coming from an `I*Service` resolved via `host->service<T>(...)`.
- Any signal originating in another plugin's DLL.
- Any signal from a class declared in `src/sdk/`.

If unsure, use strings — there's no performance penalty worth worrying about
in this domain.

---

## 7. UI standards

The full rules live in `docs/plugin-ui-standards.md` and the canonical visual
reference is `docs/reference/ui-ux-reference.md` (with a screenshot at
`docs/reference/vs-ui-reference.png`). Highlights for plugin authors:

### Theme

- Dark theme (`#1e1e1e` background) is the primary target. Light theme is
  derived automatically from the same palette.
- Never hard-code colors. Pull them from `ChatThemeTokens` or the global
  palette (`palette().color(...)`).
- Use the centralised `ThemeIcons` SVG icon set — pass an icon **name**
  (e.g. `"play"`, `"folder"`, `"play-circle"`) in `CommandSpec::iconName` or
  `CommandContribution::icon`, not a file path.

### Layout conventions

| Region | Use for |
|---|---|
| **left sidebar** | Explorers (project tree, search, references, outline). |
| **right sidebar** | Tool panels (chat, AI memory, agent insights). |
| **bottom panel** | Streams (terminal, output, problems, run, debug console). |
| **status bar** | Terse one-line state (LSP status, build status, branch). |

### Container rules

- **Never** include `mainwindow.h` from a plugin. Use `IHostServices` only.
- **Never** create a top-level `QWidget` (one with no parent). If you need to
  parent it later, call `widget->hide()` immediately on construction so it
  doesn't flash a stray top-level window — see [§11](#11-common-pitfalls).
- **Plugin-owned UI is strict** — your toolbar, menu, dock are yours to manage.
  The shell does not duplicate or take over plugin actions.
- Standard menu order: `File → Edit → View → Git → Project → Build → Debug →
  Test → Analyze → Run → Terminal → Tools → Extensions → Window → Help`.

---

## 8. Testing

Exorcist uses the Qt `QTest` framework. ~60 test files live in `tests/` and
run under CTest.

### Running tests

```bash
# Build first
cmake --build build-llvm --config Debug

# Run all
ctest --test-dir build-llvm

# Run one
ctest --test-dir build-llvm -R test_workbenchpluginbase -V
```

### Writing a test for your plugin

A typical Qt unit test:

```cpp
// tests/test_helloworldplugin.cpp
#include <QTest>
#include "helloworldplugin.h"

class TestHelloWorldPlugin : public QObject
{
    Q_OBJECT

private slots:
    void info_returnsExpectedId()
    {
        HelloWorldPlugin plugin;
        QCOMPARE(plugin.info().id, QStringLiteral("com.acme.hello-world"));
    }

    void greetCommand_showsNotification()
    {
        // Use a fake IHostServices to assert behaviour.
        // See tests/test_workbenchpluginbase.cpp for FakeCommandService etc.
    }
};

QTEST_MAIN(TestHelloWorldPlugin)
#include "test_helloworldplugin.moc"
```

`tests/test_workbenchpluginbase.cpp` is the canonical reference — it shows how
to build a fake `IHostServices` (with `FakeCommandService`, `FakeMenuManager`,
etc.) and exercise a plugin in isolation.

Add the test to `tests/CMakeLists.txt` (existing entries show the pattern).

---

## 9. Packaging & distribution

### Build target

The wizard generates a CMake target like the one below (mirrors
`plugins/build/CMakeLists.txt`):

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent Svg)

add_library(hello_world_plugin SHARED
    helloworldplugin.h
    helloworldplugin.cpp
)

target_link_libraries(hello_world_plugin PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Svg
    plugin_common
    exorcist
)

target_include_directories(hello_world_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(hello_world_plugin PROPERTIES
    OUTPUT_NAME              "hello-world"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

# Copy plugin.json next to the DLL so the loader can find it.
add_custom_command(TARGET hello_world_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "${CMAKE_BINARY_DIR}/src/plugins/hello-world.json")
```

### Output layout

After `cmake --build build-llvm --config Debug`:

```
build-llvm/src/plugins/
├── hello-world.dll        # the SHARED library
└── hello-world.json       # copied from plugin.json
```

The DLL basename and the `.json` basename **must match**. The IDE pairs them
when discovering plugins at startup.

### Manual install

Copy the two files to either:

- the IDE's bundled plugins dir (`<install>/plugins/`), or
- the user plugins dir (`%APPDATA%/Exorcist/plugins/` on Windows,
  `~/.config/Exorcist/plugins/` on Linux).

### Marketplace install

The IDE ships a `PluginMarketplaceService` (see
`src/plugin/pluginmarketplaceservice.h`):

1. **Extensions → Marketplace** opens the gallery.
2. Pick an entry → it downloads a `.zip` and extracts to the plugins dir.
3. The plugin is loaded immediately; no IDE restart required (unless it adds
   languages, which need an editor reopen).

For self-distribution, host a JSON registry with `MarketplaceEntry` records
and point the service at it via `loadFromFile(...)` or a remote URL.

---

## 10. Reference plugins (pointers)

The bundled plugins under `plugins/` are battle-tested, idiomatic templates.

| Plugin | Demonstrates |
|---|---|
| `plugins/cpp-language` | Full LSP language plugin, multi-service wiring, cross-DLL connects, status item with crash-restart. |
| `plugins/build` | Build system + run/debug. Service adapters (`ILaunchService`, `IOutputService`, `IRunOutputService`), toolbar + menus + commands, `IBuildSystem` service registration. |
| `plugins/debug` | Debugger UI: panels for variables/callstack/breakpoints, `IDebugAdapter` integration, debug-stopped-show-dock pattern. |
| `plugins/git` | Git operations + AI-assisted commit messages. |
| `plugins/copilot`, `plugins/claude` | `IAgentPlugin` / `IAgentProvider` providers; chat completion backends. |
| `plugins/automation` | Task discovery and orchestration. |
| `plugins/devops` | Service discovery, dev-environment integrations. |
| `plugins/python-language`, `plugins/rust-language`, `plugins/typescript-language` | Other LSP plugins, useful for comparing approaches. |
| `plugins/terminal`, `plugins/serial-monitor` | Bottom-panel components with their own widgets. |

When in doubt, find the closest reference plugin and copy its structure.

---

## 11. Common pitfalls

### 1. Top-level QWidget creation flashes a stray window

```cpp
// BAD — m_panel is briefly a top-level window before being parented.
m_panel = new MyPanel();         // null parent
showPanel("foo");                 // dock manager re-parents later
```

```cpp
// GOOD
m_panel = new MyPanel();
m_panel->hide();                  // suppress until reparented
```

### 2. Forgetting `Q_OBJECT` on plugin classes

A plugin without `Q_OBJECT` won't have signals/slots/MOC metadata. The plugin
loader silently skips it, or `Q_INTERFACES` reports `nullptr`. **Every** class
that participates in `Q_PLUGIN_METADATA`, `Q_INTERFACES`, signals, or slots
needs `Q_OBJECT` and a corresponding entry in CMake's automoc.

### 3. PMF connect across DLL boundaries

Already covered in [§6](#6-cross-dll-signalslot--important). Symptom: the
connect compiles fine but the slot is never called. Fix: use
`SIGNAL(...)`/`SLOT(...)` strings.

### 4. Forgetting to register a service in `initializePlugin`

If your plugin exposes a service to others (e.g. `IBuildSystem`,
`ILaunchService`), the registration must happen in `initializePlugin()`. If
you defer it to `onWorkspaceChanged` or a lazy path, peer plugins that
initialised earlier will see `nullptr` from `host->service<...>()` and fall
back to disabled paths.

```cpp
bool MyPlugin::initializePlugin()
{
    m_svc = new MyService(this);
    registerService(QStringLiteral("myService"), m_svc);   // do this first
    /* …other setup… */
    return true;
}
```

### 5. Wrong `activationEvents` format

Common mistakes:

- `"onCommand: hello.greet"` (extra space) — won't match.
- `"onLanguage:Cpp"` — IDs are lowercase: `cpp`, `python`, `typescript`.
- `"workspaceContains:CMakeLists.txt"` — works, but use globs for nested
  layouts: `"workspaceContains:**/CMakeLists.txt"`.
- Forgetting `onStartupFinished` for plugins that contribute always-visible
  toolbars or status items — they won't load until something else triggers
  them.

### 6. Including `mainwindow.h`

You must not. Reach for `IHostServices` or a typed sub-service instead. If you
genuinely need a capability that isn't on the SDK surface, file an issue — the
right answer is to add an `I*Service` interface, not to bypass the boundary.

### 7. Holding raw pointers across plugin reload

`PluginManager` may unload and reload your plugin. Any service you registered
becomes invalid. Other plugins must guard `host->service<T>(...)` with a null
check on every use, not cache the pointer for the session.

### 8. Forgetting to copy `plugin.json` to the output dir

Without the manifest next to the DLL, the IDE won't know your `id`,
contributions, or activation events. The CMake snippet in [§9](#9-packaging--distribution)
shows the required `add_custom_command(... POST_BUILD ...)`.

---

## 12. Where to ask

- **GitHub issues:** open one against the repository for SDK questions, bug
  reports, or to request a new SDK interface.
- **Reference docs:**
  - [plugin-model.md](plugin-model.md) — the plugin contract and ownership rules
  - [plugin-ui-standards.md](plugin-ui-standards.md) — UI rules
  - [system-model.md](system-model.md) — overall layering
  - [`src/sdk/`](../src/sdk) — every stable SDK interface (one header each)
  - [`src/plugin/`](../src/plugin) — base classes and contributor interfaces
  - [`docs/reference/ui-ux-reference.md`](reference/ui-ux-reference.md) — visual
    reference for VS-style dark UI
- **Validate your manifest:** `python tools/validate_plugin_manifests.py`
- **Reload plugins without restarting:** **Extensions → Reload Plugins**

Welcome to the ecosystem — go build something.

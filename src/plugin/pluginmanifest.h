#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

// ── Plugin Manifest ───────────────────────────────────────────────────────────
//
// Declarative description of everything a plugin contributes to the IDE.
// All plugin layers (C++ SDK, C ABI, LuaJIT, DSL) produce a PluginManifest.
// The ContributionRegistry consumes it and wires contributions into the IDE.
//
// Modeled after VS Code's extension manifest with Exorcist-specific additions.

// ── Contribution types ────────────────────────────────────────────────────────

struct CommandContribution
{
    QString id;           // "myPlugin.formatCode"
    QString title;        // "Format Code"
    QString category;     // "My Plugin" (shown as "My Plugin: Format Code")
    QString icon;         // resource path or theme icon name
    QString keybinding;   // "Ctrl+Alt+F" (default binding)
    QString when;         // context condition: "editorLangId == cpp"
    QString tooltip;
};

struct MenuContribution
{
    QString commandId;    // references a CommandContribution::id

    enum Location {
        MainMenuFile,
        MainMenuEdit,
        MainMenuView,
        MainMenuTools,
        MainMenuHelp,
        EditorContextMenu,
        ExplorerContextMenu,
        TerminalContextMenu,
        TabContextMenu,
        StatusBar,
    };
    Location location = MainMenuTools;

    QString group;        // "navigation", "1_modification", "z_commands"
    int     order = 0;    // sort order within group
    QString when;         // visibility condition
};

struct ViewContribution
{
    QString id;           // "myPlugin.outputView"
    QString title;        // "My Output"
    QString icon;         // icon resource

    enum Location {
        SidebarLeft,      // project/search/git area
        SidebarRight,     // AI/memory area
        BottomPanel,      // terminal/output area
        TopPanel,         // above editor (rare)
    };
    Location location = BottomPanel;

    bool    defaultVisible = false;  // shown on first activation
    int     priority = 50;           // sort order in container
};

struct LanguageContribution
{
    QString     id;                // "rust"
    QString     name;              // "Rust"
    QStringList extensions;        // [".rs"]
    QStringList filenames;         // ["Cargo.toml"] (exact name matches)
    QStringList mimeTypes;         // ["text/x-rust"]
    QString     configuration;     // path to language-configuration.json
    QString     firstLine;         // regex for first-line detection

    // LSP server config (optional — auto-launches when language is opened)
    QString     lspCommand;        // "rust-analyzer"
    QStringList lspArgs;
    QJsonObject lspInitOptions;    // passed to LSP initialize
};

struct ThemeContribution
{
    QString id;            // "my-dark-theme"
    QString label;         // "My Dark Theme"
    enum Type { Dark, Light, HighContrast };
    Type    type = Dark;
    QString path;          // path to JSON theme file (relative to plugin dir)
};

struct SettingContribution
{
    QString key;           // "myPlugin.enableFeature"
    QString title;         // "Enable Feature"
    QString description;   // "When enabled, does something useful"
    QString category;      // settings group

    enum Type { Bool, Int, Float, String, Enum, StringArray, FilePath };
    Type    type = Bool;

    QVariant    defaultValue;
    QStringList enumValues;    // for Enum type
    QVariant    minimum;       // for numeric types
    QVariant    maximum;       // for numeric types
};

struct StatusBarContribution
{
    QString id;            // "myPlugin.status"
    QString text;          // "Fmt ✓"
    QString tooltip;       // hover text
    QString command;       // command to run on click

    enum Alignment { Left, Right };
    Alignment alignment = Right;
    int       priority = 0;  // higher = more toward edge
};

struct KeybindingContribution
{
    QString commandId;     // references a CommandContribution::id
    QString key;           // "Ctrl+Shift+P"
    QString mac;           // macOS override: "Cmd+Shift+P"
    QString linux;         // Linux override (optional)
    QString when;          // context condition
};

struct SnippetContribution
{
    QString language;      // "cpp"
    QString path;          // path to snippets JSON file
};

struct TaskContribution
{
    QString id;            // "myPlugin.lint"
    QString label;         // "Run Linter"
    QString type;          // "shell" | "process"
    QString command;       // the command to run
    QStringList args;
    QString group;         // "build" | "test"
    QJsonObject problemMatcher;  // for parsing output
};

struct EditorAction
{
    QString commandId;
    QString title;
    QString when;          // "editorLangId == python"
    QString icon;
};

// ── Activation events ─────────────────────────────────────────────────────────
//
// When should the plugin be activated (loaded/initialized)?
// Lazy activation improves startup time.
//
// Examples:
//   "*"                      — always activate on startup
//   "onCommand:myPlugin.run" — when command is invoked
//   "onLanguage:python"      — when a Python file is opened
//   "onView:myPlugin.panel"  — when the view is shown
//   "workspaceContains:**/Cargo.toml" — when workspace has a Cargo project
//   "onStartupFinished"      — after IDE is fully loaded

// ── Plugin dependency ─────────────────────────────────────────────────────────

struct PluginDependency
{
    QString pluginId;      // "org.exorcist.copilot"
    QString minVersion;    // "1.0.0" (semver)
    bool    optional = false;
};

// ── Plugin layer (which tier this plugin targets) ─────────────────────────────

enum class PluginLayer
{
    CppSdk,    // Layer 1: Native C++ plugin (QPluginLoader)
    CAbi,      // Layer 2: C ABI shared library (dlopen/LoadLibrary)
    LuaJit,    // Layer 3: LuaJIT script
    Dsl,       // Layer 4: DSL / VS Code compat
};

// ── The manifest ──────────────────────────────────────────────────────────────

struct PluginManifest
{
    // ── Identity ──────────────────────────────────────────────────────────
    QString     id;              // "org.exorcist.my-plugin"
    QString     name;            // "My Plugin"
    QString     version;         // "1.2.3"
    QString     description;
    QString     author;
    QString     license;         // "MIT"
    QString     homepage;        // URL
    QString     repository;      // URL
    PluginLayer layer = PluginLayer::CppSdk;

    // ── Activation ────────────────────────────────────────────────────────
    QStringList activationEvents;  // when to load/activate
    QList<PluginDependency> dependencies;

    // ── Contributions ─────────────────────────────────────────────────────
    QList<CommandContribution>     commands;
    QList<MenuContribution>        menus;
    QList<ViewContribution>        views;
    QList<LanguageContribution>    languages;
    QList<ThemeContribution>       themes;
    QList<SettingContribution>     settings;
    QList<StatusBarContribution>   statusBarItems;
    QList<KeybindingContribution>  keybindings;
    QList<SnippetContribution>     snippets;
    QList<TaskContribution>        tasks;
    QList<EditorAction>            editorActions;

    // ── Helpers ───────────────────────────────────────────────────────────
    bool activatesOnStartup() const
    {
        return activationEvents.contains(QStringLiteral("*"));
    }

    bool hasContributions() const
    {
        return !commands.isEmpty() || !menus.isEmpty() || !views.isEmpty()
            || !languages.isEmpty() || !themes.isEmpty() || !settings.isEmpty()
            || !statusBarItems.isEmpty() || !keybindings.isEmpty()
            || !snippets.isEmpty() || !tasks.isEmpty() || !editorActions.isEmpty();
    }

    // ── JSON serialization ────────────────────────────────────────────────
    static PluginManifest fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;
};

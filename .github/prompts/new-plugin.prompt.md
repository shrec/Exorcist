---
description: "Scaffold a new Exorcist plugin with proper structure, metadata, and service registration"
agent: "plugin-dev"
argument-hint: "Plugin name and purpose, e.g. 'GitPlugin — git status, stage, commit operations'"
---
Create a new Exorcist plugin with:

1. **Plugin directory**: `plugins/<name>/`
2. **Interface header**: If the plugin provides a new service type, create the interface
3. **Plugin class**: Implementing `IPlugin` with:
   - `PluginInfo` with reverse-domain id (`org.exorcist.<Name>`)
   - `initialize()` registering services via `ServiceRegistry`
   - `shutdown()` cleaning up resources
   - `Q_PLUGIN_METADATA` and `Q_INTERFACES` macros
4. **CMakeLists.txt**: Build as `SHARED` library, link `Qt6::Widgets`
5. **JSON metadata file**: Plugin metadata for `QPluginLoader`

Reference files:
- [IPlugin interface](src/plugininterface.h)
- [ServiceRegistry](src/serviceregistry.h)
- [PluginManager](src/pluginmanager.h)
- [Plugin API docs](docs/plugin_api.md)

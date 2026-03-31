#pragma once

#include <QObject>
#include "plugin/workbenchpluginbase.h"

/// Registers regex-based syntax highlighting for ~40 languages.
/// Calls SyntaxHighlighter::registerLanguage() for each language family
/// during plugin activation so that HighlighterFactory can fall back to
/// regex highlighting when no tree-sitter grammar is available.
class LangPackPlugin : public QObject, public WorkbenchPluginBase
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin)

public:
    explicit LangPackPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;
};

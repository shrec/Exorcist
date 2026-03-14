find_package(Qt6 REQUIRED COMPONENTS Test Widgets Network Concurrent)

# ── LanguageProfile tests ─────────────────────────────────────────────────────
add_executable(test_languageprofile
    test_languageprofile.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/languageprofile.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/languageprofile.h
)
target_link_libraries(test_languageprofile PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_languageprofile PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME LanguageProfile COMMAND test_languageprofile)

# ── ServiceRegistry tests ─────────────────────────────────────────────────────
add_executable(test_serviceregistry
    test_serviceregistry.cpp
    ${CMAKE_SOURCE_DIR}/src/serviceregistry.cpp
    ${CMAKE_SOURCE_DIR}/src/serviceregistry.h
)
target_link_libraries(test_serviceregistry PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_serviceregistry PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ServiceRegistry COMMAND test_serviceregistry)

# ── PieceTableBuffer tests ────────────────────────────────────────────────────
add_executable(test_piecetable
    test_piecetable.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/piecetablebuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/piecetablebuffer.h
)
target_link_libraries(test_piecetable PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_piecetable PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/editor)
add_test(NAME PieceTable COMMAND test_piecetable)

# ── SseParser tests ───────────────────────────────────────────────────────────
add_executable(test_sseparser
    test_sseparser.cpp
    ${CMAKE_SOURCE_DIR}/plugins/common/sseparser.cpp
    ${CMAKE_SOURCE_DIR}/plugins/common/sseparser.h
)
target_link_libraries(test_sseparser PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_sseparser PRIVATE ${CMAKE_SOURCE_DIR}/plugins/common)
add_test(NAME SseParser COMMAND test_sseparser)

# ── ThemeManager tests ─────────────────────────────────────────────────────────
add_executable(test_thememanager
    test_thememanager.cpp
    ${CMAKE_SOURCE_DIR}/src/thememanager.cpp
    ${CMAKE_SOURCE_DIR}/src/thememanager.h
)
target_link_libraries(test_thememanager PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_thememanager PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ThemeManager COMMAND test_thememanager)

# ── LuaScriptEngine tests ─────────────────────────────────────────────────────
add_executable(test_luascriptengine
    test_luascriptengine.cpp
    ${CMAKE_SOURCE_DIR}/src/sdk/luajit/luascriptengine.cpp
    ${CMAKE_SOURCE_DIR}/src/sdk/luajit/luascriptengine.h
    ${CMAKE_SOURCE_DIR}/src/sdk/luajit/luahostapi.cpp
    ${CMAKE_SOURCE_DIR}/src/sdk/luajit/luahostapi.h
)
target_link_libraries(test_luascriptengine PRIVATE Qt6::Test Qt6::Widgets libluajit)
target_include_directories(test_luascriptengine PRIVATE
    ${CMAKE_SOURCE_DIR}/src/sdk/luajit
    ${CMAKE_SOURCE_DIR}/src/sdk
)
add_test(NAME LuaScriptEngine COMMAND test_luascriptengine)

# ── LspClient tests ───────────────────────────────────────────────────────────
add_executable(test_lspclient
    test_lspclient.cpp
    ${CMAKE_SOURCE_DIR}/src/lsp/lspclient.cpp
    ${CMAKE_SOURCE_DIR}/src/lsp/lspclient.h
    ${CMAKE_SOURCE_DIR}/src/lsp/lsptransport.cpp
    ${CMAKE_SOURCE_DIR}/src/lsp/lsptransport.h
    ${CMAKE_SOURCE_DIR}/src/lsp/lspmessage.h
)
target_link_libraries(test_lspclient PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_lspclient PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME LspClient COMMAND test_lspclient)

# ── SearchWorker tests ─────────────────────────────────────────────────────────
add_executable(test_searchworker
    test_searchworker.cpp
    ${CMAKE_SOURCE_DIR}/src/search/searchworker.cpp
    ${CMAKE_SOURCE_DIR}/src/search/searchworker.h
    ${CMAKE_SOURCE_DIR}/src/search/searchindex.cpp
    ${CMAKE_SOURCE_DIR}/src/search/searchindex.h
    ${CMAKE_SOURCE_DIR}/src/search/searchquery.h
    ${CMAKE_SOURCE_DIR}/src/search/searchmatch.h
)
target_link_libraries(test_searchworker PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_searchworker PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME SearchWorker COMMAND test_searchworker)

# ── OutputPanel tests ──────────────────────────────────────────────────────────
add_executable(test_outputpanel
    test_outputpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/build/outputpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/build/outputpanel.h
)
target_link_libraries(test_outputpanel PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_outputpanel PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME OutputPanel COMMAND test_outputpanel)

# ── ToolRegistry tests ─────────────────────────────────────────────────────────
add_executable(test_toolregistry
    test_toolregistry.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/itool.h
    ${CMAKE_SOURCE_DIR}/src/agent/itool.cpp
)
target_link_libraries(test_toolregistry PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_toolregistry PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ToolRegistry COMMAND test_toolregistry)

# ── ContextBuilder tests ──────────────────────────────────────────────────────
add_executable(test_contextbuilder
    test_contextbuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/contextbuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/contextbuilder.h
    ${CMAKE_SOURCE_DIR}/src/agent/icontextprovider.h
    ${CMAKE_SOURCE_DIR}/src/core/ifilesystem.h
)
target_link_libraries(test_contextbuilder PRIVATE Qt6::Test Qt6::Widgets Qt6::Network Qt6::Concurrent)
target_include_directories(test_contextbuilder PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ContextBuilder COMMAND test_contextbuilder)

# ── MultiCursorEngine tests ────────────────────────────────────────────────
add_executable(test_multicursor
    test_multicursor.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/multicursorengine.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/multicursorengine.h
)
target_link_libraries(test_multicursor PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_multicursor PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME MultiCursor COMMAND test_multicursor)

# ── CodeFolding tests ─────────────────────────────────────────────────────────
add_executable(test_codefolding
    test_codefolding.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/codefoldingengine.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/codefoldingengine.h
)
target_link_libraries(test_codefolding PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_codefolding PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME CodeFolding COMMAND test_codefolding)

# ── GitService tests ──────────────────────────────────────────────────────────
add_executable(test_gitservice
    test_gitservice.cpp
    ${CMAKE_SOURCE_DIR}/src/git/gitservice.cpp
    ${CMAKE_SOURCE_DIR}/src/git/gitservice.h
    ${CMAKE_SOURCE_DIR}/src/process/bridgeclient.h
    ${CMAKE_SOURCE_DIR}/src/process/bridgeclient.cpp
    ${CMAKE_SOURCE_DIR}/src/process/ipcprotocol.cpp
)
target_link_libraries(test_gitservice PRIVATE Qt6::Test Qt6::Widgets Qt6::Concurrent Qt6::Network)
target_include_directories(test_gitservice PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME GitService COMMAND test_gitservice)

# ── ProjectManager tests ──────────────────────────────────────────────────────
add_executable(test_projectmanager
    test_projectmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/project/projectmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/project/projectmanager.h
    ${CMAKE_SOURCE_DIR}/src/project/projecttypes.h
)
target_link_libraries(test_projectmanager PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_projectmanager PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ProjectManager COMMAND test_projectmanager)

# ── TerminalScreen tests ──────────────────────────────────────────────────────
add_executable(test_terminalscreen
    test_terminalscreen.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalscreen.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalscreen.h
)
target_link_libraries(test_terminalscreen PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_terminalscreen PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME TerminalScreen COMMAND test_terminalscreen)

# ── TreeSitterHighlighter tests ───────────────────────────────────────────────
add_executable(test_treesitter_highlighter
    test_treesitter_highlighter.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/treesitterhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/treesitterhighlighter.h
)
target_link_libraries(test_treesitter_highlighter PRIVATE Qt6::Test Qt6::Widgets tree-sitter-core
    ts-grammar-c ts-grammar-cpp ts-grammar-python ts-grammar-javascript ts-grammar-json)
target_compile_definitions(test_treesitter_highlighter PRIVATE EXORCIST_HAS_TREESITTER)
target_include_directories(test_treesitter_highlighter PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME TreeSitterHighlighter COMMAND test_treesitter_highlighter)

# ── SyntaxHighlighter tests ───────────────────────────────────────────────────
file(GLOB LANG_BUILDER_SRCS ${CMAKE_SOURCE_DIR}/src/editor/languages/*.cpp)
add_executable(test_syntaxhighlighter
    test_syntaxhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/syntaxhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/syntaxhighlighter.h
    ${LANG_BUILDER_SRCS}
)
target_link_libraries(test_syntaxhighlighter PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_syntaxhighlighter PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME SyntaxHighlighter COMMAND test_syntaxhighlighter)

# ── HighlighterFactory tests ──────────────────────────────────────────────────
add_executable(test_highlighterfactory
    test_highlighterfactory.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/highlighterfactory.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/highlighterfactory.h
    ${CMAKE_SOURCE_DIR}/src/editor/treesitterhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/treesitterhighlighter.h
    ${CMAKE_SOURCE_DIR}/src/editor/syntaxhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/syntaxhighlighter.h
    ${LANG_BUILDER_SRCS}
)
target_link_libraries(test_highlighterfactory PRIVATE Qt6::Test Qt6::Widgets tree-sitter-core
    ts-grammar-c ts-grammar-cpp ts-grammar-python ts-grammar-javascript
    ts-grammar-typescript ts-grammar-rust ts-grammar-json
    ts-grammar-go ts-grammar-yaml ts-grammar-toml)
target_compile_definitions(test_highlighterfactory PRIVATE EXORCIST_HAS_TREESITTER)
target_include_directories(test_highlighterfactory PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME HighlighterFactory COMMAND test_highlighterfactory)

# ── KeymapManager tests ──────────────────────────────────────────────────────
add_executable(test_keymapmanager
    test_keymapmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/keymapmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/keymapmanager.h
)
target_link_libraries(test_keymapmanager PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_keymapmanager PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME KeymapManager COMMAND test_keymapmanager)

# ── CommandPalette tests ─────────────────────────────────────────────────────
add_executable(test_commandpalette
    test_commandpalette.cpp
    ${CMAKE_SOURCE_DIR}/src/commandpalette.cpp
    ${CMAKE_SOURCE_DIR}/src/commandpalette.h
)
target_link_libraries(test_commandpalette PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_commandpalette PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME CommandPalette COMMAND test_commandpalette)

# ── TestDiscoveryService tests ────────────────────────────────────────────────
add_executable(test_testdiscovery
    test_testdiscovery.cpp
    ${CMAKE_SOURCE_DIR}/src/testing/testdiscoveryservice.cpp
    ${CMAKE_SOURCE_DIR}/src/testing/testdiscoveryservice.h
)
target_link_libraries(test_testdiscovery PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_testdiscovery PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME TestDiscovery COMMAND test_testdiscovery)

# ── ProblemsPanel tests ───────────────────────────────────────────────────────
add_executable(test_problemspanel
    test_problemspanel.cpp
    ${CMAKE_SOURCE_DIR}/src/problems/problemspanel.cpp
    ${CMAKE_SOURCE_DIR}/src/problems/problemspanel.h
    ${CMAKE_SOURCE_DIR}/src/build/outputpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/build/outputpanel.h
)
target_link_libraries(test_problemspanel PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_problemspanel PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ProblemsPanel COMMAND test_problemspanel)

# ── WorkspaceSettings ─────────────────────────────────────────────────────────
add_executable(test_workspacesettings
    test_workspacesettings.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/workspacesettings.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/workspacesettings.h
)
target_link_libraries(test_workspacesettings PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_workspacesettings PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME WorkspaceSettings COMMAND test_workspacesettings)

# ── DiffMerge ─────────────────────────────────────────────────────────────────
add_executable(test_diffmerge
    test_diffmerge.cpp
    ${CMAKE_SOURCE_DIR}/src/git/gitservice.cpp
    ${CMAKE_SOURCE_DIR}/src/git/gitservice.h
    ${CMAKE_SOURCE_DIR}/src/git/diffexplorerpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/git/diffexplorerpanel.h
    ${CMAKE_SOURCE_DIR}/src/git/mergeeditor.cpp
    ${CMAKE_SOURCE_DIR}/src/git/mergeeditor.h
    ${CMAKE_SOURCE_DIR}/src/process/bridgeclient.h
    ${CMAKE_SOURCE_DIR}/src/process/bridgeclient.cpp
    ${CMAKE_SOURCE_DIR}/src/process/ipcprotocol.cpp
)
target_link_libraries(test_diffmerge PRIVATE Qt6::Test Qt6::Widgets Qt6::Concurrent Qt6::Network)
target_include_directories(test_diffmerge PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME DiffMerge COMMAND test_diffmerge)

# ── VimHandler ────────────────────────────────────────────────────────────────
add_executable(test_vimhandler
    test_vimhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/vimhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/vimhandler.h
)
target_link_libraries(test_vimhandler PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_vimhandler PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME VimHandler COMMAND test_vimhandler)

# ── RemoteLsp ─────────────────────────────────────────────────────────────────
add_executable(test_remotelsp
    test_remotelsp.cpp
    ${CMAKE_SOURCE_DIR}/src/lsp/socketlsptransport.cpp
    ${CMAKE_SOURCE_DIR}/src/lsp/socketlsptransport.h
    ${CMAKE_SOURCE_DIR}/src/lsp/lsptransport.cpp
    ${CMAKE_SOURCE_DIR}/src/lsp/lsptransport.h
    ${CMAKE_SOURCE_DIR}/src/lsp/lspmessage.h
    ${CMAKE_SOURCE_DIR}/plugins/remote/sshsession.cpp
    ${CMAKE_SOURCE_DIR}/plugins/remote/sshsession.h
    ${CMAKE_SOURCE_DIR}/plugins/remote/sshprofile.h
)
target_link_libraries(test_remotelsp PRIVATE Qt6::Test Qt6::Widgets Qt6::Network)
target_include_directories(test_remotelsp PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/plugins)
add_test(NAME RemoteLsp COMMAND test_remotelsp)

# ── PluginMarketplace ─────────────────────────────────────────────────────────
add_executable(test_pluginmarketplace
    test_pluginmarketplace.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmarketplaceservice.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmarketplaceservice.h
)
target_link_libraries(test_pluginmarketplace PRIVATE Qt6::Test Qt6::Widgets Qt6::Network)
target_include_directories(test_pluginmarketplace PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME PluginMarketplace COMMAND test_pluginmarketplace)

# ── GitignoreFilter tests ─────────────────────────────────────────────────────
add_executable(test_gitignorefilter
    test_gitignorefilter.cpp
)
target_link_libraries(test_gitignorefilter PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_gitignorefilter PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME GitignoreFilter COMMAND test_gitignorefilter)

# ── ContextPruner tests ──────────────────────────────────────────────────────
add_executable(test_contextpruner
    test_contextpruner.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/contextpruner.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/contextpruner.h
)
target_link_libraries(test_contextpruner PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_contextpruner PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ContextPruner COMMAND test_contextpruner)

# ── AutoCompactor tests ──────────────────────────────────────────────────────
add_executable(test_autocompactor
    test_autocompactor.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/autocompactor.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/autocompactor.h
)
target_link_libraries(test_autocompactor PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_autocompactor PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME AutoCompactor COMMAND test_autocompactor)

# ── ExoBridge IPC + core tests ─────────────────────────────────────────────────
add_executable(test_exobridge
    test_exobridge.cpp
    ${CMAKE_SOURCE_DIR}/src/process/exobridgecore.h
    ${CMAKE_SOURCE_DIR}/src/process/exobridgecore.cpp
    ${CMAKE_SOURCE_DIR}/src/process/ipcprotocol.h
    ${CMAKE_SOURCE_DIR}/src/process/ipcprotocol.cpp
)
target_link_libraries(test_exobridge PRIVATE Qt6::Test Qt6::Network)
target_include_directories(test_exobridge PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ExoBridge COMMAND test_exobridge)

# ── BridgeClient crash detection + connectivity tests ──────────────────────────
add_executable(test_bridgeclient
    test_bridgeclient.cpp
    ${CMAKE_SOURCE_DIR}/src/process/bridgeclient.h
    ${CMAKE_SOURCE_DIR}/src/process/bridgeclient.cpp
    ${CMAKE_SOURCE_DIR}/src/process/exobridgecore.h
    ${CMAKE_SOURCE_DIR}/src/process/exobridgecore.cpp
    ${CMAKE_SOURCE_DIR}/src/process/ipcprotocol.h
    ${CMAKE_SOURCE_DIR}/src/process/ipcprotocol.cpp
)
target_link_libraries(test_bridgeclient PRIVATE Qt6::Test Qt6::Network)
target_include_directories(test_bridgeclient PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME BridgeClient COMMAND test_bridgeclient)

# ── ExoBridge services tests ───────────────────────────────────────────────────
add_executable(test_bridgeservices
    test_bridgeservices.cpp
    ${CMAKE_SOURCE_DIR}/server/gitwatchservice.h
    ${CMAKE_SOURCE_DIR}/server/gitwatchservice.cpp
    ${CMAKE_SOURCE_DIR}/server/authtokenservice.h
    ${CMAKE_SOURCE_DIR}/server/authtokenservice.cpp
)
target_link_libraries(test_bridgeservices PRIVATE Qt6::Test Qt6::Widgets Qt6::Network)
if(WIN32)
    target_link_libraries(test_bridgeservices PRIVATE Advapi32)
endif()
target_include_directories(test_bridgeservices PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/server
)
add_test(NAME BridgeServices COMMAND test_bridgeservices)

# ── Core Interfaces tests (IFileWatcher, IEnvironment) ─────────────────────────
add_executable(test_coreinterfaces
    test_coreinterfaces.cpp
    ${CMAKE_SOURCE_DIR}/src/core/qtfilewatcher.cpp
    ${CMAKE_SOURCE_DIR}/src/core/qtfilewatcher.h
    ${CMAKE_SOURCE_DIR}/src/core/qtenvironment.cpp
    ${CMAKE_SOURCE_DIR}/src/core/qtenvironment.h
)
target_link_libraries(test_coreinterfaces PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_coreinterfaces PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME CoreInterfaces COMMAND test_coreinterfaces)

# ── CrashHandler tests ────────────────────────────────────────────────────────
add_executable(test_crashhandler
    test_crashhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/crashhandler.cpp
    ${CMAKE_SOURCE_DIR}/src/crashhandler.h
)
target_link_libraries(test_crashhandler PRIVATE Qt6::Test Qt6::Widgets)
if(WIN32)
    target_link_libraries(test_crashhandler PRIVATE dbghelp)
endif()
target_include_directories(test_crashhandler PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME CrashHandler COMMAND test_crashhandler)

# ── IPC Protocol tests ────────────────────────────────────────────────────────
add_executable(test_ipcprotocol
    test_ipcprotocol.cpp
    ${CMAKE_SOURCE_DIR}/src/process/ipcprotocol.cpp
    ${CMAKE_SOURCE_DIR}/src/process/ipcprotocol.h
)
target_link_libraries(test_ipcprotocol PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_ipcprotocol PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME IpcProtocol COMMAND test_ipcprotocol)

# ── FeatureFlags tests ────────────────────────────────────────────────────────
add_executable(test_featureflags
    test_featureflags.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/featureflags.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/featureflags.h
)
target_link_libraries(test_featureflags PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_featureflags PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME FeatureFlags COMMAND test_featureflags)

# ── PluginManifest tests ──────────────────────────────────────────────────────
add_executable(test_pluginmanifest
    test_pluginmanifest.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmanifest.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmanifest.h
)
target_link_libraries(test_pluginmanifest PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_pluginmanifest PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME PluginManifest COMMAND test_pluginmanifest)

# ── PluginSuspend tests ───────────────────────────────────────────────────────
add_executable(test_pluginsuspend
    test_pluginsuspend.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmanifest.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmanifest.h
)
target_link_libraries(test_pluginsuspend PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_pluginsuspend PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME PluginSuspend COMMAND test_pluginsuspend)

# ── TreeSitterQueryTool tests ─────────────────────────────────────────────────
add_executable(test_treesitterquerytool
    test_treesitterquerytool.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/tools/treesitterquerytool.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/tools/treesitterquerytool.h
)
target_link_libraries(test_treesitterquerytool PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_treesitterquerytool PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME TreeSitterQueryTool COMMAND test_treesitterquerytool)

# ── DiagnosticsNotifier tests ─────────────────────────────────────────────────
add_executable(test_diagnosticsnotifier
    test_diagnosticsnotifier.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/diagnosticsnotifier.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/diagnosticsnotifier.h
)
target_link_libraries(test_diagnosticsnotifier PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_diagnosticsnotifier PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME DiagnosticsNotifier COMMAND test_diagnosticsnotifier)

# ── BuildTestServices tests ───────────────────────────────────────────────────
add_executable(test_buildtestservices
    test_buildtestservices.cpp
    ${CMAKE_SOURCE_DIR}/src/build/buildsystemservice.cpp
    ${CMAKE_SOURCE_DIR}/src/build/buildsystemservice.h
    ${CMAKE_SOURCE_DIR}/src/build/cmakeintegration.cpp
    ${CMAKE_SOURCE_DIR}/src/build/cmakeintegration.h
    ${CMAKE_SOURCE_DIR}/src/build/toolchainmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/build/toolchainmanager.h
    ${CMAKE_SOURCE_DIR}/src/testing/testrunnerservice.cpp
    ${CMAKE_SOURCE_DIR}/src/testing/testrunnerservice.h
    ${CMAKE_SOURCE_DIR}/src/testing/testdiscoveryservice.cpp
    ${CMAKE_SOURCE_DIR}/src/testing/testdiscoveryservice.h
    ${CMAKE_SOURCE_DIR}/src/sdk/ibuildsystem.h
    ${CMAKE_SOURCE_DIR}/src/sdk/itestrunner.h
)
target_link_libraries(test_buildtestservices PRIVATE Qt6::Test Qt6::Widgets Qt6::Concurrent)
target_include_directories(test_buildtestservices PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME BuildTestServices COMMAND test_buildtestservices)

# ── ContributionRegistry tests ────────────────────────────────────────────────
add_executable(test_contributionregistry
    test_contributionregistry.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmanifest.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmanifest.h
)
target_link_libraries(test_contributionregistry PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_contributionregistry PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ContributionRegistry COMMAND test_contributionregistry)

# ── AgentUIBus tests ──────────────────────────────────────────────────────────
add_executable(test_agentuibus
    test_agentuibus.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/ui/agentuibus.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/ui/agentuibus.h
    ${CMAKE_SOURCE_DIR}/src/agent/ui/agentuievent.h
    ${CMAKE_SOURCE_DIR}/src/agent/ui/iagentuirenderer.h
)
target_link_libraries(test_agentuibus PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_agentuibus PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME AgentUIBus COMMAND test_agentuibus)

# ── HTTP/2 Hardening tests ────────────────────────────────────────────────────
add_executable(test_http2hardening
    test_http2hardening.cpp
)
target_link_libraries(test_http2hardening PRIVATE Qt6::Test Qt6::Widgets Qt6::Network)
target_include_directories(test_http2hardening PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME Http2Hardening COMMAND test_http2hardening)

# ── MCP Workspace Trust tests ─────────────────────────────────────────────────
add_executable(test_mcptrust
    test_mcptrust.cpp
    ${CMAKE_SOURCE_DIR}/src/mcp/mcpservermanager.cpp
    ${CMAKE_SOURCE_DIR}/src/mcp/mcpservermanager.h
)
target_link_libraries(test_mcptrust PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_mcptrust PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME McpTrust COMMAND test_mcptrust)

# ── Marketplace Safety tests ──────────────────────────────────────────────────
add_executable(test_marketplacesafety
    test_marketplacesafety.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmarketplaceservice.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmarketplaceservice.h
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmanifest.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pluginmanifest.h
)
target_link_libraries(test_marketplacesafety PRIVATE Qt6::Test Qt6::Widgets Qt6::Network)
target_include_directories(test_marketplacesafety PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME MarketplaceSafety COMMAND test_marketplacesafety)

# ── ProjectBrainDb tests ──────────────────────────────────────────────────────
add_executable(test_projectbraindb
    test_projectbraindb.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/tools/projectbraindb.cpp
    ${CMAKE_SOURCE_DIR}/src/agent/tools/projectbraindb.h
    ${CMAKE_SOURCE_DIR}/src/agent/itool.cpp
)
target_link_libraries(test_projectbraindb PRIVATE Qt6::Test Qt6::Widgets Qt6::Sql)
target_include_directories(test_projectbraindb PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ProjectBrainDb COMMAND test_projectbraindb)

# Deploy Qt DLLs so tests can run (Windows)
if(WIN32 AND EXORCIST_DEPLOY_QT)
    find_program(WINDEPLOYQT windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET test_serviceregistry POST_BUILD
            COMMAND "${WINDEPLOYQT}" --compiler-runtime "$<TARGET_FILE:test_serviceregistry>"
            COMMENT "Deploying Qt for tests"
        )
    endif()
endif()

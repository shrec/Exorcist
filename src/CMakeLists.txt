add_executable(exorcist WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    editor/editorview.cpp
    editor/editorview.h
    editor/largefileloader.cpp
    editor/largefileloader.h
    editor/syntaxhighlighter.cpp
    editor/syntaxhighlighter.h
    editor/textbuffer.h
    editor/piecetablebuffer.cpp
    editor/piecetablebuffer.h
    search/searchquery.h
    search/searchmatch.h
    search/searchindex.cpp
    search/searchindex.h
    search/searchpanel.cpp
    search/searchpanel.h
    search/searchworker.cpp
    search/searchworker.h
    search/searchservice.cpp
    search/searchservice.h
    lsp/lspmessage.h
    lsp/lsptransport.cpp
    lsp/lsptransport.h
    lsp/processlsptransport.cpp
    lsp/processlsptransport.h
    lsp/clangdmanager.cpp
    lsp/clangdmanager.h
    lsp/lspclient.cpp
    lsp/lspclient.h
    lsp/completionpopup.cpp
    lsp/completionpopup.h
    lsp/lspeditorbridge.cpp
    lsp/lspeditorbridge.h
    lsp/referencespanel.cpp
    lsp/referencespanel.h
    editor/symboloutlinepanel.cpp
    editor/symboloutlinepanel.h
    terminal/terminalscreen.cpp
    terminal/terminalscreen.h
    terminal/ptybackend.cpp
    terminal/ptybackend.h
    terminal/terminalview.cpp
    terminal/terminalview.h
    terminal/terminalwidget.cpp
    terminal/terminalwidget.h
    agent/iagentplugin.h
    agent/agentorchestrator.cpp
    agent/agentorchestrator.h
    agent/agentchatpanel.cpp
    agent/agentchatpanel.h
    commandpalette.cpp
    commandpalette.h
    logger.cpp
    logger.h
    resources.qrc
    exorcist.rc
    aiinterface.h
    plugininterface.h
    pluginmanager.cpp
    pluginmanager.h
    serviceregistry.cpp
    serviceregistry.h
    core/ifilesystem.h
    core/iprocess.h
    core/iterminal.h
    core/inetwork.h
    core/qtfilesystem.cpp
    core/qtfilesystem.h
    core/qtprocess.cpp
    core/qtprocess.h
    core/qtterminal.cpp
    core/qtterminal.h
    core/nullnetwork.cpp
    core/nullnetwork.h
)

target_link_libraries(exorcist PRIVATE Qt6::Widgets)

if(EXORCIST_USE_LLVM)
    target_include_directories(exorcist PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(exorcist PRIVATE ${LLVM_DEFINITIONS})
endif()

if(WIN32 AND EXORCIST_DEPLOY_QT)
    get_target_property(_qt_core_location Qt6::Core LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)
    add_custom_command(TARGET exorcist POST_BUILD
        COMMAND "${_qt_bin_dir}/windeployqt.exe"
            --compiler-runtime
            "$<TARGET_FILE:exorcist>"
        VERBATIM
    )
endif()

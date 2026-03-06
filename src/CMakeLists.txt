add_executable(exorcist WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    editor/editorview.cpp
    editor/editorview.h
    editor/largefileloader.cpp
    editor/largefileloader.h
    editor/textbuffer.h
    resources.qrc
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

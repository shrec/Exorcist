# ── Qt feature modules required by qt-tools plugin ────────────────────────
# These were previously linked into the exorcist binary; the manifesto says
# Qt-specific features belong in this plugin DLL, so the modules move here.
find_package(Qt6 REQUIRED COMPONENTS UiTools Quick Qml QuickWidgets)
find_package(Qt6 QUIET COMPONENTS Help)

add_library(qt_tools_plugin SHARED
    qttoolsplugin.h
    qttoolsplugin.cpp

    # In-IDE Qt help dock (F1 keyword lookup via QHelpEngineCore)
    qthelpdock.h
    qthelpdock.cpp

    # Qt kit manager dialog (auto-detects Qt versions + compilers)
    kitmanagerdialog.h
    kitmanagerdialog.cpp

    # New Qt Class wizard (Q_OBJECT scaffolder + optional .ui)
    newqtclasswizard.h
    newqtclasswizard.cpp

    # Custom Qt Designer replacement (form editor)
    formeditor/uiformeditor.h
    formeditor/uiformeditor.cpp
    formeditor/widgetpalette.h
    formeditor/widgetpalette.cpp
    formeditor/formcanvas.h
    formeditor/formcanvas.cpp
    formeditor/propertyinspector.h
    formeditor/propertyinspector.cpp
    formeditor/uixmlio.h
    formeditor/uixmlio.cpp
    formeditor/inlinelabeleditor.h
    formeditor/inlinelabeleditor.cpp
    formeditor/signalsloteditor.h
    formeditor/signalsloteditor.cpp
    formeditor/promotedialog.h
    formeditor/promotedialog.cpp

    # QML editor + live preview pane
    qmleditor/qmleditorwidget.h
    qmleditor/qmleditorwidget.cpp
    qmleditor/qmlpreviewpane.h
    qmleditor/qmlpreviewpane.cpp

    # QSS editor + live preview pane
    qsseditor/qsseditorwidget.h
    qsseditor/qsseditorwidget.cpp
    qsseditor/qsspreviewpane.h
    qsseditor/qsspreviewpane.cpp

    # Qt Linguist (.ts) editor
    linguist/linguisteditor.h
    linguist/linguisteditor.cpp
    linguist/tsxmlio.h
    linguist/tsxmlio.cpp
)

target_link_libraries(qt_tools_plugin PRIVATE
    Qt6::Widgets
    Qt6::UiTools
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickWidgets
    plugin_common
    exorcist
)

if(Qt6Help_FOUND)
    target_link_libraries(qt_tools_plugin PRIVATE Qt6::Help)
    target_compile_definitions(qt_tools_plugin PRIVATE EXORCIST_HAS_QT_HELP=1)
else()
    message(STATUS "Qt6::Help not found — qt-tools plugin's QtHelpDock will run in fallback mode.")
endif()

target_include_directories(qt_tools_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(qt_tools_plugin PROPERTIES
    OUTPUT_NAME "qt-tools"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_custom_command(TARGET qt_tools_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:qt_tools_plugin>/qt-tools.json"
)

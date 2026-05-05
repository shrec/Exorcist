add_library(debug_plugin SHARED
    debugplugin.h
    debugplugin.cpp
    gdbmiadapter.h
    gdbmiadapter.cpp
    memoryviewpanel.h
    memoryviewpanel.cpp
    disassemblypanel.h
    disassemblypanel.cpp
    debugpanel.h
    debugpanel.cpp
    watchtreemodel.h
    watchtreemodel.cpp
    quickwatchdialog.h
    quickwatchdialog.cpp
    watchpointdialog.h
    watchpointdialog.cpp
)

target_link_libraries(debug_plugin PRIVATE Qt6::Widgets Qt6::Svg plugin_common exorcist)

target_include_directories(debug_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(debug_plugin PROPERTIES
    OUTPUT_NAME  "debug"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

# Copy manifest next to the DLL so PluginManager can find it
add_custom_command(TARGET debug_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:debug_plugin>/debug.json"
)

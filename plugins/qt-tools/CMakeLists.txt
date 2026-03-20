add_library(qt_tools_plugin SHARED
    qttoolsplugin.h
    qttoolsplugin.cpp
)

target_link_libraries(qt_tools_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

target_include_directories(qt_tools_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
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
add_library(embedded_tools_plugin SHARED
    embeddedcommandresolver.h
    embeddedcommandresolver.cpp
    embeddedtoolsplugin.h
    embeddedtoolsplugin.cpp
    embeddedtoolspanel.h
    embeddedtoolspanel.cpp
)

target_link_libraries(embedded_tools_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

target_include_directories(embedded_tools_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
)

set_target_properties(embedded_tools_plugin PROPERTIES
    OUTPUT_NAME "embedded-tools"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_custom_command(TARGET embedded_tools_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:embedded_tools_plugin>/embedded-tools.json"
)
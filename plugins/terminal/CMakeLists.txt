add_library(terminal_plugin SHARED
    terminalplugin.h
    terminalplugin.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalpanel.h
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalwidget.h
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalview.h
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalview.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalscreen.h
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalscreen.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/ptybackend.h
    ${CMAKE_SOURCE_DIR}/src/terminal/ptybackend.cpp
)

target_link_libraries(terminal_plugin PRIVATE Qt6::Widgets plugin_common exorcist)

target_include_directories(terminal_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
)

set_target_properties(terminal_plugin PROPERTIES
    OUTPUT_NAME  "terminal"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

# Copy manifest next to the DLL so PluginManager can find it
add_custom_command(TARGET terminal_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:terminal_plugin>/terminal.json"
)

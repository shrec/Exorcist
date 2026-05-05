add_library(git_plugin SHARED
    gitplugin.h
    gitplugin.cpp
    inlineblameoverlay.h
    inlineblameoverlay.cpp
    gitpanel.h
    gitpanel.cpp
    diffexplorerpanel.h
    diffexplorerpanel.cpp
    mergeeditor.h
    mergeeditor.cpp
)

target_link_libraries(git_plugin PRIVATE Qt6::Widgets Qt6::Concurrent plugin_common exorcist)

target_include_directories(git_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(git_plugin PROPERTIES
    OUTPUT_NAME  "git"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

# Copy manifest next to the DLL so PluginManager can find it
add_custom_command(TARGET git_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:git_plugin>/git.json"
)

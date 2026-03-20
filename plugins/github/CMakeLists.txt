add_library(github_plugin SHARED
    githubplugin.h
    githubplugin.cpp
    ghservice.h
    ghservice.cpp
    ghpanel.h
    ghpanel.cpp
)

target_link_libraries(github_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

target_include_directories(github_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(github_plugin PROPERTIES
    OUTPUT_NAME "github"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

# Copy plugin.json next to the plugin binary for manifest loading
add_custom_command(TARGET github_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "${CMAKE_BINARY_DIR}/src/plugins/github.json"
)

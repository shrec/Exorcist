add_library(testing_plugin SHARED
    testingplugin.h
    testingplugin.cpp
    testdiscoveryservice.h
    testdiscoveryservice.cpp
    testexplorerpanel.h
    testexplorerpanel.cpp
    testrunnerservice.h
    testrunnerservice.cpp
)

target_link_libraries(testing_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

target_include_directories(testing_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(testing_plugin PROPERTIES
    OUTPUT_NAME "testing"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

# Copy plugin.json next to the plugin binary for manifest loading
add_custom_command(TARGET testing_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "${CMAKE_BINARY_DIR}/src/plugins/testing.json"
)

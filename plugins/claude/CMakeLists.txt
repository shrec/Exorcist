add_library(claude_plugin SHARED
    claudeplugin.h
    claudeplugin.cpp
    claudeprovider.h
    claudeprovider.cpp
    ${CMAKE_SOURCE_DIR}/src/aiinterface.h
)

target_link_libraries(claude_plugin PRIVATE
    Qt6::Widgets
    Qt6::Network
    plugin_common
)

target_include_directories(claude_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(claude_plugin PROPERTIES
    OUTPUT_NAME "claude"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

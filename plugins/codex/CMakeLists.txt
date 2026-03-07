add_library(codex_plugin SHARED
    codexplugin.h
    codexplugin.cpp
    codexprovider.h
    codexprovider.cpp
    ${CMAKE_SOURCE_DIR}/src/aiinterface.h
)

target_link_libraries(codex_plugin PRIVATE
    Qt6::Widgets
    Qt6::Network
    plugin_common
)

target_include_directories(codex_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(codex_plugin PROPERTIES
    OUTPUT_NAME "codex"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

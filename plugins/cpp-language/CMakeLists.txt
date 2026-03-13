add_library(cpp_language_plugin SHARED
    cpplanguageplugin.h
    cpplanguageplugin.cpp
    clangdmanager.h
    clangdmanager.cpp
)

target_link_libraries(cpp_language_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

target_include_directories(cpp_language_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
)

set_target_properties(cpp_language_plugin PROPERTIES
    OUTPUT_NAME "cpp-language"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_custom_command(TARGET cpp_language_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "${CMAKE_BINARY_DIR}/src/plugins/plugin.json"
    COMMENT "Copying cpp-language plugin.json"
)

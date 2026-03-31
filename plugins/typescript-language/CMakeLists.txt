add_library(typescript_language_plugin SHARED
    typescriptlanguageplugin.h
    typescriptlanguageplugin.cpp
)

target_link_libraries(typescript_language_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

target_include_directories(typescript_language_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
)

set_target_properties(typescript_language_plugin PROPERTIES
    OUTPUT_NAME "typescript-language"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_custom_command(TARGET typescript_language_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:typescript_language_plugin>/typescript-language.json"
)

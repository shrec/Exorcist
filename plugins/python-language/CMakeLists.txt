add_library(python_language_plugin SHARED
    pythonlanguageplugin.h
    pythonlanguageplugin.cpp
)

target_link_libraries(python_language_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

target_include_directories(python_language_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
)

set_target_properties(python_language_plugin PROPERTIES
    OUTPUT_NAME "python-language"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_custom_command(TARGET python_language_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:python_language_plugin>/python-language.json"
)
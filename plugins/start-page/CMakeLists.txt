add_library(start_page_plugin SHARED
    startpageplugin.h
    startpageplugin.cpp
    welcomewidget.h
    welcomewidget.cpp
)

target_link_libraries(start_page_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

target_include_directories(start_page_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(start_page_plugin PROPERTIES
    OUTPUT_NAME  "start-page"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_custom_command(TARGET start_page_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:start_page_plugin>/start-page.json"
)

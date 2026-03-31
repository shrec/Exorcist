add_library(automation_plugin SHARED
    automationplugin.h
    automationplugin.cpp
    taskdiscovery.h
    taskdiscovery.cpp
    taskrunnerpanel.h
    taskrunnerpanel.cpp
)

target_link_libraries(automation_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

target_include_directories(automation_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
)

set_target_properties(automation_plugin PROPERTIES
    OUTPUT_NAME "automation"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_custom_command(TARGET automation_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:automation_plugin>/automation.json"
)

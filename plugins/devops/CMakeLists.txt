add_library(devops_plugin SHARED
    devopsplugin.h
    devopsplugin.cpp
    servicediscovery.h
    servicediscovery.cpp
    devopspanel.h
    devopspanel.cpp
)

target_link_libraries(devops_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

target_include_directories(devops_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
)

set_target_properties(devops_plugin PROPERTIES
    OUTPUT_NAME "devops"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_custom_command(TARGET devops_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:devops_plugin>/devops.json"
)

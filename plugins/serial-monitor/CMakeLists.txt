add_library(serial_monitor_plugin SHARED
    serialmonitorplugin.h
    serialmonitorplugin.cpp
    serialmonitorcontroller.h
    serialmonitorcontroller.cpp
    serialmonitorpanel.h
    serialmonitorpanel.cpp
)

target_link_libraries(serial_monitor_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

if(TARGET Qt6::SerialPort)
    target_link_libraries(serial_monitor_plugin PRIVATE Qt6::SerialPort)
    target_compile_definitions(serial_monitor_plugin PRIVATE EXORCIST_HAS_QT_SERIALPORT)
endif()

target_include_directories(serial_monitor_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
)

set_target_properties(serial_monitor_plugin PROPERTIES
    OUTPUT_NAME "serial-monitor"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_custom_command(TARGET serial_monitor_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:serial_monitor_plugin>/serial-monitor.json"
)
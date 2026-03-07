add_library(copilot_plugin SHARED
    copilotplugin.h
    copilotplugin.cpp
    copilotprovider.h
    copilotprovider.cpp
    copilottoken.h
    copilotauthdialog.h
    copilotauthdialog.cpp
    ${CMAKE_SOURCE_DIR}/src/aiinterface.h
)

target_link_libraries(copilot_plugin PRIVATE
    Qt6::Widgets
    Qt6::Network
    plugin_common
)

target_include_directories(copilot_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(copilot_plugin PROPERTIES
    OUTPUT_NAME "copilot"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_library(byok_plugin SHARED
    byokplugin.h
    byokplugin.cpp
    byokprovider.h
    byokprovider.cpp
    ${CMAKE_SOURCE_DIR}/src/aiinterface.h
)

target_link_libraries(byok_plugin PRIVATE
    Qt6::Widgets
    Qt6::Network
)

target_include_directories(byok_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(byok_plugin PROPERTIES
    OUTPUT_NAME "byok"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

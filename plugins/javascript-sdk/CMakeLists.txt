add_library(javascript_sdk_plugin SHARED
    jspluginsdkplugin.h
    jspluginsdkplugin.cpp
    jspluginruntime.h
    jspluginruntime.cpp
    jshostapi.h
    jshostapi.cpp
    ultralightpluginview.h
    ultralightpluginview.cpp
    ${CMAKE_SOURCE_DIR}/src/plugininterface.h
)

target_link_libraries(javascript_sdk_plugin PRIVATE
    Qt6::Widgets
    Qt6::Network
    ultralight-sdk
)

target_include_directories(javascript_sdk_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(javascript_sdk_plugin PROPERTIES
    OUTPUT_NAME "javascript-sdk"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

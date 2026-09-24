# Headless JS plugin runtime — needs only the JavaScriptCore C API, provided
# by the `javascriptcore` interface target (Ultralight SDK or system JSC).
set(_jssdk_sources
    jspluginsdkplugin.h
    jspluginsdkplugin.cpp
    jspluginruntime.h
    jspluginruntime.cpp
    jshostapi.h
    jshostapi.cpp
    ${CMAKE_SOURCE_DIR}/src/plugininterface.h
)

# The HTML/WebView plugin view is Ultralight-only.
if(EXORCIST_USE_ULTRALIGHT)
    list(APPEND _jssdk_sources
        ultralightpluginview.h
        ultralightpluginview.cpp
    )
endif()

add_library(javascript_sdk_plugin SHARED ${_jssdk_sources})

target_link_libraries(javascript_sdk_plugin PRIVATE
    Qt6::Widgets
    Qt6::Network
    javascriptcore
)

if(EXORCIST_USE_ULTRALIGHT)
    target_link_libraries(javascript_sdk_plugin PRIVATE ultralight-sdk)
    target_compile_definitions(javascript_sdk_plugin PRIVATE EXORCIST_HAS_ULTRALIGHT)
endif()

target_include_directories(javascript_sdk_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(javascript_sdk_plugin PROPERTIES
    OUTPUT_NAME "javascript-sdk"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

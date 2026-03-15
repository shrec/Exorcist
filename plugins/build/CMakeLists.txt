find_package(Qt6 REQUIRED COMPONENTS Concurrent)

add_library(build_plugin SHARED
    buildplugin.h
    buildplugin.cpp
    cmakeintegration.h
    cmakeintegration.cpp
    toolchainmanager.h
    toolchainmanager.cpp
    buildtoolbar.h
    buildtoolbar.cpp
    debuglaunchcontroller.h
    debuglaunchcontroller.cpp
    buildsystemservice.h
    buildsystemservice.cpp
    kitmanager.h
    kitmanager.cpp
)

target_link_libraries(build_plugin PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    plugin_common
    exorcist
)

target_include_directories(build_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(build_plugin PROPERTIES
    OUTPUT_NAME "build"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

# Copy plugin.json next to the plugin binary for manifest loading
add_custom_command(TARGET build_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "${CMAKE_BINARY_DIR}/src/plugins/build.json"
)

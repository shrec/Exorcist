add_library(remote_plugin SHARED
    remoteplugin.h
    remoteplugin.cpp
    sshprofile.h
    sshsession.h
    sshsession.cpp
    sshconnectionmanager.h
    sshconnectionmanager.cpp
    remotefilesystemmodel.h
    remotefilesystemmodel.cpp
    remotefilepanel.h
    remotefilepanel.cpp
    remotesyncservice.h
    remotesyncservice.cpp
    remotehostinfo.h
    remotehostinfo.cpp
)

target_link_libraries(remote_plugin PRIVATE
    Qt6::Widgets
    plugin_common
    exorcist
)

target_include_directories(remote_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(remote_plugin PROPERTIES
    OUTPUT_NAME "remote"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

# Copy plugin.json next to the plugin binary for manifest loading
add_custom_command(TARGET remote_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "${CMAKE_BINARY_DIR}/src/plugins/remote.json"
)

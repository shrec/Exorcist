add_library(lang_pack_plugin SHARED
    langpackplugin.h
    langpackplugin.cpp
    # Language data — compiled into this plugin (not the main executable)
    ${CMAKE_SOURCE_DIR}/src/editor/languages/langcommon.h
    ${CMAKE_SOURCE_DIR}/src/editor/languages/langcommon.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/languages/langbuilders.h
    ${CMAKE_SOURCE_DIR}/src/editor/languages/lang_cpp.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/languages/lang_python.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/languages/lang_javascript.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/languages/lang_web.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/languages/lang_jvm.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/languages/lang_systems.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/languages/lang_scripting.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/languages/lang_shell.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/languages/lang_data.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/languages/lang_config.cpp
)

target_link_libraries(lang_pack_plugin PRIVATE Qt6::Widgets plugin_common exorcist)

target_include_directories(lang_pack_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
)

set_target_properties(lang_pack_plugin PROPERTIES
    OUTPUT_NAME  "lang-pack"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_custom_command(TARGET lang_pack_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:lang_pack_plugin>/lang-pack.json"
    COMMENT "Copying lang-pack plugin manifest"
)

add_library(search_plugin SHARED
    searchplugin.h
    searchplugin.cpp
    ${CMAKE_SOURCE_DIR}/src/search/searchpanel.h
    ${CMAKE_SOURCE_DIR}/src/search/searchpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/search/searchservice.h
    ${CMAKE_SOURCE_DIR}/src/search/searchservice.cpp
    ${CMAKE_SOURCE_DIR}/src/search/searchworker.h
    ${CMAKE_SOURCE_DIR}/src/search/searchworker.cpp
    ${CMAKE_SOURCE_DIR}/src/search/searchindex.h
    ${CMAKE_SOURCE_DIR}/src/search/searchindex.cpp
    ${CMAKE_SOURCE_DIR}/src/search/symbolindex.h
    ${CMAKE_SOURCE_DIR}/src/search/workspaceindexer.h
    ${CMAKE_SOURCE_DIR}/src/search/workspaceindexer.cpp
    ${CMAKE_SOURCE_DIR}/src/search/workspacechunkindex.h
    ${CMAKE_SOURCE_DIR}/src/search/embeddingindex.h
    ${CMAKE_SOURCE_DIR}/src/search/embeddingindex.cpp
)

target_link_libraries(search_plugin PRIVATE Qt6::Widgets plugin_common exorcist)

target_include_directories(search_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plugins/common
)

set_target_properties(search_plugin PROPERTIES
    OUTPUT_NAME  "search"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

add_custom_command(TARGET search_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
        "$<TARGET_FILE_DIR:search_plugin>/search.json"
    COMMENT "Copying search plugin manifest"
)

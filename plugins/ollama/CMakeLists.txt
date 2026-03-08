add_library(ollama_plugin SHARED
    ollamaplugin.h
    ollamaplugin.cpp
    ollamaprovider.h
    ollamaprovider.cpp
    ${CMAKE_SOURCE_DIR}/src/aiinterface.h
)

target_link_libraries(ollama_plugin PRIVATE
    Qt6::Widgets
    Qt6::Network
)

target_include_directories(ollama_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(ollama_plugin PROPERTIES
    OUTPUT_NAME "ollama"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/plugins"
)

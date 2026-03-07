add_library(plugin_common STATIC
    sseparser.h
    sseparser.cpp
)

target_link_libraries(plugin_common PUBLIC Qt6::Core)
target_include_directories(plugin_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(plugin_common PROPERTIES POSITION_INDEPENDENT_CODE ON)

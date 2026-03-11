add_executable(exobridge
    main.cpp
    ../src/process/exobridgecore.h
    ../src/process/ipcprotocol.h
)

target_link_libraries(exobridge PRIVATE Qt6::Network)
target_include_directories(exobridge PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Deploy alongside the IDE binary
if(WIN32)
    set_target_properties(exobridge PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src"
    )
else()
    set_target_properties(exobridge PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src"
    )
endif()

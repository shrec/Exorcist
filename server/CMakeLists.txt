add_executable(exobridge
    main.cpp
    mcpbridgeservice.h
    mcpbridgeservice.cpp
    gitwatchservice.h
    gitwatchservice.cpp
    authtokenservice.h
    authtokenservice.cpp
    ../src/process/exobridgecore.h
    ../src/process/exobridgecore.cpp
    ../src/process/ipcprotocol.h
    ../src/process/ipcprotocol.cpp
)

target_link_libraries(exobridge PRIVATE Qt6::Network)
if(WIN32)
    target_link_libraries(exobridge PRIVATE Advapi32)
endif()
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

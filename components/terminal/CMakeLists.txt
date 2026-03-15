add_library(terminal_component SHARED
    terminalcomponent.h
    terminalcomponent.cpp

    # Terminal sources from core (will be removed from core exe later)
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalpanel.h
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalpanel.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalwidget.h
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalview.h
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalview.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalscreen.h
    ${CMAKE_SOURCE_DIR}/src/terminal/terminalscreen.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/ptybackend.h
    ${CMAKE_SOURCE_DIR}/src/terminal/ptybackend.cpp
)

target_link_libraries(terminal_component PRIVATE
    Qt6::Widgets
)

target_include_directories(terminal_component PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(terminal_component PROPERTIES
    OUTPUT_NAME "terminal"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/components"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/components"
)

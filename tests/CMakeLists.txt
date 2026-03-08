find_package(Qt6 REQUIRED COMPONENTS Test Widgets Network)

# ── ServiceRegistry tests ─────────────────────────────────────────────────────
add_executable(test_serviceregistry
    test_serviceregistry.cpp
    ${CMAKE_SOURCE_DIR}/src/serviceregistry.cpp
    ${CMAKE_SOURCE_DIR}/src/serviceregistry.h
)
target_link_libraries(test_serviceregistry PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_serviceregistry PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ServiceRegistry COMMAND test_serviceregistry)

# ── PieceTableBuffer tests ────────────────────────────────────────────────────
add_executable(test_piecetable
    test_piecetable.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/piecetablebuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/piecetablebuffer.h
)
target_link_libraries(test_piecetable PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_piecetable PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/editor)
add_test(NAME PieceTable COMMAND test_piecetable)

# ── SseParser tests ───────────────────────────────────────────────────────────
add_executable(test_sseparser
    test_sseparser.cpp
    ${CMAKE_SOURCE_DIR}/plugins/common/sseparser.cpp
    ${CMAKE_SOURCE_DIR}/plugins/common/sseparser.h
)
target_link_libraries(test_sseparser PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_sseparser PRIVATE ${CMAKE_SOURCE_DIR}/plugins/common)
add_test(NAME SseParser COMMAND test_sseparser)

# ── ThemeManager tests ─────────────────────────────────────────────────────────
add_executable(test_thememanager
    test_thememanager.cpp
    ${CMAKE_SOURCE_DIR}/src/thememanager.cpp
    ${CMAKE_SOURCE_DIR}/src/thememanager.h
)
target_link_libraries(test_thememanager PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_thememanager PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ThemeManager COMMAND test_thememanager)

# Deploy Qt DLLs so tests can run (Windows)
if(WIN32 AND EXORCIST_DEPLOY_QT)
    find_program(WINDEPLOYQT windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET test_serviceregistry POST_BUILD
            COMMAND "${WINDEPLOYQT}" --compiler-runtime "$<TARGET_FILE:test_serviceregistry>"
            COMMENT "Deploying Qt for tests"
        )
    endif()
endif()

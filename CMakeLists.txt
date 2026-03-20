cmake_minimum_required(VERSION 3.21)

project(Exorcist VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Exorcist requires a 64-bit toolchain.")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent Sql Svg)
find_package(Qt6 QUIET COMPONENTS SerialPort)

option(EXORCIST_USE_LLVM "Enable LLVM integration stubs" OFF)
option(EXORCIST_DEPLOY_QT "Run windeployqt after build on Windows" ON)
option(EXORCIST_USE_TREESITTER "Enable tree-sitter syntax highlighting" ON)
option(EXORCIST_USE_ULTRALIGHT "Enable Ultralight HTML chat renderer" ON)
if(EXORCIST_USE_LLVM)
    find_package(LLVM CONFIG REQUIRED)
endif()

include(FetchContent)

# ── Tree-sitter ──────────────────────────────────────────────────────────────
if(EXORCIST_USE_TREESITTER)
    #include(FetchContent)  # already included above

    FetchContent_Declare(
        tree-sitter
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
        GIT_TAG        v0.25.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(
        tree-sitter-c
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-c.git
        GIT_TAG        v0.23.5
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(
        tree-sitter-cpp
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-cpp.git
        GIT_TAG        v0.23.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(
        tree-sitter-python
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-python.git
        GIT_TAG        v0.23.6
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(
        tree-sitter-javascript
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-javascript.git
        GIT_TAG        v0.23.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(
        tree-sitter-typescript
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-typescript.git
        GIT_TAG        v0.23.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(
        tree-sitter-rust
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-rust.git
        GIT_TAG        v0.23.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(
        tree-sitter-json
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-json.git
        GIT_TAG        v0.24.8
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(
        tree-sitter-go
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-go.git
        GIT_TAG        v0.23.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(
        tree-sitter-yaml
        GIT_REPOSITORY https://github.com/tree-sitter-grammars/tree-sitter-yaml.git
        GIT_TAG        v0.7.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(
        tree-sitter-toml
        GIT_REPOSITORY https://github.com/tree-sitter-grammars/tree-sitter-toml.git
        GIT_TAG        v0.7.0
        GIT_SHALLOW    TRUE
    )

    # Populate sources only — do NOT add subdirectories (grammar CMakeLists
    # define conflicting targets like "ts-test").
    FetchContent_Populate(tree-sitter)
    FetchContent_Populate(tree-sitter-c)
    FetchContent_Populate(tree-sitter-cpp)
    FetchContent_Populate(tree-sitter-python)
    FetchContent_Populate(tree-sitter-javascript)
    FetchContent_Populate(tree-sitter-typescript)
    FetchContent_Populate(tree-sitter-rust)
    FetchContent_Populate(tree-sitter-json)
    FetchContent_Populate(tree-sitter-go)
    FetchContent_Populate(tree-sitter-yaml)
    FetchContent_Populate(tree-sitter-toml)

    # Build tree-sitter core as a static C library
    set(TS_SRC_DIR "${tree-sitter_SOURCE_DIR}/lib/src")
    set(TS_INC_DIR "${tree-sitter_SOURCE_DIR}/lib/include")
    add_library(tree-sitter-core STATIC "${TS_SRC_DIR}/lib.c")
    target_include_directories(tree-sitter-core PUBLIC "${TS_INC_DIR}" PRIVATE "${TS_SRC_DIR}")
    set_target_properties(tree-sitter-core PROPERTIES C_STANDARD 11)

    # Grammar static libraries
    add_library(ts-grammar-c STATIC "${tree-sitter-c_SOURCE_DIR}/src/parser.c")
    target_include_directories(ts-grammar-c PRIVATE "${tree-sitter-c_SOURCE_DIR}/src" "${TS_INC_DIR}")

    add_library(ts-grammar-cpp STATIC
        "${tree-sitter-cpp_SOURCE_DIR}/src/parser.c"
        "${tree-sitter-cpp_SOURCE_DIR}/src/scanner.c"
    )
    target_include_directories(ts-grammar-cpp PRIVATE "${tree-sitter-cpp_SOURCE_DIR}/src" "${TS_INC_DIR}")

    add_library(ts-grammar-python STATIC
        "${tree-sitter-python_SOURCE_DIR}/src/parser.c"
        "${tree-sitter-python_SOURCE_DIR}/src/scanner.c"
    )
    target_include_directories(ts-grammar-python PRIVATE "${tree-sitter-python_SOURCE_DIR}/src" "${TS_INC_DIR}")

    add_library(ts-grammar-javascript STATIC
        "${tree-sitter-javascript_SOURCE_DIR}/src/parser.c"
        "${tree-sitter-javascript_SOURCE_DIR}/src/scanner.c"
    )
    target_include_directories(ts-grammar-javascript PRIVATE "${tree-sitter-javascript_SOURCE_DIR}/src" "${TS_INC_DIR}")

    add_library(ts-grammar-typescript STATIC
        "${tree-sitter-typescript_SOURCE_DIR}/typescript/src/parser.c"
        "${tree-sitter-typescript_SOURCE_DIR}/typescript/src/scanner.c"
    )
    target_include_directories(ts-grammar-typescript PRIVATE "${tree-sitter-typescript_SOURCE_DIR}/typescript/src" "${TS_INC_DIR}")

    add_library(ts-grammar-rust STATIC
        "${tree-sitter-rust_SOURCE_DIR}/src/parser.c"
        "${tree-sitter-rust_SOURCE_DIR}/src/scanner.c"
    )
    target_include_directories(ts-grammar-rust PRIVATE "${tree-sitter-rust_SOURCE_DIR}/src" "${TS_INC_DIR}")

    add_library(ts-grammar-json STATIC "${tree-sitter-json_SOURCE_DIR}/src/parser.c")
    target_include_directories(ts-grammar-json PRIVATE "${tree-sitter-json_SOURCE_DIR}/src" "${TS_INC_DIR}")

    add_library(ts-grammar-go STATIC "${tree-sitter-go_SOURCE_DIR}/src/parser.c")
    target_include_directories(ts-grammar-go PRIVATE "${tree-sitter-go_SOURCE_DIR}/src" "${TS_INC_DIR}")

    add_library(ts-grammar-yaml STATIC
        "${tree-sitter-yaml_SOURCE_DIR}/src/parser.c"
        "${tree-sitter-yaml_SOURCE_DIR}/src/scanner.c"
    )
    target_include_directories(ts-grammar-yaml PRIVATE "${tree-sitter-yaml_SOURCE_DIR}/src" "${TS_INC_DIR}")

    add_library(ts-grammar-toml STATIC
        "${tree-sitter-toml_SOURCE_DIR}/src/parser.c"
        "${tree-sitter-toml_SOURCE_DIR}/src/scanner.c"
    )
    target_include_directories(ts-grammar-toml PRIVATE "${tree-sitter-toml_SOURCE_DIR}/src" "${TS_INC_DIR}")
endif()

add_subdirectory(cmake/luajit)
if(EXORCIST_USE_ULTRALIGHT)
    add_subdirectory(cmake/ultralight)
endif()
add_subdirectory(src)
add_subdirectory(server)
add_subdirectory(plugins)
add_subdirectory(components)

enable_testing()
add_subdirectory(tests)

# ── Source Graph reindex after build ─────────────────────────────────────────
option(EXORCIST_REINDEX_AFTER_BUILD "Run source_graph incremental reindex after build" ON)
if(EXORCIST_REINDEX_AFTER_BUILD)
    find_program(PYTHON_EXE python3 python)
    if(PYTHON_EXE)
        set(SOURCE_GRAPH_SCRIPT "${CMAKE_SOURCE_DIR}/tools/source_graph_kit/source_graph.py")
        if(EXISTS "${SOURCE_GRAPH_SCRIPT}")
            add_custom_target(source_graph_reindex ALL
                COMMAND ${CMAKE_COMMAND} -E env PYTHONIOENCODING=utf-8
                        ${PYTHON_EXE} "${SOURCE_GRAPH_SCRIPT}" build -i
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                COMMENT "Incremental source graph reindex..."
                USES_TERMINAL
            )
            # Run after all real targets are built
            add_dependencies(source_graph_reindex exorcist)
        endif()
    endif()
endif()

# ── CPack packaging ──────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME "Exorcist")
set(CPACK_PACKAGE_VENDOR "Exorcist Contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fast native code editor and IDE built with Qt 6")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Exorcist IDE")
    set(CPACK_NSIS_PACKAGE_NAME "Exorcist")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Exorcist")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Exorcist Contributors")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6 (>= 6.5)")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
endif()

include(CPack)

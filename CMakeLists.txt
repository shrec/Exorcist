cmake_minimum_required(VERSION 3.21)

project(Exorcist VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Exorcist requires a 64-bit toolchain.")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent)

option(EXORCIST_USE_LLVM "Enable LLVM integration stubs" OFF)
option(EXORCIST_DEPLOY_QT "Run windeployqt after build on Windows" ON)
if(EXORCIST_USE_LLVM)
    find_package(LLVM CONFIG REQUIRED)
endif()

add_subdirectory(cmake/luajit)
add_subdirectory(src)
add_subdirectory(plugins)

enable_testing()
add_subdirectory(tests)

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

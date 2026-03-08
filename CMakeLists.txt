cmake_minimum_required(VERSION 3.21)

project(Exorcist VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Exorcist requires a 64-bit toolchain.")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

option(EXORCIST_USE_LLVM "Enable LLVM integration stubs" OFF)
option(EXORCIST_DEPLOY_QT "Run windeployqt after build on Windows" ON)
if(EXORCIST_USE_LLVM)
    find_package(LLVM CONFIG REQUIRED)
endif()

add_subdirectory(src)
add_subdirectory(plugins)

enable_testing()
add_subdirectory(tests)

cmake_minimum_required(VERSION 3.21)

project(Exorcist VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

option(EXORCIST_USE_LLVM "Enable LLVM integration stubs" OFF)
if(EXORCIST_USE_LLVM)
    find_package(LLVM CONFIG REQUIRED)
endif()

add_subdirectory(src)

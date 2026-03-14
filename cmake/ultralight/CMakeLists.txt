# ── Ultralight SDK integration ─────────────────────────────────────────────────
#
# Downloads the prebuilt Ultralight SDK and creates an INTERFACE target.
# The SDK provides a lightweight HTML/CSS/JS renderer for the chat panel.
#
# Libraries: Ultralight, UltralightCore, WebCore, JavaScriptCore
# CPU rendering mode (BitmapSurface) — no GPU/OpenGL dependency.
#
cmake_minimum_required(VERSION 3.21)

# Disable AUTOMOC — Ultralight is pure C/C++, no Qt classes here.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

set(ULTRALIGHT_SDK_VERSION "1.4.0-beta" CACHE STRING "Ultralight SDK version")

# ── Platform archive selection ────────────────────────────────────────────────
if(WIN32)
    set(_UL_PLATFORM "win-x64")
    set(_UL_LIB_PREFIX "")
    set(_UL_LIB_SUFFIX ".lib")
    set(_UL_DLL_SUFFIX ".dll")
elseif(APPLE)
    set(_UL_PLATFORM "mac-x64")
    set(_UL_LIB_PREFIX "lib")
    set(_UL_LIB_SUFFIX ".dylib")
    set(_UL_DLL_SUFFIX ".dylib")
else()
    set(_UL_PLATFORM "linux-x64")
    set(_UL_LIB_PREFIX "lib")
    set(_UL_LIB_SUFFIX ".so")
    set(_UL_DLL_SUFFIX ".so")
endif()

# ── SDK location ──────────────────────────────────────────────────────────────
# Users can pre-download the SDK to third_party/ultralight-sdk/ or let
# FetchContent grab it.  Check local path first.
set(ULTRALIGHT_SDK_PATH "${CMAKE_SOURCE_DIR}/third_party/ultralight-sdk" CACHE PATH
    "Path to pre-downloaded Ultralight SDK")

if(EXISTS "${ULTRALIGHT_SDK_PATH}/include/Ultralight/Ultralight.h")
    message(STATUS "Ultralight SDK found at ${ULTRALIGHT_SDK_PATH}")
    set(_UL_ROOT "${ULTRALIGHT_SDK_PATH}")
    set(_UL_ROOT "${ULTRALIGHT_SDK_PATH}" PARENT_SCOPE)
else()
    message(STATUS "Ultralight SDK not found locally, using FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        ultralight-sdk
        URL "https://ultralight-sdk.sfo2.cdn.digitaloceanspaces.com/ultralight-sdk-latest-${_UL_PLATFORM}.7z"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Populate(ultralight-sdk)
    set(_UL_ROOT "${ultralight-sdk_SOURCE_DIR}")
    set(_UL_ROOT "${ultralight-sdk_SOURCE_DIR}" PARENT_SCOPE)
endif()

# ── INTERFACE target ──────────────────────────────────────────────────────────
add_library(ultralight-sdk INTERFACE)

target_include_directories(ultralight-sdk INTERFACE "${_UL_ROOT}/include")

# Link directories (import libs on Windows, shared libs on Unix)
if(WIN32)
    # Generate mingw-compatible import libs (.a) from DLLs if they don't exist.
    # The SDK ships MSVC .lib files which don't work with llvm-mingw/lld.
    set(_UL_DLLS Ultralight UltralightCore AppCore WebCore)
    foreach(_DLL_NAME ${_UL_DLLS})
        set(_A_FILE "${_UL_ROOT}/lib/lib${_DLL_NAME}.a")
        if(NOT EXISTS "${_A_FILE}")
            find_program(_GENDEF gendef)
            find_program(_DLLTOOL dlltool)
            if(_GENDEF AND _DLLTOOL)
                execute_process(
                    COMMAND "${_GENDEF}" "${_UL_ROOT}/bin/${_DLL_NAME}.dll"
                    WORKING_DIRECTORY "${_UL_ROOT}/lib"
                    OUTPUT_QUIET ERROR_QUIET)
                execute_process(
                    COMMAND "${_DLLTOOL}" -d "${_UL_ROOT}/lib/${_DLL_NAME}.def"
                            -l "${_A_FILE}" -D "${_DLL_NAME}.dll"
                    OUTPUT_QUIET ERROR_QUIET)
            endif()
        endif()
    endforeach()

    target_link_directories(ultralight-sdk INTERFACE "${_UL_ROOT}/lib")
    target_link_libraries(ultralight-sdk INTERFACE
        Ultralight UltralightCore AppCore WebCore
    )
else()
    target_link_directories(ultralight-sdk INTERFACE "${_UL_ROOT}/bin")
    target_link_libraries(ultralight-sdk INTERFACE
        "${_UL_LIB_PREFIX}Ultralight${_UL_LIB_SUFFIX}"
        "${_UL_LIB_PREFIX}UltralightCore${_UL_LIB_SUFFIX}"
        "${_UL_LIB_PREFIX}AppCore${_UL_LIB_SUFFIX}"
        "${_UL_LIB_PREFIX}WebCore${_UL_LIB_SUFFIX}"
    )
endif()

# ── Copy runtime DLLs/resources to build output ──────────────────────────────
# This function copies Ultralight DLLs and resources next to the executable.
function(ultralight_deploy_runtime TARGET_NAME)
    if(WIN32)
        set(_BIN_DIR "${_UL_ROOT}/bin")
        set(_RES_DIR "${_UL_ROOT}/resources")

        # Copy DLLs
        file(GLOB _UL_DLLS "${_BIN_DIR}/*.dll")
        foreach(_DLL ${_UL_DLLS})
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_DLL}" "$<TARGET_FILE_DIR:${TARGET_NAME}>"
                VERBATIM
            )
        endforeach()

        # Copy resources/ directory (ICU data, certs, etc.)
        if(EXISTS "${_RES_DIR}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_RES_DIR}"
                    "$<TARGET_FILE_DIR:${TARGET_NAME}>/ultralight-resources"
                VERBATIM
            )
        endif()
    elseif(APPLE)
        # Copy .dylibs
        file(GLOB _UL_LIBS "${_UL_ROOT}/bin/*.dylib")
        foreach(_LIB ${_UL_LIBS})
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_LIB}" "$<TARGET_FILE_DIR:${TARGET_NAME}>"
            )
        endforeach()
    else()
        # Copy .so files
        file(GLOB _UL_LIBS "${_UL_ROOT}/bin/*.so*")
        foreach(_LIB ${_UL_LIBS})
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_LIB}" "$<TARGET_FILE_DIR:${TARGET_NAME}>"
            )
        endforeach()
    endif()
endfunction()

# Export root path for resource copying
set(ULTRALIGHT_ROOT "${_UL_ROOT}" PARENT_SCOPE)

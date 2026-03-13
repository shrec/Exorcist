# cmake/report_binary_size.cmake
# Post-build script: reports binary size and checks against budget.
# Usage: cmake -DBINARY_PATH=<exe> -DMAX_BYTES=<limit> -P report_binary_size.cmake

if(NOT EXISTS "${BINARY_PATH}")
    message(WARNING "Binary not found: ${BINARY_PATH}")
    return()
endif()

file(SIZE "${BINARY_PATH}" BINARY_SIZE)
math(EXPR SIZE_KB "${BINARY_SIZE} / 1024")
math(EXPR SIZE_MB "${BINARY_SIZE} / 1048576")

message("=== Binary Size Report ===")
message("  File:   ${BINARY_PATH}")
message("  Size:   ${SIZE_MB} MB (${SIZE_KB} KB, ${BINARY_SIZE} bytes)")

if(MAX_BYTES AND BINARY_SIZE GREATER MAX_BYTES)
    math(EXPR MAX_MB "${MAX_BYTES} / 1048576")
    message(WARNING "  ⚠ OVER BUDGET: ${SIZE_MB} MB exceeds ${MAX_MB} MB limit!")
else()
    if(MAX_BYTES)
        math(EXPR MAX_MB "${MAX_BYTES} / 1048576")
        message("  Budget: ${MAX_MB} MB — OK")
    endif()
endif()

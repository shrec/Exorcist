# ── LuaJIT 2.1 static library build (x64, Windows, Clang/GCC) ──────────────
#
# Replicates the official Makefile build chain:
#   1. minilua     → host Lua interpreter
#   2. genversion  → luajit.h with version
#   3. dynasm      → host/buildvm_arch.h
#   4. buildvm     → generated headers + lj_vm.obj
#   5. ljamalg.c   → libluajit-5.1.a (static)
#
# This file lives in cmake/luajit/ (outside the git submodule) and
# references sources from third_party/luajit/ via LUAJIT_ROOT.
#
cmake_minimum_required(VERSION 3.21)
project(luajit C)

# LuaJIT is pure C — disable Qt's AUTOMOC/AUTOUIC/AUTORCC which the parent
# project enables globally.  Without this, AUTOMOC adds a spurious
# mocs_compilation.cpp to host tools like buildvm, forcing the CXX linker
# and producing a broken lj_vm.obj.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

set(LUAJIT_ROOT ${CMAKE_SOURCE_DIR}/third_party/luajit)
set(LUAJIT_DIR  ${LUAJIT_ROOT}/src)
set(DYNASM_DIR  ${LUAJIT_ROOT}/dynasm)
set(GEN_DIR     ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(HOST_DIR    ${CMAKE_CURRENT_BINARY_DIR}/host)

file(MAKE_DIRECTORY ${GEN_DIR})
file(MAKE_DIRECTORY ${HOST_DIR})

# ── Platform detection ─────────────────────────────────────────────────────
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LUAJIT_ARCH "x64")
    set(DASM_ARCH "x64")
    set(DASM_FLAGS -D ENDIAN_LE -D P64 -D JIT -D FFI -D VER=80)
else()
    set(LUAJIT_ARCH "x86")
    set(DASM_ARCH "x86")
    set(DASM_FLAGS -D ENDIAN_LE -D JIT -D FFI -D VER=)
endif()

if(WIN32)
    list(APPEND DASM_FLAGS -D WIN)
endif()

set(DASM_DASC ${LUAJIT_DIR}/vm_${DASM_ARCH}.dasc)

# ── Common defines ─────────────────────────────────────────────────────────
set(LUAJIT_DEFINES
    LUAJIT_ENABLE_LUA52COMPAT
    _FILE_OFFSET_BITS=64
    _LARGEFILE_SOURCE
)
if(WIN32)
    list(APPEND LUAJIT_DEFINES _CRT_SECURE_NO_WARNINGS)
endif()
if(UNIX AND NOT APPLE)
    list(APPEND LUAJIT_DEFINES LUAJIT_UNWIND_EXTERNAL)
    set(LUAJIT_UNWIND_FLAGS -funwind-tables)
endif()

# ── Step 1: Build minilua (host tool) ──────────────────────────────────────
add_executable(minilua ${LUAJIT_DIR}/host/minilua.c)
target_include_directories(minilua PRIVATE ${LUAJIT_DIR})
if(NOT WIN32)
    target_link_libraries(minilua m)
endif()
set_target_properties(minilua PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${HOST_DIR}
)

# ── Step 2: Generate luajit.h (version embedding) ─────────────────────────
# Copy relver for genversion.lua
add_custom_command(
    OUTPUT  ${GEN_DIR}/luajit_relver.txt
    COMMAND ${CMAKE_COMMAND} -E copy
            ${LUAJIT_ROOT}/.relver
            ${GEN_DIR}/luajit_relver.txt
    DEPENDS ${LUAJIT_ROOT}/.relver
    COMMENT "Copying release version"
)

add_custom_command(
    OUTPUT  ${GEN_DIR}/luajit.h
    COMMAND ${CMAKE_COMMAND} -E copy
            ${LUAJIT_DIR}/luajit_rolling.h ${GEN_DIR}/luajit_rolling.h
    COMMAND minilua ${LUAJIT_DIR}/host/genversion.lua
            ${GEN_DIR}/luajit_rolling.h
            ${GEN_DIR}/luajit_relver.txt
            ${GEN_DIR}/luajit.h
    WORKING_DIRECTORY ${GEN_DIR}
    DEPENDS minilua ${GEN_DIR}/luajit_relver.txt
            ${LUAJIT_DIR}/luajit_rolling.h
            ${LUAJIT_DIR}/host/genversion.lua
    COMMENT "Generating luajit.h"
)

# ── Step 3: DynASM → buildvm_arch.h ───────────────────────────────────────
add_custom_command(
    OUTPUT  ${HOST_DIR}/buildvm_arch.h
    COMMAND minilua ${DYNASM_DIR}/dynasm.lua
            ${DASM_FLAGS}
            -o ${HOST_DIR}/buildvm_arch.h
            ${DASM_DASC}
    DEPENDS minilua
            ${DYNASM_DIR}/dynasm.lua
            ${DYNASM_DIR}/dasm_x86.lua
            ${DYNASM_DIR}/dasm_x64.lua
            ${DASM_DASC}
    COMMENT "DynASM → buildvm_arch.h"
)

# ── Step 4: Build buildvm (host tool) ─────────────────────────────────────
add_executable(buildvm
    ${LUAJIT_DIR}/host/buildvm.c
    ${LUAJIT_DIR}/host/buildvm_asm.c
    ${LUAJIT_DIR}/host/buildvm_peobj.c
    ${LUAJIT_DIR}/host/buildvm_lib.c
    ${LUAJIT_DIR}/host/buildvm_fold.c
)
target_include_directories(buildvm PRIVATE
    ${LUAJIT_DIR}
    ${HOST_DIR}       # for buildvm_arch.h
    ${GEN_DIR}        # for luajit.h
)
target_compile_definitions(buildvm PRIVATE ${LUAJIT_DEFINES})
set_target_properties(buildvm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${HOST_DIR}
)
# buildvm_arch.h + luajit.h must be generated before buildvm compiles
add_custom_target(buildvm_deps DEPENDS
    ${HOST_DIR}/buildvm_arch.h
    ${GEN_DIR}/luajit.h
)
add_dependencies(buildvm buildvm_deps)

# ── Step 5: buildvm → generated headers + lj_vm object ────────────────────
set(LJLIB_C
    ${LUAJIT_DIR}/lib_base.c
    ${LUAJIT_DIR}/lib_math.c
    ${LUAJIT_DIR}/lib_bit.c
    ${LUAJIT_DIR}/lib_string.c
    ${LUAJIT_DIR}/lib_table.c
    ${LUAJIT_DIR}/lib_io.c
    ${LUAJIT_DIR}/lib_os.c
    ${LUAJIT_DIR}/lib_package.c
    ${LUAJIT_DIR}/lib_debug.c
    ${LUAJIT_DIR}/lib_jit.c
    ${LUAJIT_DIR}/lib_ffi.c
    ${LUAJIT_DIR}/lib_buffer.c
)

if(WIN32)
    set(LJVM_MODE peobj)
    set(LJVM_OUT ${GEN_DIR}/lj_vm.obj)
else()
    set(LJVM_MODE elfasm)
    set(LJVM_OUT ${GEN_DIR}/lj_vm.S)
endif()

add_custom_command(
    OUTPUT  ${LJVM_OUT}
    COMMAND buildvm -m ${LJVM_MODE} -o ${LJVM_OUT}
    DEPENDS buildvm
    COMMENT "buildvm → lj_vm"
)

add_custom_command(
    OUTPUT  ${GEN_DIR}/lj_bcdef.h
    COMMAND buildvm -m bcdef -o ${GEN_DIR}/lj_bcdef.h ${LJLIB_C}
    DEPENDS buildvm ${LJLIB_C}
    COMMENT "buildvm → lj_bcdef.h"
)

add_custom_command(
    OUTPUT  ${GEN_DIR}/lj_ffdef.h
    COMMAND buildvm -m ffdef -o ${GEN_DIR}/lj_ffdef.h ${LJLIB_C}
    DEPENDS buildvm ${LJLIB_C}
    COMMENT "buildvm → lj_ffdef.h"
)

add_custom_command(
    OUTPUT  ${GEN_DIR}/lj_libdef.h
    COMMAND buildvm -m libdef -o ${GEN_DIR}/lj_libdef.h ${LJLIB_C}
    DEPENDS buildvm ${LJLIB_C}
    COMMENT "buildvm → lj_libdef.h"
)

add_custom_command(
    OUTPUT  ${GEN_DIR}/lj_recdef.h
    COMMAND buildvm -m recdef -o ${GEN_DIR}/lj_recdef.h ${LJLIB_C}
    DEPENDS buildvm ${LJLIB_C}
    COMMENT "buildvm → lj_recdef.h"
)

add_custom_command(
    OUTPUT  ${GEN_DIR}/lj_folddef.h
    COMMAND buildvm -m folddef -o ${GEN_DIR}/lj_folddef.h
            ${LUAJIT_DIR}/lj_opt_fold.c
    DEPENDS buildvm ${LUAJIT_DIR}/lj_opt_fold.c
    COMMENT "buildvm → lj_folddef.h"
)

add_custom_command(
    OUTPUT  ${GEN_DIR}/vmdef.lua
    COMMAND buildvm -m vmdef -o ${GEN_DIR}/vmdef.lua ${LJLIB_C}
    DEPENDS buildvm ${LJLIB_C}
    COMMENT "buildvm → vmdef.lua"
)

# Umbrella target for all generated headers
add_custom_target(luajit_generated DEPENDS
    ${LJVM_OUT}
    ${GEN_DIR}/lj_bcdef.h
    ${GEN_DIR}/lj_ffdef.h
    ${GEN_DIR}/lj_libdef.h
    ${GEN_DIR}/lj_recdef.h
    ${GEN_DIR}/lj_folddef.h
    ${GEN_DIR}/vmdef.lua
    ${GEN_DIR}/luajit.h
)

# ── Step 6: Static library from amalgamation ───────────────────────────────
add_library(libluajit STATIC
    ${LUAJIT_DIR}/ljamalg.c
    ${LJVM_OUT}
)
add_dependencies(libluajit luajit_generated)

target_include_directories(libluajit
    PUBLIC  ${LUAJIT_DIR}     # lua.h, lauxlib.h, lualib.h
            ${GEN_DIR}        # luajit.h, generated headers
    PRIVATE ${LUAJIT_DIR}
            ${GEN_DIR}
)

target_compile_definitions(libluajit PRIVATE
    ${LUAJIT_DEFINES}
    LUA_CORE
    ljamalg_c
)

# Optimize the amalgamation
target_compile_options(libluajit PRIVATE
    -O2 -fomit-frame-pointer
    -U_FORTIFY_SOURCE
    -fno-stack-protector
    ${LUAJIT_UNWIND_FLAGS}
)

set_target_properties(libluajit PROPERTIES
    OUTPUT_NAME "luajit-5.1"
    POSITION_INDEPENDENT_CODE OFF
)

# Export the target name for the parent project
set(LUAJIT_INCLUDE_DIRS ${LUAJIT_DIR} ${GEN_DIR} PARENT_SCOPE)

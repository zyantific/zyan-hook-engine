cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
include(GenerateExportHeader)
include(GNUInstallDirs)

project(Zyrex VERSION 1.0)

# =============================================================================================== #
# Overridable options                                                                             #
# =============================================================================================== #

# Build configuration
option(ZYREX_BUILD_SHARED_LIB
    "Build shared library"
    OFF)
option(ZYREX_WHOLE_PROGRAM_OPTIMIZATION
    "Enable whole program optimization"
    OFF)
option(ZYREX_NO_LIBC
    "Don't use any C standard library functions (for exotic build-envs like kernel drivers)"
    OFF)
option(ZYREX_BUILD_EXAMPLES 
    "Build examples" 
    ON)
option(ZYREX_DEV_MODE
    "Enable developer mode (-Wall, -Werror, ...)"
    OFF)

# =============================================================================================== #
# Developer mode                                                                                  #
# =============================================================================================== #

# If in developer mode, hack global compiler flags.
if (ZYREX_DEV_MODE)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR
            "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR
            "${CMAKE_C_COMPILER_ID}" STREQUAL "AppleClang")
        set(compiler_specific "-pedantic -Wextra -Werror")
    elseif (MSVC)
        set(compiler_specific "/WX /W4 /TC")
    endif ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${compiler_specific}"
        CACHE STRING "Flags used by the compiler during all build types." FORCE)
endif ()

# =============================================================================================== #
# Library configuration                                                                           #
# =============================================================================================== #

if (ZYREX_BUILD_SHARED_LIB)
    add_library("Zyrex" SHARED)
else ()
    add_library("Zyrex" STATIC)
endif ()

option(ZYDIS_MINIMAL_MODE "" ON)
option(ZYDIS_FEATURE_FORMATTER "" OFF)
option(ZYDIS_FEATURE_EVEX "" OFF)
option(ZYDIS_FEATURE_MVEX "" OFF)
option(ZYDIS_BUILD_EXAMPLES "" OFF)
option(ZYDIS_BUILD_TOOLS "" OFF)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/dependencies/zydis" EXCLUDE_FROM_ALL)
target_link_libraries("Zyrex" PRIVATE "Zydis")

target_include_directories("Zyrex" 
    PUBLIC "include" ${PROJECT_BINARY_DIR}
    PRIVATE "src")
target_compile_definitions("Zyrex" PRIVATE "_CRT_SECURE_NO_WARNINGS" "ZYREX_EXPORTS")
if (ZYREX_WHOLE_PROGRAM_OPTIMIZATION AND MSVC)
    set_target_properties("Zyrex" PROPERTIES COMPILE_FLAGS "/GL")
    set_target_properties("Zyrex" PROPERTIES LINK_FLAGS_RELEASE "/LTCG")
    set_target_properties("Zyrex" PROPERTIES STATIC_LIBRARY_FLAGS_RELEASE "/LTCG")
endif ()
generate_export_header("Zyrex" BASE_NAME "ZYREX" EXPORT_FILE_NAME "ZyrexExportConfig.h")

if (ZYREX_NO_LIBC)
    target_compile_definitions("Zyrex" PUBLIC "ZYREX_NO_LIBC")
endif ()

target_sources("Zyrex"
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Defines.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/InlineHook.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Status.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Zyrex.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Internal/Trampoline.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Internal/Utils.h"
        "src/InlineHook.c"
        "src/Trampoline.c"
        "src/Utils.c"
        "src/Zyrex.c")

if (ZYREX_BUILD_SHARED_LIB AND WIN32)
    target_sources("Zyrex" PRIVATE "src/VersionInfo.rc")
endif ()

# TODO: Install CMake config.
install(TARGETS "Zyrex"
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY "include" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# =============================================================================================== #
# Examples                                                                                        #
# =============================================================================================== #

if (ZYREX_BUILD_EXAMPLES)
    add_executable("InlineHook" "examples/InlineHook.c")
    target_link_libraries("InlineHook" "Zyrex")
    set_target_properties("InlineHook" PROPERTIES FOLDER "Examples/InlineHook")
    target_compile_definitions("InlineHook" PRIVATE "_CRT_SECURE_NO_WARNINGS")
    if (ZYREX_WHOLE_PROGRAM_OPTIMIZATION AND MSVC)
        set_target_properties("InlineHook" PROPERTIES COMPILE_FLAGS "/GL")
        set_target_properties("InlineHook" PROPERTIES STATIC_LIBRARY_FLAGS_RELEASE "/LTCG")
    endif ()
endif ()

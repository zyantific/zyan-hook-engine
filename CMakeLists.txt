cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
include(GenerateExportHeader)
include(GNUInstallDirs)

project(Zyrex VERSION 1.0.0.0 LANGUAGES C CXX)

# =============================================================================================== #
# Overridable options                                                                             #
# =============================================================================================== #

# Build configuration
option(ZYREX_BUILD_SHARED_LIB
    "Build shared library"
    OFF)
option(ZYREX_BUILD_EXAMPLES 
    "Build examples" 
    ON)

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
option(ZYDIS_FEATURE_AVX512 "" OFF)
option(ZYDIS_FEATURE_KNC "" OFF)
option(ZYDIS_BUILD_EXAMPLES "" OFF)
option(ZYDIS_BUILD_TOOLS "" OFF)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../zydis" "${CMAKE_CURRENT_BINARY_DIR}/zydis" 
    EXCLUDE_FROM_ALL)
target_link_libraries("Zyrex" PUBLIC "Zydis")

target_include_directories("Zyrex" 
    PUBLIC "include" ${PROJECT_BINARY_DIR}
    PRIVATE "src")
target_compile_definitions("Zyrex" PRIVATE "_CRT_SECURE_NO_WARNINGS" "ZYREX_EXPORTS")
zyan_maybe_enable_wpo_for_lib("Zyrex")
generate_export_header("Zyrex" BASE_NAME "ZYREX" EXPORT_FILE_NAME "ZyrexExportConfig.h")

target_sources("Zyrex"
    PRIVATE
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
    target_link_libraries("InlineHook" "Zycore")
    target_link_libraries("InlineHook" "Zyrex")
    set_target_properties("InlineHook" PROPERTIES FOLDER "Examples/InlineHook")
    target_compile_definitions("InlineHook" PRIVATE "_CRT_SECURE_NO_WARNINGS")
    zyan_set_common_flags("InlineHook")
    zyan_maybe_enable_wpo("InlineHook")
endif ()

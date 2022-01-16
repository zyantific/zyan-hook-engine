cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(Zyrex VERSION 1.0.0.0 LANGUAGES C CXX)

include(GenerateExportHeader)
include(GNUInstallDirs)

if (NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
if (NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

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

# Dependencies
option(ZYAN_SYSTEM_ZYCORE
    "Use system Zycore library"
    OFF)
set(ZYAN_ZYCORE_PATH
    "${CMAKE_CURRENT_LIST_DIR}/dependencies/zycore"
    CACHE
    PATH
    "The path to look for Zydis")
option(ZYAN_SYSTEM_ZYDIS
    "Use system Zydis library"
    OFF)
set(ZYAN_ZYDIS_PATH
    "${CMAKE_CURRENT_LIST_DIR}/dependencies/zydis"
    CACHE
    PATH
    "The path to look for Zycore")

# =============================================================================================== #
# Dependencies                                                                                    #
# =============================================================================================== #

if (ZYAN_SYSTEM_ZYCORE)
    find_package(Zycore)
else ()
    # Try to initialize the Zycore submodule using Git
    if (NOT EXISTS "${ZYAN_ZYCORE_PATH}/CMakeLists.txt" AND 
        "${ZYAN_ZYCORE_PATH}" STREQUAL "${CMAKE_CURRENT_LIST_DIR}/dependencies/zycore")
        find_package(Git QUIET)
        if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive 
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
            )
        endif()
    endif ()

    if (NOT EXISTS "${ZYAN_ZYCORE_PATH}/CMakeLists.txt")
        message(
            FATAL_ERROR
            "Can't find zycore submodule. Please make sure to clone the repo recursively.\n"
            "You can fix this by running\n"
            "    git submodule update --init\n"
            "or by cloning using\n"
            "    git clone --recursive <url>\n"
            "Alternatively, you can manually clone zycore to some path and set ZYDIS_ZYCORE_PATH."
        )
    endif ()

    add_subdirectory(${ZYAN_ZYCORE_PATH} "zycore" EXCLUDE_FROM_ALL)
endif ()

if (ZYAN_SYSTEM_ZYDIS)
    find_package(Zydis)
else ()
    # Try to initialize the Zydis submodule using Git
    if (NOT EXISTS "${ZYAN_ZYDIS_PATH}/CMakeLists.txt" AND 
        "${ZYAN_ZYDIS_PATH}" STREQUAL "${CMAKE_CURRENT_LIST_DIR}/dependencies/zydis")
        find_package(Git QUIET)
        if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive 
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
            )
        endif()
    endif ()

    if (NOT EXISTS "${ZYAN_ZYDIS_PATH}/CMakeLists.txt")
        message(
            FATAL_ERROR
            "Can't find zydis submodule. Please make sure to clone the repo recursively.\n"
            "You can fix this by running\n"
            "    git submodule update --init\n"
            "or by cloning using\n"
            "    git clone --recursive <url>\n"
            "Alternatively, you can manually clone zycore to some path and set ZYDIS_ZYDIS_PATH."
        )
    endif ()

    add_subdirectory(${ZYAN_ZYDIS_PATH} "zydis" EXCLUDE_FROM_ALL)
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
option(ZYDIS_FEATURE_AVX512 "" OFF)
option(ZYDIS_FEATURE_KNC "" OFF)
option(ZYDIS_FEATURE_ENCODER "" OFF)
option(ZYDIS_BUILD_EXAMPLES "" OFF)
option(ZYDIS_BUILD_TOOLS "" OFF)

target_link_libraries("Zyrex" PUBLIC "Zycore")
target_link_libraries("Zyrex" PUBLIC "Zydis")

target_include_directories("Zyrex"
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "src")
target_compile_definitions("Zyrex" PRIVATE "_CRT_SECURE_NO_WARNINGS" "ZYREX_EXPORTS")
set_target_properties("Zyrex" PROPERTIES
    VERSION ${Zyrex_VERSION}
    SOVERSION ${Zyrex_VERSION_MAJOR}.${Zyrex_VERSION_MINOR})
zyan_set_common_flags("Zyrex")
zyan_maybe_enable_wpo_for_lib("Zyrex")
generate_export_header("Zyrex" BASE_NAME "ZYREX" EXPORT_FILE_NAME "ZyrexExportConfig.h")

target_sources("Zyrex"
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Barrier.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Status.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Transaction.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Zyrex.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Internal/InlineHook.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Internal/Relocation.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Internal/Trampoline.h"
        "${CMAKE_CURRENT_LIST_DIR}/include/Zyrex/Internal/Utils.h"
        "src/Barrier.c"
        "src/Relocation.c"
        "src/InlineHook.c"
        "src/Trampoline.c"
        "src/Transaction.c"
        "src/Utils.c"
        "src/Zyrex.c")

if (ZYREX_BUILD_SHARED_LIB AND WIN32)
    target_sources("Zyrex" PRIVATE "resources/VersionInfo.rc")
endif ()

zyan_set_source_group("Zyrex")

configure_package_config_file(cmake/zyrex-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/zyrex-config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/zyrex"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/zyrex-config-version.cmake"
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/zyrex-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/zyrex-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/zyrex"
)

install(TARGETS "Zyrex"
    EXPORT "zyrex-targets"
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(EXPORT "zyrex-targets"
    NAMESPACE "Zydis::"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/zyrex")
install(FILES
    "${PROJECT_BINARY_DIR}/ZyrexExportConfig.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY "include/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

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

    add_executable("Barrier" "examples/Barrier.c")
    target_link_libraries("Barrier" "Zycore")
    target_link_libraries("Barrier" "Zyrex")
    set_target_properties("Barrier" PROPERTIES FOLDER "Examples/Barrier")
    target_compile_definitions("Barrier" PRIVATE "_CRT_SECURE_NO_WARNINGS")
    zyan_set_common_flags("Barrier")
    zyan_maybe_enable_wpo("Barrier")
endif ()

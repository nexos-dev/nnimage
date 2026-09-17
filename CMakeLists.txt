#[[
    CMakeLists.txt - contains build system
    Copyright 2026 Jedidiah Thompson

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

         http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
]]

###################
# PROJECT SETUP   #
###################

cmake_minimum_required(VERSION 3.50)
project(nnimage VERSION 0.1.0 LANGUAGES CXX)

# Yes we enable the language in project but GNUInstallDirs complains without this
enable_language(CXX)

# Default to debug build
if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Make sure we are not on Windows
if(WIN32)
    message(FATAL_ERROR "nnimage does not support Windows")
endif()

include(GNUInstallDirs)
include(cmake/Backend.cmake)
include(source/comps/Comps.cmake)

###################
# COMPILE OPTIONS #
###################

option(NNIMAGE_ENABLE_TESTS "Enable tests" OFF)

option(NNIMAGE_ENABLE_PROFILING "Enable gprof output" OFF)

set(NNIMAGE_ENABLED_BACKENDS "krun;xorriso" CACHE STRING "Comma-separated list of enabled backends (krun, guestfs, xorriso, loopback)")

set(NNIMAGE_DEFAULT_BACKEND "krun" CACHE STRING "Default backend to use when creating images (krun, guestfs, xorriso, loopback)")

set(NNIMAGE_ALL_BACKENDS "krun;guestfs;xorriso;loopback" CACHE INTERNAL "All backends that can be enabled")

set(NNIMAGE_BACKEND_SOURCES "")
set(NNIMAGE_LIBRARIES "")
set(NNIMAGE_USABLE_BACKENDS "")

if(NNIMAGE_ENABLE_TESTS)
    enable_testing()
    include(external/cmake/doctest.cmake)
endif()

###################
# PACKAGE SETUP   #
###################

find_package(PkgConfig REQUIRED)

# Get uchardet
pkg_check_modules(UCHARDET IMPORTED_TARGET uchardet)
# If not found, that's fine
if(NOT UCHARDET_FOUND)
    set(HAVE_UCHARDET OFF)
else()
    set(HAVE_UCHARDET ON)
    list(APPEND NNIMAGE_LIBRARIES PkgConfig::UCHARDET)
endif()

# Get iconv
find_package(Iconv REQUIRED)
list(APPEND NNIMAGE_LIBRARIES Iconv::Iconv)

# Define backend configurations

# This is where CMake gets a little annoying, as we need a list of lists
# Unfortunately, CMake doesn't support lists of lists, so we have to do this in a roundabout way
# It does add an extra step if we need to add another backend, but it is the only way to do it in CMake (I think)

set(BACKEND_KRUN_CONFIG "krun;libkrun;pkg_check_modules;source/backend/Krun.cxx")
set(BACKEND_GUESTFS_CONFIG "guestfs;libguestfs;pkg_check_modules;source/backend/Guestfs.cxx")
set(BACKEND_XORRISO_CONFIG "xorriso;xorriso;find_program;source/backend/Xorriso.cxx")
set(BACKEND_LOOPBACK_CONFIG "loopback;Linux;system_check;source/backend/Loopback.cxx")

# Master backend configuration list
# NOTE: always keep synced with defined configurations
set(BACKEND_CONFIGS BACKEND_KRUN_CONFIG BACKEND_GUESTFS_CONFIG BACKEND_XORRISO_CONFIG BACKEND_LOOPBACK_CONFIG)

# Main backend loop. Goes through every enabled backend and sees if we can use it
foreach(BACKEND ${NNIMAGE_ENABLED_BACKENDS})
    set(BACKEND_FOUND OFF)

    foreach(CONFIG ${BACKEND_CONFIGS})

        # Get each part of the configuration
        set(ACTUAL_CONFIG ${${CONFIG}})
        list(GET ACTUAL_CONFIG 0 BACKEND_NAME)
        list(GET ACTUAL_CONFIG 1 BACKEND_LIB)
        list(GET ACTUAL_CONFIG 2 BACKEND_CHECK)
        list(GET ACTUAL_CONFIG 3 BACKEND_SOURCE)

        # Check if backend matches
        if(BACKEND_NAME STREQUAL BACKEND)
            set(BACKEND_FOUND ON)

            CheckBackend("${ACTUAL_CONFIG}" BACKEND_ENABLED BACKEND_LIBRARY_TARGET)

            if(BACKEND_ENABLED)

                list(APPEND NNIMAGE_USABLE_BACKENDS ${BACKEND_NAME})
                list(APPEND NNIMAGE_BACKEND_SOURCES ${BACKEND_SOURCE})

                if(BACKEND_LIBRARY_TARGET)
                    list(APPEND NNIMAGE_LIBRARIES ${BACKEND_LIBRARY_TARGET})
                endif()

            else()
                message(WARNING "Backend ${BACKEND} is not supported on this system")
            endif()

        endif()
    endforeach()

    if(NOT BACKEND_FOUND)
        message(FATAL_ERROR "Backend ${BACKEND} is not supported or not found")
    endif()
endforeach()

message(STATUS "Enabled backends: ${NNIMAGE_USABLE_BACKENDS}")

# Convert default backend to backend type
ConvertBackendNameToType(${NNIMAGE_DEFAULT_BACKEND} NNIMAGE_BACKEND_DEFAULT_ID)

#####################
# HEADER GENERATION #
#####################

# Generate backend header
set(BACKEND_TABLE_FILE "${CMAKE_CURRENT_BINARY_DIR}/BackendTable.h")
set(BACKEND_TYPES_FILE "${CMAKE_CURRENT_BINARY_DIR}/BackendTypes.h")
set(BACKENDS_FILE "${CMAKE_CURRENT_BINARY_DIR}/Backends.h")

# More CMake ugliness here so we can pass lists as a single argument
# CMake lists are so broken
# NOTE: we use commas instead of spaces because with spaces it would break each component into an argument

string(REPLACE ";" "," NNIMAGE_USABLE_BACKENDS_STR "${NNIMAGE_USABLE_BACKENDS}")
string(REPLACE ";" "," NNIMAGE_ALL_BACKENDS_STR "${NNIMAGE_ALL_BACKENDS}")

add_custom_command(
    OUTPUT "${BACKEND_TABLE_FILE}" "${BACKEND_TYPES_FILE}" "${BACKENDS_FILE}"
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_backends.sh 
            "${NNIMAGE_USABLE_BACKENDS_STR}" "${NNIMAGE_ALL_BACKENDS_STR}"
            "${BACKEND_TABLE_FILE}" "${BACKEND_TYPES_FILE}" "${BACKENDS_FILE}"
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_backends.sh
    COMMENT "Generating backend header file..."
)

add_custom_target(GenerateBackendHeader DEPENDS "${BACKEND_TABLE_FILE}" 
                    "${BACKEND_TYPES_FILE}" "${BACKENDS_FILE}")

# Generate component header
set(COMP_TABLE_FILE "${CMAKE_CURRENT_BINARY_DIR}/CompTable.h")
set(COMP_TYPES_FILE "${CMAKE_CURRENT_BINARY_DIR}/CompTypes.h")
set(COMPS_FILE "${CMAKE_CURRENT_BINARY_DIR}/Components.h")

file(GLOB COMP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/source/comps/decl/*.comp")
file(GLOB SLOT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/source/comps/slots/*.comp")

add_custom_command(
    OUTPUT "${COMP_TABLE_FILE}" "${COMP_TYPES_FILE}" "${COMPS_FILE}"
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_components.sh
            "${CMAKE_CURRENT_SOURCE_DIR}/source/comps" "${CMAKE_CURRENT_BINARY_DIR}"
            "${COMP_TABLE_FILE}" "${COMP_TYPES_FILE}" "${COMPS_FILE}"
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_components.sh ${COMP_FILES}
    COMMENT "Generating component tables..."
)

add_custom_target(GenerateComponentHeader DEPENDS "${COMP_TABLE_FILE}" "${COMP_TYPES_FILE}" 
                    "${COMPS_FILE}" "${COMP_FILES}" "${SLOT_FILES}")

# Configure config.h
configure_file(source/config.h.in config.h)

######################
# PREPARE EXECUTABLE #
######################

# Main sources
list(APPEND NNIMAGE_SOURCES source/Log.cxx
                            source/SimpleLexer.cxx
                            source/ConfParser.cxx
                            source/Dispatch.cxx
                            source/Options.cxx
                            source/Action.cxx
                            source/ErrorFormat.cxx
                            source/Backend.cxx
                            source/Frontend.cxx
                            source/Image.cxx
                            source/ImgBase.cxx
                            source/ImageCmd.cxx
                            source/ImageConf.cxx
                            source/Task.cxx)

list(APPEND NNIMAGE_EXT_SOURCES external/MemoryMapped.cpp)

add_library(libnnimage OBJECT ${NNIMAGE_SOURCES} ${NNIMAGE_COMP_SOURCES} ${NNIMAGE_EXT_SOURCES})
target_compile_features(libnnimage PUBLIC cxx_std_23)

# Ensure nnimage is dependent on the backend header file
add_dependencies(libnnimage GenerateBackendHeader GenerateComponentHeader)

add_library(libbackend OBJECT ${NNIMAGE_BACKEND_SOURCES})
target_link_libraries(libbackend PRIVATE libnnimage)

if(NNIMAGE_ENABLE_TESTS)
    add_executable(nnimage_test tests/TestDriver.cxx
                                tests/Task.cxx
                                tests/Error.cxx
                                tests/Log.cxx
                                tests/Helpers.cxx
                                tests/Image.cxx
                                tests/SimpleLexer.cxx
                                tests/ConfParser.cxx
                                tests/Options.cxx
                                tests/ImageCmd.cxx)
    target_link_libraries(nnimage_test PRIVATE libnnimage libbackend ${NNIMAGE_LIBRARIES})

    # Add doctests to CTest
    doctest_discover_tests(nnimage_test)
endif()

######################
# PROGRAM CREATION   #
######################

# Create the program
add_executable(nnimage source/main.cxx)

# Add all libraries
target_link_libraries(nnimage PRIVATE libnnimage libbackend ${NNIMAGE_LIBRARIES})

if(NNIMAGE_ENABLE_PROFILING)
    target_compile_options(nnimage PRIVATE -pg)
    target_link_options(nnimage PRIVATE -pg)
endif()

target_include_directories(libnnimage PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(libnnimage PUBLIC source)
target_include_directories(libnnimage PUBLIC external/include)
# Ugly hack because libchardet doesn't provide version.h which it depends on, so we have a fake one
# We probably shouldn't go to such lengths to support a broken library
target_include_directories(libnnimage PUBLIC cmake)

# Install it
install(TARGETS nnimage)

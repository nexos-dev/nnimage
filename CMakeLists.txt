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

cmake_minimum_required(VERSION 3.22)
project(nnimage LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(NNIMAGE_VERSION "0.1.0-dev")      # Set with every version change

# Default to debug build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Make sure we are not on Windows
if(WIN32)
    message(FATAL_ERROR "nnimage does not support Windows")
endif()

include(GNUInstallDirs)
include(cmake/Util.cmake)

# Compile options
option(NNIMAGE_ENABLE_TESTS "Enable tests" OFF)
set(NNIMAGE_ENABLED_BACKENDS "krun;xorriso" CACHE STRING "Comma-separated list of enabled backends (krun, guestfs, xorriso, loopback)")
set(NNIMAGE_DEFAULT_BACKEND "krun" CACHE STRING "Default backend to use when creating images (krun, guestfs, xorriso, loopback)")


find_package(PkgConfig REQUIRED)

# Prepare sources
set(NNIMAGE_SOURCES "")
set(NNIMAGE_INCLUDES "")
set(NNIMAGE_LIBRARIES "")
set(NNIMAGE_USABLE_BACKENDS "")

# Get libchardet
pkg_check_modules(CHARDET chardet)
# If not found, that's fine
if(NOT CHARDET_FOUND STREQUAL "1")
    set(HAVE_CHARDET OFF)
else()
    set(HAVE_CHARDET ON)
    list(APPEND NNIMAGE_INCLUDES ${CHARDET_INCLUDE_DIRS})
    list(APPEND NNIMAGE_LIBRARIES ${CHARDET_LIBRARIES})
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
set(NNIMAGE_ALL_BACKENDS "krun;guestfs;xorriso;loopback" CACHE INTERNAL "All backends that can be enabled")

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
            set(BACKEND_ENABLED OFF)
            # Now find whatever is requried of it
            if(BACKEND_CHECK STREQUAL "pkg_check_modules")
                pkg_check_modules(${BACKEND_LIB} ${BACKEND_LIB})
                if(${BACKEND_LIB}_FOUND STREQUAL "1")
                    set(BACKEND_ENABLED ON)
                    # Ensure we add the include directories and libraries to the main lists
                    list(APPEND NNIMAGE_INCLUDES ${${BACKEND_LIB}_INCLUDE_DIRS})
                    list(APPEND NNIMAGE_LIBRARIES ${${BACKEND_LIB}_LIBRARIES})
                endif()
            elseif(BACKEND_CHECK STREQUAL "find_program")
                find_program(${BACKEND_LIB} ${BACKEND_LIB})
                if(${BACKEND_LIB})
                    set(BACKEND_ENABLED ON)
                endif()
            elseif(BACKEND_CHECK STREQUAL "system_check")
                if(CMAKE_SYSTEM_NAME STREQUAL BACKEND_LIB)
                    set(BACKEND_ENABLED ON)
                endif()
            endif()
            if(BACKEND_ENABLED)
                list(APPEND NNIMAGE_USABLE_BACKENDS ${BACKEND_NAME})
                #list(APPEND NNIMAGE_SOURCES ${BACKEND_SOURCE})
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

# Generate backend header
set(BACKEND_TABLE_FILE "${CMAKE_CURRENT_BINARY_DIR}/BackendTable.h")
set(BACKEND_TYPES_FILE "${CMAKE_CURRENT_BINARY_DIR}/BackendTypes.h")
# More CMake ugliness here so we can pass lists as a single argument
# CMake lists are so broken
# NOTE: we use commas instead of spaces because with spaces it would break each component into an argument
string(REPLACE ";" "," NNIMAGE_USABLE_BACKENDS_STR "${NNIMAGE_USABLE_BACKENDS}")
string(REPLACE ";" "," NNIMAGE_ALL_BACKENDS_STR "${NNIMAGE_ALL_BACKENDS}")
add_custom_command(
    OUTPUT "${BACKEND_TABLE_FILE}" "${BACKEND_TYPES_FILE}"
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_backends.sh 
            "${NNIMAGE_USABLE_BACKENDS_STR}" "${NNIMAGE_ALL_BACKENDS_STR}"
            "${BACKEND_TABLE_FILE}" "${BACKEND_TYPES_FILE}"
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_backends.sh
    COMMENT "Generating backend header file..."
)
add_custom_target(GenerateBackendHeader DEPENDS "${BACKEND_TABLE_FILE}" "${BACKEND_TYPES_FILE}")

# Configure config.h
configure_file(source/config.h.in config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(source)
include_directories(external/include)
# Ugly hack because libchardet doesn't provide version.h which it depends on, so we have a fake one
# We probably shouldn't go to such lengths to support a broken library
include_directories(cmake)

# List sources
list(APPEND NNIMAGE_SOURCES source/main.cxx
                            source/Log.cxx
                            source/SimpleLexer.cxx
                            source/ConfParser.cxx

#                         source/Action.cxx
#                         source/Conf.cxx
#                         source/ConfParser.cxx
#                         source/Image.cxx
#                         source/Task.cxx
#                         source/backend/Krun.cxx
#                         source/backend/Backend.cxx
                         external/MemoryMapped.cpp)

#if(NNIMAGE_ENABLE_TESTS)
#    list(APPEND NNIMAGE_SOURCES tests/TestDriver.cxx)
#endif()

# Create the program
add_executable(nnimage ${NNIMAGE_SOURCES})

# Ensure nnimage is dependent on the backend header file
add_dependencies(nnimage GenerateBackendHeader)

# Add all libraries
if(NNIMAGE_INCLUDES)
    target_include_directories(nnimage PUBLIC ${NNIMAGE_INCLUDES})
endif()
if(NNIMAGE_LIBRARIES)
    target_link_libraries(nnimage PUBLIC ${NNIMAGE_LIBRARIES})
endif()
if(NNIMAGE_CXXFLAGS)
    target_compile_options(nnimage PUBLIC ${NNIMAGE_CXXFLAGS})
endif()

# Install it
install(TARGETS nnimage)

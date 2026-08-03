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

include(GNUInstallDirs)

# Compile options
set(NNIMAGE_LOGLEVEL Normal CACHE STRING "Specified verbosity of nnimage (can be Normal or Verbose)")
set(NNIMAGE_LOG_FILE "nnimage/nnimage-log" CACHE PATH "Specifies where default nnimage log goes in users .config")
option(NNIMAGE_ENABLE_GUESTFS "Enable guestfs backend" OFF)
option(NNIMAGE_ENABLE_XORRISO "Enable xorriso backend" ON)
option(NNIMAGE_ENABLE_LOOPBACK "Enable loopback backend" ON)
option(NNIMAGE_ENABLE_TESTS "Enable tests" OFF)

# Validate options
if(NOT NNIMAGE_LOGLEVEL STREQUAL "Normal" AND NOT NNIMAGE_LOGLEVEL STREQUAL "Verbose")
    message(FATAL_ERROR "Invalid log level \"${NNIMAGE_LOGLEVEL}\" specified")
endif()

if(NNIMAGE_LOGLEVEL STREQUAL "Normal")
    set(NNIMAGE_LOGLEVEL_NAME "Info")
else()
    set(NNIMAGE_LOGLEVEL_NAME ${NNIMAGE_LOGLEVEL})
endif()

find_package(PkgConfig REQUIRED)

# Get libchardet
pkg_check_modules(CHARDET chardet)
# If not found, that's fine
if(NOT CHARDET_FOUND STREQUAL "1")
    set(HAVE_CHARDET OFF)
    set(NNIMAGE_INCLUDES ${CHARDET_INCLUDE_DIRS})
    set(NNIMAGE_LIBRARIES ${CHARDET_LIBRARIES})
else()
    set(HAVE_CHARDET ON)
endif()

# Get iconv
find_package(Iconv REQUIRED)
set(NNIMAGE_LIBRARIES ${NNIMAGE_LIBRARIES} Iconv::Iconv)

# Get libkrun
pkg_check_modules(KRUN REQUIRED libkrun)

# Get libguestfs
if(NNIMAGE_ENABLE_GUESTFS)
    pkg_check_modules(GUESTFS libguestfs)
    if(NOT GUESTFS_FOUND STREQUAL "1")
        message(FATAL_ERROR "libguestfs not found but guestfs backend is enabled")
    endif()
    set(HAVE_GUESTFS ON)
endif()

if(NNIMAGE_ENABLE_XORRISO)
    set(USE_XORRISO ON)
endif()

if(NNIMAGE_ENABLE_LOOPBACK)
    set(USE_LOOPBACK ON)
endif()

# Configure the header
configure_file(source/config.h.in config.h)
include_directories(build)
include_directories(source)
include_directories(external/include)

# List sources
list(APPEND NNIMAGE_SOURCES source/main.cxx
                         source/Log.cxx
                         source/Action.cxx
                         source/Conf.cxx
                         source/ConfParser.cxx
                         source/Image.cxx
                         source/Task.cxx
                         source/backend/Krun.cxx
                         source/backend/Backend.cxx
                         external/MemoryMapped.cpp)

if(NNIMAGE_ENABLE_GUESTFS)
    list(APPEND NNIMAGE_SOURCES source/backend/Guestfs.cxx)
endif()

if(NNIMAGE_ENABLE_XORRISO)
    list(APPEND NNIMAGE_SOURCES source/backend/Xorriso.cxx)
endif()

if(NNIMAGE_ENABLE_LOOPBACK)
    list(APPEND NNIMAGE_SOURCES source/backend/Loopback.cxx)
endif()

if(NNIMAGE_ENABLE_TESTS)
    list(APPEND NNIMAGE_SOURCES source/TestDriver.cxx)
endif()

# Create the program
add_executable(nnimage ${NNIMAGE_SOURCES})

target_link_libraries(nnimage PRIVATE ${GUESTFS_LIBRARIES} ${KRUN_LIBRARIES})

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

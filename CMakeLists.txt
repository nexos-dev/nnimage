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
set(CMAKE_CXX_STANDARD 20)
set(NNIMAGE_VERSION "0.1.0-dev")      # Set with every version change

# Default to debug build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

include(GNUInstallDirs)

# Compile options
set(NNIMAGE_LOGLEVEL Error CACHE STRING "Specified verbosity of nnimage (can be Error, Warning, Info, or Verbose)")
set(NNIMAGE_LOG_FOLDER "/${CMAKE_INSTALL_LOCALSTATEDIR}/log/nnimage-log" CACHE PATH "Specifies where default nnimage log goes")

# Validate options
if(NOT NNIMAGE_LOGLEVEL STREQUAL "Error" AND NOT NNIMAGE_LOGLEVEL STREQUAL "Warning"
    AND NOT NNIMAGE_LOGLEVEL STREQUAL "Info" AND NOT NNIMAGE_LOGLEVEL STREQUAL "Verbose")
    message(FATAL_ERROR "Invalid log level \"${NNIMAGE_LOGLEVEL}\" specified")
endif()

find_package(PkgConfig REQUIRED)

# Get libchardet
pkg_check_modules(CHARDET chardet)
# If not found, that's fine
if(NOT ${CHARDET_FOUND})
    set(HAVE_CHARDET OFF)
else()
    set(HAVE_CHARDET ON)
endif()
# Get utf8proc
if(HAVE_CHARDET)
    # Find iconv now
    find_package(Iconv)
    if(Iconv_FOUND)
        set(NNIMAGE_INCLUDES ${CHARDET_INCLUDE_DIRS})
        set(NNIMAGE_LIBRARIES ${CHARDET_LIBRARIES} Iconv::Iconv)
    else()
        set(HAVE_CHARDET OFF)
    endif()
endif()
# Get libkrun
pkg_check_modules(KRUN REQUIRED libkrun)

# Configure the header
configure_file(source/config.h.in config.h)
include_directories(build)

# Create the program
add_executable(nnimage source/main.cxx 
                       source/Log.cxx
                       source/Action.cxx
                       source/Conf.cxx
                       source/ConfParser.cxx
                       source/Image.cxx
                       source/external/MemoryMapped.cpp)

# Add all libraries
if(NNIMAGE_INCLUDES)
    target_include_directories(nnimage PUBLIC ${NNIMAGE_INCLUDE})
endif()
if(NNIMAGE_LIBRARIES)
    target_link_libraries(nnimage PUBLIC ${NNIMAGE_LIBRARIES})
endif()
if(NNIMAGE_CXXFLAGS)
    target_compile_options(nnimage PUBLIC ${NNIMAGE_CXXFLAGS})
endif()

# Install it
install(TARGETS nnimage)
install(DIRECTORY DESTINATION ${NNIMAGE_LOG_FOLDER})

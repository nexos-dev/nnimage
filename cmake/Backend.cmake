#[[
    Util.cmake - contains utility functions for the build system
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

function(CheckBackend backendConfig enabledVar libraryVar)
    set(actualConfig ${backendConfig})
    list(GET actualConfig 1 backendLibrary)
    list(GET actualConfig 2 backendCheck)

    set(backendEnabled OFF)
    set(backendLibraryTarget "")

    if(backendCheck STREQUAL "pkg_check_modules")
        pkg_check_modules(${backendLibrary} IMPORTED_TARGET ${backendLibrary})

        if(${backendLibrary}_FOUND)
            set(backendEnabled ON)
            set(backendLibraryTarget "PkgConfig::${backendLibrary}")
        endif()

    elseif(backendCheck STREQUAL "find_program")
        find_program(${backendLibrary} ${backendLibrary})

        if(${backendLibrary})
            set(backendEnabled ON)
        endif()

    elseif(backendCheck STREQUAL "system_check")

        if(CMAKE_SYSTEM_NAME STREQUAL backendLibrary)
            set(backendEnabled ON)
        endif()
        
    endif()

    set(${enabledVar} ${backendEnabled} PARENT_SCOPE)
    set(${libraryVar} ${backendLibraryTarget} PARENT_SCOPE)
endfunction()

function(ConvertBackendNameToType backendName outputVar)
    # Just capitalize the first letter of the backend name and use that as the enum value
    string(SUBSTRING "${backendName}" 0 1 firstLetter)
    string(TOUPPER "${firstLetter}" firstLetterUpper)
    string(SUBSTRING "${backendName}" 1 -1 restOfName)
    string(CONCAT backendTypeName "${firstLetterUpper}${restOfName}")
    set(${outputVar} "${backendTypeName}" PARENT_SCOPE)
endfunction()

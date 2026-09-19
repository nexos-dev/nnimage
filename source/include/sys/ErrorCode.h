/*
    ErrorCode.h - all defined error codes and their string representations
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
*/

#ifndef ERRORCODE_H
#define ERRORCODE_H

#include "include/EnumArray.h"

#include <string>

#define ERROR_CODE_LIST(X)                                            \
    X (None, "No error")                                              \
    X (FileError, "File failure")                                     \
    X (PathError, "Path failure")                                     \
    X (ParseError, "Parser error")                                    \
    X (LexError, "Lexer error")                                       \
    X (LogCtrlLocked, "Log control file is locked")                   \
    X (EncMismatch, "Unable to retrieve character encoding")          \
    X (SysFailure, "Call to system failed")                           \
    X (Internal, "Internal error")                                    \
    X (BadAction, "Invalid action specified")                         \
    X (InvalidOption, "Invalid option configuration")                 \
    X (OpFailed, "Operation failed")                                  \
    X (NameMissing, "Component name missing")                         \
    X (InvalidImgType, "Invalid image type")                          \
    X (MissingRequiredProp, "Missing required property")              \
    X (InvalidImgProp, "Invalid image property")                      \
    X (BadFloppySize, "Invalid floppy disc size")                     \
    X (PropTypeMismatch, "Unexpected property type")                  \
    X (ImgInvalid, "Image validation failure")                        \
    X (InvalidPartProp, "Invalid partition property")                 \
    X (InvalidId, "Invalid indentifier")                              \
    X (BadArgument, "Bad argument format")                            \
    X (ImgMissingProp, "Required image property missing")             \
    X (PartMissingProp, "Required partition property missing")        \
    X (ComponentOverwrite, "Attempt to overwrite existing component") \
    X (PropConflict, "Conflicting properties found in registry")      \
    X (MissingPart, "Image requires at least 1 partition")            \
    X (UnusedArg, "Unused command-line argument found")               \
    X (DuplicateImage, "Duplicate image found")                       \
    X (CompNotLoaded, "Image component not loaded")

// Make clang-format shut up to prevent it from moving Max to the previous line
// clang-format off
#define ERROR_CODE_ENUM(code, message) code,
enum class ErrorCode
{
    ERROR_CODE_LIST (ERROR_CODE_ENUM)
    Max
};
#undef ERROR_CODE_ENUM

#define ERROR_CODE_STRING(code, message) {ErrorCode::code, message},
static const EnumArray<ErrorCode, std::string, ErrorCode::Max> _errorCodeStrings = {
    ERROR_CODE_LIST (ERROR_CODE_STRING)};
#undef ERROR_CODE_STRING
#undef ERROR_CODE_LIST

#endif

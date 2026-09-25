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
#include <vector>
#include <string_view>

#define ERROR_CODE_LIST(X)                                                                                   \
    X (None, "No error")                                                                                     \
    X (FileError, "File failure")                                                                            \
    X (PathError, "Path failure")                                                                            \
    X (ParseError, "{}", "message")                                                                          \
    X (LexError, "{}", "message")                                                                            \
    X (LogCtrlLocked, "Log control file is locked")                                                          \
    X (FileConvFailure, "Text encoding conversion failed for file \"{}\"", "file")                           \
    X (EncMismatch, "Unable to retrieve character encoding")                                                 \
    X (SysFailure, "{}", "error")                                                                            \
    X (LockFileOpen, "Failed to open lock file: {}", "path")                                                 \
    X (LockFileAcquire, "Failed to acquire lock on file \"{}\"", "path")                                     \
    X (TextFileOpen, "{}: {}", "file", "error")                                                              \
    X (EncodingUndetected, "unable to detect character set for file {}, assuming ASCII", "file")             \
    X (Internal, "{}", "message")                                                                            \
    X (BadAction, "invalid action name specfied")                                                            \
    X (InvalidOption, "{}", "message")                                                                       \
    X (OpFailed, "Operation aborted")                                                                        \
    X (NameMissing, "Name required for block type \"{}\"", "block_type")                                     \
    X (InvalidImgType, "Invalid image type \"{}\" specified on image{}", "type", "name_suffix")              \
    X (MissingRequiredProp, "Missing required property")                                                     \
    X (InvalidImgProp, "Unrecognized property \"{}\" specified on image{}", "prop", "name_suffix")           \
    X (BadFloppySize, "Floppy disc{} must have size 720K, 1.44M, or 2.88M", "name_suffix")                   \
    X (PropTypeMismatch, "Invalid type specified on property \"{}\"", "prop")                                \
    X (ImgInvalid, "{}", "message")                                                                          \
    X (InvalidPartProp, "Unrecognized property \"{}\" specified on partition{}", "prop", "name_suffix")      \
    X (InvalidId, "Invalid ID \"{}\" specified for property \"{}\" on image{}", "id", "prop", "name_suffix") \
    X (BadArgument, "{}", "message")                                                                         \
    X (ImgMissingProp, "Missing required property \"{}\" on image{}", "prop", "name_suffix")                 \
    X (PartMissingProp, "Missing required property \"{}\" on partition{}", "prop", "name_suffix")            \
    X (ComponentOverwrite, "Attempt to overwrite existing component")                                        \
    X (PropConflict, "Conflicting properties found in registry")                                             \
    X (MissingPart, "Image{} requires at least one partition", "name_suffix")                                \
    X (UnusedArg, "Unused command-line option \"{}\"", "option")                                             \
    X (DuplicateImage, "Image{} already exists", "name_suffix")                                              \
    X (DuplicatePartition, "Partition{} already exists", "name_suffix")                                      \
    X (CompNotLoaded, "Attempt to use unloaded component on image{}", "name_suffix")                         \
    X (ImgParseFailed, "Failed to parse image spec file")                                                    \
    X (InvalidMultiplier, "Invalid multiplier \"{}\" specified", "multiplier")                               \
    X (SizeOverflow, "Size overflow")                                                                        \
    X (UnexpectedComponentType, "Requested image component has an unexpected type")                          \
    X (DirectoryCreate, "Unable to create log directory")                                                    \
    X (UnexpectedToken, "Unexpected token \"{}\"", "token")                                                  \
    X (IntegerOutOfRange, "Integer out of range")                                                            \
    X (InternalMissingKey, "Access to non-existant log control key")                                         \
    X (ErrorReportMissing, "No such file or directory")                                                      \
    X (ErrorReportOpen, "Unable to open error reporting file \"{}\"", "file")                                \
    X (LogFileOpen, "Failed to open log file: {}", "file")                                                   \
    X (LogPathCreate, "Unable to create log directory")                                                      \
    X (LogPathNotDirectory, "Log path is not a directory")                                                   \
    X (ManagedLogOpen, "Failed to open log")                                                                 \
    X (ManagedLogControlOpen, "unable to open log control file")                                             \
    X (ExtraneousToken, "Extraneous token")                                                                  \
    X (InvalidArgumentFormat, "Specified in invalid format")                                                 \
    X (MalformedImageProperty, "Malformed image property")                                                   \
    X (MalformedPartitionSpec, "Malformed partition specification")                                          \
    X (UnableToProcessOption, "Unable to process \"{}\"", "option")                                          \
    X (ImgParseError, "{}", "message")                                                                       \
    X (ImgParseWarning, "{}", "message")                                                                     \
    X (ImgInvalidBlock, "Invalid block type \"{}\" specified", "block")                                      \
    X (UnresolvedPartition, "Reference to undefined partition \"{}\"", "part_name")                          \
    X (UnresolvedImage, "Reference to undefined image \"{}\"", "image_name")                                 \
    X (UnresolvedDeferredProp, "Unable to resolve property \"{}\" on image{}", "prop", "name_suffix")

struct ErrorEntry
{
    std::string_view str;
    std::vector<std::string_view> params;
};

// Make clang-format shut up to prevent it from moving Max to the previous line
// clang-format off
#define ERROR_CODE_ENUM(code, message, ...) code,
enum class ErrorCode
{
    ERROR_CODE_LIST (ERROR_CODE_ENUM)
    Max
};
#undef ERROR_CODE_ENUM

#define ERROR_CODE_STRING(code, message, ...) {ErrorCode::code, {message, {__VA_ARGS__}}},
static const EnumArray<ErrorCode, ErrorEntry, ErrorCode::Max> _errorCodeStrings = {
    ERROR_CODE_LIST (ERROR_CODE_STRING)};
#undef ERROR_CODE_STRING
#undef ERROR_CODE_LIST

#endif

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

enum class ErrorCode
{
    None,
    FileError,
    PathError,
    ParseError,
    LexError,
    LogCtrlLocked,
    EncMismatch,
    SysFailure,
    Internal,
    BadAction,
    InvalidOption,
    Max
};

static const EnumArray<ErrorCode, std::string, ErrorCode::Max> _errorCodeStrings = {
    {ErrorCode::None, "No error"},
    {ErrorCode::FileError, "File failure"},
    {ErrorCode::PathError, "Path failure"},
    {ErrorCode::ParseError, "Parser error"},
    {ErrorCode::LexError, "Lexer error"},
    {ErrorCode::LogCtrlLocked, "Log control file is locked"},
    {ErrorCode::EncMismatch, "Unable to retrieve character encoding"},
    {ErrorCode::SysFailure, "Call to system failed"},
    {ErrorCode::Internal, "Internal error"},
    {ErrorCode::BadAction, "Invalid action specified"},
    {ErrorCode::InvalidOption, "Invalid option configuration specified"}};

#endif

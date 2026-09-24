/*
    ImgError.h - contains image error construction helpers
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

#ifndef IMGERROR_H
#define IMGERROR_H

#include "include/Error.h"

#include <format>
#include <initializer_list>
#include <string>
#include <string_view>

class ImageError
{
  public:
    static Error Make (ErrorCode code,
        std::initializer_list<ErrorProp> props,
        ErrorLog log = ErrorLog::Normal,
        ErrorSeverity severity = ErrorSeverity::Error)
    {
        return Error ({ErrorDomain::Image, code, log, severity}, props);
    }

    static Error MakeWithContext (ErrorCode code,
        std::initializer_list<ErrorProp> props,
        std::string_view file,
        int line,
        ErrorLog log = ErrorLog::Normal,
        ErrorSeverity severity = ErrorSeverity::Error)
    {
        return Error ({ErrorDomain::Image, code, log, severity}, props)
            .AddContext ({{"file", std::string (file)}, {"line", std::to_string (line)}});
    }

    static std::string NameSuffix (std::string_view name)
    {
        if (name.empty())
            return {};
        return std::format (" {}", name);
    }

    static Error InvalidId (std::string_view prop, std::string_view name, std::string_view id)
    {
        return Make (ErrorCode::InvalidId,
            {{"prop", std::string (prop)}, {"name_suffix", NameSuffix (name)}, {"id", std::string (id)}});
    }
    static Error Invalid (std::string_view message, ErrorLog log = ErrorLog::Normal)
    {
        return Make (ErrorCode::ImgInvalid, {{"message", std::string (message)}}, log);
    }
};

#endif

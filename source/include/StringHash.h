/*
    StringHash.h - contains string hash helper
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

#ifndef STRINGHASH_H
#define STRINGHASH_H

#include <functional>
#include <string>
#include <string_view>

// Custom hash to avoid turning a string_view into a string when using NameRegistry
struct StringHash
{
    using is_transparent = void;

    size_t operator() (std::string_view value) const noexcept
    {
        return std::hash<std::string_view>{}(value);
    }
    size_t operator() (const std::string& value) const noexcept
    {
        return operator() (std::string_view (value));
    }
    size_t operator() (const char* value) const noexcept
    {
        return operator() (std::string_view (value));
    }
};

#endif

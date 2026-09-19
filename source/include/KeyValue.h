/*
    KeyValue.h - contains a basic key-value splitter
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

#ifndef KEYVALUE_H
#define KEYVALUE_H

#include "include/Error.h"

#include <string_view>
#include <string>
#include <optional>
#include <vector>
#include <ranges>

class KeyVal
{
  public:
    // Array of key=value pairs if they follow said format
    static std::optional<std::vector<std::pair<std::string_view, std::string_view>>> Parse (std::string_view text)
    {
        if (text.empty())
            return std::nullopt;

        std::vector<std::pair<std::string_view, std::string_view>> values;
        for (auto pair : text | std::views::split (',') |
                             std::views::transform ([] (auto&& subRange) { return std::string_view (subRange); }))
        {
            // Reject empty entries caused by consecutive commas or a trailing comma.
            if (pair.empty())
                return std::nullopt;

            auto eqPos = pair.find ('=');
            // Reject no =, leading =, and trailing =
            if (eqPos == std::string_view::npos || eqPos == 0 || eqPos == pair.length() - 1)
                return std::nullopt;

            std::string_view key = pair.substr (0, eqPos);
            std::string_view value = pair.substr (eqPos + 1);

            // Ensure there are no extra equals signs
            if (value.find ('=') != std::string_view::npos)
                return std::nullopt;

            values.push_back ({key, value});
        }
        return values;
    }
};

#endif

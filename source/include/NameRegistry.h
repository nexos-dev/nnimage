/*
    NameRegistry.h - contains name-to-type registry definition
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

#ifndef NAMEREGISTRY_H
#define NAMEREGISTRY_H

#include "include/StringHash.h"

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <string>
#include <unordered_map>

// Generic name-to-enum registry
template <typename Value>
class NameRegistry
{
  public:
    NameRegistry (std::initializer_list<std::pair<const std::string, Value>> values) : values{values}
    {}

    template <size_t N>
    NameRegistry (const std::array<std::pair<std::string_view, Value>, N>& entries)
    {
        for (const auto& [name, value] : entries)
            values.emplace (name, value);
    }

    Value Resolve (std::string_view name) const
        requires std::is_enum_v<Value>
    {
        auto it = values.find (name);
        if (it == values.end())
            return Value::Max;
        return it->second;
    }

    const Value* Find (std::string_view name) const
        requires std::is_class_v<Value>
    {
        auto it = values.find (name);
        if (it == values.end())
            return nullptr;
        return &it->second;
    }

    const std::string& GetName (Value value) const
    {
        auto it =
            std::find_if (values.begin(), values.end(), [value] (const auto& pair) { return pair.second == value; });
        if (it == values.end())
            throw std::out_of_range ("NameRegistry value is out of range");
        return it->first;
    }

  private:
    std::unordered_map<std::string, Value, StringHash, std::equal_to<>> values;
};

#endif

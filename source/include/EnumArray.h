/*
    EnumArray.h - A simple array class that uses an enum as the index type.
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

#ifndef ENUMARRAY_H
#define ENUMARRAY_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T>
concept EnumType = std::is_enum_v<T>;

template <EnumType Enum, typename ValueType, Enum Size>
class EnumArray
{
  public:
    EnumArray()
    {
        static_assert (static_cast<size_t> (Size) > 0, "Size must be greater than 0");
    }
    constexpr EnumArray (std::initializer_list<ValueType> vals)
    {
        if (vals.size() > data.size())
            throw std::length_error ("Too many values for EnumArray");
        std::copy (vals.begin(), vals.end(), data.begin());
    }
    constexpr EnumArray (std::initializer_list<std::pair<Enum, ValueType>> vals)
    {
        static_assert (static_cast<size_t> (Size) > 0, "Size must be greater than 0");
        for (const auto& val : vals)
        {
            data.at (checkedIndex (val.first)) = val.second;
        }
    }

    constexpr ValueType& operator[] (Enum index)
    {
        return data.at (checkedIndex (index));
    }

    constexpr const ValueType& operator[] (Enum index) const
    {
        return data.at (checkedIndex (index));
    }
    constexpr size_t size() const noexcept
    {
        return static_cast<size_t> (Size);
    }
    constexpr auto begin() noexcept
    {
        return data.begin();
    }
    constexpr auto end() noexcept
    {
        return data.end();
    }

    using EnumArrayIter = std::array<ValueType, static_cast<size_t> (Size)>::iterator;
    constexpr Enum index (EnumArrayIter it) const
    {
        size_t position = 0;
        for (auto current = data.begin(); current != data.end(); ++current, ++position)
        {
            if (current == it)
                return static_cast<Enum> (position);
        }
        throw std::out_of_range ("EnumArray iterator is out of range");
    }

  private:
    static constexpr size_t checkedIndex (Enum index)
    {
        if (static_cast<size_t> (index) >= static_cast<size_t> (Size))
            throw std::out_of_range ("EnumArray index is out of range");
        return static_cast<size_t> (index);
    }

    std::array<ValueType, static_cast<size_t> (Size)> data{};
};

#endif

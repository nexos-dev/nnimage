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
#include <type_traits>
#include <utility>

template <typename EnumType, typename ValueType, EnumType Size>
class EnumArray
{
  public:
    EnumArray()
    {
        static_assert (std::is_enum<EnumType>::value, "EnumType must be an enum type");
        static_assert (static_cast<size_t> (Size) > 0, "Size must be greater than 0");
    }
    constexpr EnumArray (std::initializer_list<ValueType> vals)
    {
        static_assert (std::is_enum<EnumType>::value, "EnumType must be an enum type");
        static_assert (static_cast<size_t> (Size) > 0, "Size must be greater than 0");
        std::copy (vals.begin(), vals.end(), data.begin());
    }
    constexpr EnumArray (std::initializer_list<std::pair<EnumType, ValueType>> vals)
    {
        static_assert (std::is_enum<EnumType>::value, "EnumType must be an enum type");
        static_assert (static_cast<size_t> (Size) > 0, "Size must be greater than 0");
        for (const auto& val : vals)
        {
            data[static_cast<size_t> (val.first)] = val.second;
        }
    }

    constexpr ValueType& operator[] (EnumType index)
    {
        return data[static_cast<size_t> (index)];
    }

    constexpr const ValueType& operator[] (EnumType index) const
    {
        return data[static_cast<size_t> (index)];
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
    constexpr EnumType index (EnumArrayIter it) noexcept
    {
        return static_cast<EnumType> (it - data.begin());
    }

  private:
    std::array<ValueType, static_cast<size_t> (Size)> data{};
};

#endif

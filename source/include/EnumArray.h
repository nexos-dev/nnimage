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

#include <array>
#include <cstdlib>
#include <initializer_list>
#include <type_traits>

template <typename EnumType, typename ValueType, size_t Size = static_cast<size_t> (EnumType::Max)>
class EnumArray
{
  public:
    EnumArray()
    {
        static_assert (std::is_enum<EnumType>::value, "EnumType must be an enum type");
        static_assert (Size > 0, "Size must be greater than 0");
        static_assert (Size <= static_cast<size_t> (EnumType::Max),
                       "Size must be less than or equal to EnumType::Max");
    }
    constexpr EnumArray (std::initializer_list<ValueType> vals)
    {
        static_assert (std::is_enum<EnumType>::value, "EnumType must be an enum type");
        static_assert (Size > 0, "Size must be greater than 0");
        static_assert (Size <= static_cast<size_t> (EnumType::Max),
                       "Size must be less than or equal to EnumType::Max");
        std::copy (vals.begin(), vals.end(), data.begin());
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
        return Size;
    }
    constexpr auto begin() noexcept
    {
        return data.begin();
    }
    constexpr auto end() noexcept
    {
        return data.end();
    }

  private:
    std::array<ValueType, Size> data{};
};

#endif
